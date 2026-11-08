#include <chrono>
#include <civserial.h>
#include <cstdint>
#include <deque>
#include <functional>
#include <iostream>
#include <ostream>
#include <sys/types.h>
#include <thread>

using namespace std;
using namespace std::chrono;
using namespace civ;

void CivSerial::init(string portName, int baudRate, bool isRadio)
{
    this->isRadio = isRadio;
    this->portName = portName;
    serialPort = make_shared<async_comm::Serial>(portName, baudRate);
    serialPort->register_receive_callback(std::bind(&CivSerial::civCallback, this, placeholders::_1, placeholders::_2));
    if (!serialPort->init())
    {
        cout << "Failed to initialize serial port on " << portName << " with rate " << baudRate << endl;
    }
    lastRx = steady_clock::now();
    // Time to receive two bytes at the current baud rate (9 bits, 8 data, 1 stop) in microseconds
    symbolTimeus = (9000000/baudRate);
    guardTimeus = symbolTimeus * 2;
    sentConfirmedOdometer = 0;
    sentOdometer = 0;

    // Wait for the guard time to make sure we're good to send
    this_thread::sleep_for(microseconds(guardTimeus + 1));
}

CivSerial::CivSerial(string portName, int baudRate, bool isRadio)
{
    init(portName, baudRate, isRadio);
}

CivSerial::~CivSerial()
{
    serialBuffer.clear();
    if (serialPort != nullptr)
    {
        serialPort->close();
        serialPort = nullptr;
    }
}

// We could handle this data dynamically. But since we're trying to make this compatible with arduino, we'll do it this way
void CivSerial::civCallback(const uint8_t* buf, size_t len)
{
    //cout << "Callback received: ";

    // If this isn't a radio (actually on the civ bus)
    // Then send the data back to the port to simulate
    // being on the civ bus
    if (!isRadio)
        SendBytes(buf, len);
    
    // Load buffer with data
    //cout << "Received data on " << portName;
    for(auto bufferPos = 0; bufferPos < len; bufferPos++)
    {
        // If uncleared jammer then just exit until resynced
        if (JammerReceivedFlag() || JammerSentFlag())
            return;

        //cout << CivUtils::CharToHex(buf[bufferPos]) << " ";
        // Raise jammer received flag if we see a jammer on here
        if (buf[bufferPos] == 0xfc)
        {
            receivedJammers.push_back(sentOdometer - len + bufferPos);
        }

        // Check if there is a sent buffer, if so we expect to see our data back!
        if (isRadio && !sentBytes.empty())
        {
            // If the byte isn't what we expect, 
            // send a jammer to the bus 
            // and raise the flag for jammer sent
            if (sentBytes[0] != buf[bufferPos])
            {
                sentJammers.push_back(sentOdometer - sentBytes.size());
                serialPort->send_bytes(JAMMER_SEQUENCE, 5);
                break;
            }
            
            // Consume the buffer entry
            sentBytes.pop_front();
            if (sentOdometer > sentBytes.size())
                sentConfirmedOdometer = sentOdometer - sentBytes.size();
            else
                sentConfirmedOdometer = 0;
        }
        else
        {
            //cout << " " << CivUtils::Uint8ToHex(buf[bufferPos]);
            // Otherwise push the byte 
            serialBuffer.push_back(buf[bufferPos]);

            // Set last received time
            // We don't want to count bytes we sent
            lastRx = steady_clock::now();
            sentConfirmedOdometer = sentOdometer;
        }
    }
    //cout << endl;
    //cout << endl;
}

int CivSerial::WaitingBytes() const
{
    return serialBuffer.size();
}

bool CivSerial::HasAvailableBytes() const
{
    return serialBuffer.size() > 0;
}

bool CivSerial::JammerSentFlag() const
{
    return !sentJammers.empty();
}

bool CivSerial::JammerReceivedFlag() const
{
    return !receivedJammers.empty();
}

