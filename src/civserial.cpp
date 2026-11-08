#include <chrono>
#include <civserial.h>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <deque>
#include <functional>
#include <iostream>
#include <mutex>
#include <ostream>
#include <sys/types.h>
#include <thread>

using namespace std;
using namespace std::chrono;
using namespace civ;

namespace
{
    const size_t MAX_TX_QUEUE = 64;   // Frames; oldest are dropped beyond this
}

void CivSerial::init(string portName, int baudRate, bool isRadio)
{
    // Set up all state before the port opens, as callbacks can start straight away
    this->isRadio = isRadio;
    this->portName = portName;
    // Time to send one byte at the current baud rate (8 data, 1 stop, ~9 bits) in microseconds
    symbolTimeus = (9000000/baudRate);
    guardTimeus = symbolTimeus * 2;
    lastRx = steady_clock::now();
    txWritten = 0;
    txConfirmed = 0;
    txState = TxState::Idle;
    txAttempts = 0;
    jammerEchoToSwallow = 0;
    replySeen = false;
    txDeadline = lastRx;
    collisions = 0;
    droppedFrames = 0;
    // Shared by all ports so log timestamps line up across ports
    static const steady_clock::time_point processStart = steady_clock::now();
    startTime = processStart;
    debugLog = (getenv("CIVROUTER_DEBUG") != nullptr);

    serialPort = make_shared<async_comm::Serial>(portName, baudRate);
    serialPort->register_receive_callback(std::bind(&CivSerial::civCallback, this, placeholders::_1, placeholders::_2));
    if (!serialPort->init())
    {
        cout << "Failed to initialize serial port on " << portName << " with rate " << baudRate << endl;
    }
}

CivSerial::CivSerial(string portName, int baudRate, bool isRadio)
{
    init(portName, baudRate, isRadio);
}

// Debug: one line per chunk, time in ms since the port opened
void CivSerial::logBytes(const char* tag, const uint8_t* buf, size_t len) const
{
    if (!debugLog)
        return;
    char text[16];
    double ms = duration_cast<microseconds>(steady_clock::now() - startTime).count() / 1000.0;
    snprintf(text, sizeof(text), "%10.3f", ms);
    cout << text << " " << portName << " " << tag << ":";
    for (size_t pos = 0; pos < len; pos++)
    {
        snprintf(text, sizeof(text), " %02X", buf[pos]);
        cout << text;
    }
    cout << endl;
}

// stateGuard must be held
void CivSerial::sendLocked(const char* tag, const uint8_t* buf, size_t len)
{
    logBytes(tag, buf, len);
    serialPort->send_bytes(buf, len);
}

CivSerial::~CivSerial()
{
    // Close first: this joins async_comm's callback thread, so nothing
    // touches our buffers after this point
    if (serialPort != nullptr)
    {
        serialPort->close();
        serialPort = nullptr;
    }
    serialBuffer.clear();
    txQueue.clear();
}

// Runs on async_comm's callback thread
void CivSerial::civCallback(const uint8_t* buf, size_t len)
{
    auto now = steady_clock::now();
    lock_guard<mutex> lock(stateGuard);
    logBytes("RX", buf, len);

    if (!isRadio)
    {
        // Not on a real CI-V bus: echo the data back to simulate one
        sendLocked("TX echo", buf, len);
        for (size_t pos = 0; pos < len; pos++)
            serialBuffer.push_back(buf[pos]);
        lastRx = now;
        return;
    }

    for (size_t pos = 0; pos < len; pos++)
        handleRadioByte(buf[pos], now);
}

// stateGuard must be held
void CivSerial::handleRadioByte(uint8_t value, steady_clock::time_point now)
{
    lastRx = now;

    // Our own jammer coming back off the bus
    if (jammerEchoToSwallow > 0 && value == CIV_SERIAL_JAMMER)
    {
        jammerEchoToSwallow--;
        return;
    }

    if (txState == TxState::AwaitBodyEcho || txState == TxState::AwaitEndEcho)
    {
        if (txConfirmed < txWritten && value == txFrame[txConfirmed])
        {
            txConfirmed++;

            // Whole body confirmed: now let the radio have the 0xFD
            if (txState == TxState::AwaitBodyEcho && txConfirmed == txFrame.size() - 1)
            {
                txWritten = txFrame.size();
                txState = TxState::AwaitEndEcho;
                txDeadline = now + microseconds(SEND_TIMEOUT_US);
                sendLocked("TX end", &txFrame.back(), 1);
            }
            else if (txState == TxState::AwaitEndEcho && txConfirmed == txFrame.size())
            {
                finishFrameLocked(true, now);
            }
            return;
        }

        // Echo didn't match (or something arrived we didn't send): collision.
        // If what we saw was someone else's jammer, there's no need to add ours.
        collisionLocked(value != CIV_SERIAL_JAMMER, now);
        return;
    }

    // Not transmitting: this is incoming data. Stray jammer bytes are dropped
    // (the frame they interrupted is discarded by the frame parser).
    if (value == CIV_SERIAL_JAMMER)
        return;

    if (txState == TxState::AwaitReply && value == 0xfd)
        replySeen = true;

    serialBuffer.push_back(value);
}