ulong CivSerial::ConsumeReceivedJammer()
{
    ulong result = receivedJammers[0];
    receivedJammers.pop_front();
    return result;
}

ulong CivSerial::ConsumeSentJammer()
{
    ulong result = sentJammers[0];
    sentJammers.pop_front();
    return result;
}

bool CivSerial::TimeoutErrorFlag()
{
    if (timeoutError)
    {
        timeoutError = false;
        return true;
    }
    return false;
}

uint8_t CivSerial::PopByte()
{
    if (!serialBuffer.empty())
    {
        uint8_t result = PeekByte();
        serialBuffer.pop_front();
    }
    return 0;
}

uint8_t CivSerial::PeekByte() const
{
    if (!serialBuffer.empty())
    {
        return serialBuffer[0];
    }
    return 0;
}

void CivSerial::PopBytes(std::vector<uint8_t>& buffer)
{
    while (!serialBuffer.empty())
    {
        buffer.push_back(serialBuffer[0]);
        serialBuffer.pop_front();
    }
}

void CivSerial::PopBytes(std::deque<uint8_t>& buffer)
{
    while (!serialBuffer.empty())
    {
        buffer.push_back(serialBuffer[0]);
        serialBuffer.pop_front();
    }
}

bool CivSerial::CanSend() const
{
    return (duration_cast<microseconds>(steady_clock::now() - lastRx).count() > guardTimeus);
}

bool CivSerial::SendByte(const uint8_t value)
{
    if (!CanSend())
        return false;

    sentBytes.push_back(value);
    serialPort->send_byte(value);
    sentOdometer++;

    return true;
}

bool CivSerial::SendBytes(const uint8_t* buffer, const size_t len)
{
    if (!CanSend())
        return false;

    //cout << "Sent data on " << portName;
    for(int pos = 0; pos < len; pos++)
    {
        sentBytes.push_back(buffer[pos]);
        //cout << " " << CivUtils::Uint8ToHex(buffer[pos]);
    }
    //cout << endl;
    serialPort->send_bytes(buffer, len);
    sentOdometer += len;
    //if (JammerSentFlag() || JammerReceivedFlag())
    //    return false;

    return true;
}

bool CivSerial::SendBytes(std::vector<uint8_t>& buffer)
{
    if (buffer.empty())
        return false;

    return SendBytes(&buffer[0], buffer.size());
}

void CivSerial::FlushBuffers()
{
    // Reset both buffers to regain sync
    serialBuffer.clear();
    sentBytes.clear();
    receivedJammers.clear();
    sentJammers.clear();
}

void CivSerial::WaitJammer() const
{
    // Pick a random time in microseconds to wait, 
    // between 10ms (10000us) and 50ms (50000us)
    // Then add the time it would take to send or receive the 5 jammer codes
    // Then wait that long
    const int delayTimeus = 10000 + (rand() % 40000) + (5 * symbolTimeus);
    this_thread::sleep_for(microseconds(delayTimeus));
}

void CivSerial::WaitGuard() const
{
    this_thread::sleep_for(microseconds(guardTimeus));
}

bool CivSerial::WaitSend()
{
    auto startTime = steady_clock::now();
    int waitTime = SEND_TIMEOUT_US + (guardTimeus * sentBytes.size());
    // Wait until buffer is clear, or until jammer received/sent
    while (!JammerReceivedFlag() && !JammerSentFlag() && !sentBytes.empty())
    {
        this_thread::sleep_for(microseconds(symbolTimeus));
        if (duration_cast<microseconds>(steady_clock::now() - startTime).count() > waitTime)
        {
            timeoutError = true;
            return false;
        }
    }
    if (JammerReceivedFlag() || JammerSentFlag())
        return false;

    return sentBytes.empty();
}

ulong CivSerial::GetSentCount() const
{
    return sentOdometer;
}

ulong CivSerial::GetConfirmedSentCount() const
{
    return sentOdometer - sentBytes.size();
}