// stateGuard must be held
void CivSerial::collisionLocked(bool sendJammer, steady_clock::time_point now)
{
    collisions++;
    if (sendJammer)
    {
        sendLocked("TX jammer", JAMMER_SEQUENCE, sizeof(JAMMER_SEQUENCE));
        jammerEchoToSwallow += sizeof(JAMMER_SEQUENCE);
    }

    txAttempts++;
    if (txAttempts >= MAX_SEND_RETRIES)
    {
        cout << portName << ": dropping frame after " << txAttempts << " failed attempts" << endl;
        droppedFrames++;
        finishFrameLocked(false, now);
        return;
    }

    // Random back-off between 10ms and 50ms, plus time for the jammer to go out
    const int delayTimeus = 10000 + (rand() % 40000) + ((int)sizeof(JAMMER_SEQUENCE) * symbolTimeus);
    txDeadline = now + microseconds(delayTimeus);
    txState = TxState::Backoff;
}

// stateGuard must be held
void CivSerial::finishFrameLocked(bool sent, steady_clock::time_point now)
{
    // A frame to a specific address (byte 2, not broadcast 0x00) gets an answer:
    // leave the bus free for it before starting the next frame
    bool expectReply = sent && txFrame.size() > 2 && txFrame[2] != 0x00;

    txFrame.clear();
    txWritten = 0;
    txConfirmed = 0;
    txAttempts = 0;
    if (expectReply)
    {
        replySeen = false;
        txDeadline = now + microseconds(REPLY_WAIT_US);
        txState = TxState::AwaitReply;
    }
    else
    {
        txState = TxState::Idle;
    }
}

// stateGuard must be held. txFrame must hold the frame to (re)send.
void CivSerial::startFrameLocked(steady_clock::time_point now)
{
    // Any jammer echo still outstanding is long gone by now
    jammerEchoToSwallow = 0;
    txConfirmed = 0;
    txWritten = txFrame.size() - 1;   // Everything except the 0xFD
    txState = TxState::AwaitBodyEcho;
    txDeadline = now + microseconds(SEND_TIMEOUT_US + (int)txFrame.size() * symbolTimeus);
    sendLocked(txAttempts == 0 ? "TX body" : "TX retry body", txFrame.data(), txWritten);
}

// stateGuard must be held
bool CivSerial::busIdleLocked(steady_clock::time_point now) const
{
    return duration_cast<microseconds>(now - lastRx).count() > guardTimeus;
}

// Runs on the main thread
void CivSerial::Service()
{
    if (!isRadio)
        return;

    auto now = steady_clock::now();
    lock_guard<mutex> lock(stateGuard);

    // Reply arrived (and the bus has gone quiet) or we've waited long enough
    if (txState == TxState::AwaitReply && ((replySeen && busIdleLocked(now)) || now >= txDeadline))
        txState = TxState::Idle;

    switch (txState)
    {
        case TxState::AwaitReply:
            break;

        case TxState::Idle:
            if (!txQueue.empty() && busIdleLocked(now))
            {
                txFrame = std::move(txQueue.front());
                txQueue.pop_front();
                txAttempts = 0;
                startFrameLocked(now);
            }
            break;

        case TxState::Backoff:
            if (now >= txDeadline && busIdleLocked(now))
                startFrameLocked(now);
            break;

        case TxState::AwaitBodyEcho:
        case TxState::AwaitEndEcho:
            if (now >= txDeadline)
            {
                cout << portName << ": timed out waiting for echo (" << txConfirmed << "/" << txWritten << " bytes)" << endl;
                // If the 0xFD hasn't gone out, the radio never got a complete frame,
                // so no jammer is needed. If it has, jam in case it's half received.
                collisionLocked(txState == TxState::AwaitEndEcho, now);
            }
            break;
    }
}

// Runs on the main thread
bool CivSerial::SendFrame(const vector<uint8_t>& frame)
{
    if (frame.size() < 2)
        return false;

    lock_guard<mutex> lock(stateGuard);

    if (!isRadio)
    {
        sendLocked("TX", frame.data(), frame.size());
        return true;
    }

    if (txQueue.size() >= MAX_TX_QUEUE)
    {
        cout << portName << ": send queue full, dropping oldest frame" << endl;
        txQueue.pop_front();
        droppedFrames++;
    }
    txQueue.push_back(frame);
    return true;
}

bool CivSerial::HasAvailableBytes() const
{
    lock_guard<mutex> lock(stateGuard);
    return !serialBuffer.empty();
}

int CivSerial::WaitingBytes() const
{
    lock_guard<mutex> lock(stateGuard);
    return serialBuffer.size();
}

void CivSerial::PopBytes(std::vector<uint8_t>& buffer)
{
    lock_guard<mutex> lock(stateGuard);
    buffer.insert(buffer.end(), serialBuffer.begin(), serialBuffer.end());
    serialBuffer.clear();
}

void CivSerial::PopBytes(std::deque<uint8_t>& buffer)
{
    lock_guard<mutex> lock(stateGuard);
    buffer.insert(buffer.end(), serialBuffer.begin(), serialBuffer.end());
    serialBuffer.clear();
}

bool CivSerial::IsSending() const
{
    lock_guard<mutex> lock(stateGuard);
    return txState != TxState::Idle || !txQueue.empty();
}

ulong CivSerial::CollisionCount() const
{
    lock_guard<mutex> lock(stateGuard);
    return collisions;
}

ulong CivSerial::DroppedFrameCount() const
{
    lock_guard<mutex> lock(stateGuard);
    return droppedFrames;
}
