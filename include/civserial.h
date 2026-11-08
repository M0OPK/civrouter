#ifndef CIV_SERIAL_H
#define CIV_SERIAL_H

#include <async_comm/serial.h>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <deque>
#include <memory>
#include <mutex>
#include <string>
#include <sys/types.h>
#include <vector>

namespace civ
{
    using namespace std;
    using namespace std::chrono;
    const uint8_t JAMMER_SEQUENCE[5] = { 0xfc, 0xfc, 0xfc, 0xfc, 0xfc};
    const uint8_t CIV_SERIAL_JAMMER = 0xfc;
    const int SEND_TIMEOUT_US = 100000;   // Base time to wait for our echo before giving up on a frame
    const int MAX_SEND_RETRIES = 5;       // Attempts per frame before it is dropped
    const int REPLY_WAIT_US = 50000;      // Max time to leave the bus free for a radio's reply

    // Abstracted serial access (for Linux/Windows using async_comm library)
    //
    // Radio ports (a real, shared CI-V bus) send each frame in two parts:
    //   1. Everything except the final 0xFD is written at full speed.
    //   2. Once every one of those bytes has echoed back correctly, the 0xFD is written.
    // A radio only acts on a command when it sees the 0xFD, so if any echoed byte
    // doesn't match (a collision) we send the jammer instead and the radio never
    // receives a complete, corrupt command. The frame is then retried after a
    // random back-off. Only one frame is on the bus at a time, and after a
    // frame addressed to a radio we leave the bus free until its reply has
    // arrived (or REPLY_WAIT_US passes), so we don't talk over the answer.
    //
    // Echo checking happens in civCallback, which runs on async_comm's callback
    // thread, so the jammer goes out as soon as the bad byte arrives. Everything
    // else (starting frames, retries, timeouts) happens in Service(), called from
    // the main loop. All shared state is protected by stateGuard.
    //
    // Controller ports (virtual/USB links to PC software) have no shared bus:
    // frames are written straight out and received bytes are echoed back to
    // simulate being on a CI-V bus.
    class CivSerial
    {
        private:
        enum class TxState
        {
            Idle,           // Nothing in progress
            AwaitBodyEcho,  // Frame body written, waiting for it to echo back
            AwaitEndEcho,   // 0xFD written, waiting for its echo
            AwaitReply,     // Frame sent, leaving the bus free for the radio to answer
            Backoff         // Collision seen, waiting to retry
        };

        std::shared_ptr<async_comm::Serial> serialPort;
        string portName;
        bool isRadio;
        int guardTimeus;
        int symbolTimeus;

        mutable mutex stateGuard;

        // Receive side
        deque<uint8_t> serialBuffer;
        steady_clock::time_point lastRx;

        // Transmit side (radio ports)
        deque<vector<uint8_t>> txQueue;     // Frames waiting to be sent
        vector<uint8_t> txFrame;            // Frame currently on the bus
        size_t txWritten;                   // Bytes of txFrame handed to the port
        size_t txConfirmed;                 // Bytes of txFrame seen echoed back
        TxState txState;
        int txAttempts;
        int jammerEchoToSwallow;            // Our own jammer bytes still to come back
        bool replySeen;                     // A complete frame (0xFD) arrived during AwaitReply
        steady_clock::time_point txDeadline;   // Echo timeout, or end of back-off

        // Counters for diagnostics
        ulong collisions;
        ulong droppedFrames;

        bool debugLog;                      // Set CIVROUTER_DEBUG=1 to log raw TX/RX bytes
        steady_clock::time_point startTime;
        void logBytes(const char* tag, const uint8_t* buf, size_t len) const;
        void sendLocked(const char* tag, const uint8_t* buf, size_t len);

        void init(string portName, int baudRate, bool isRadio);
        void civCallback(const uint8_t* buf, size_t len);
        void handleRadioByte(uint8_t value, steady_clock::time_point now);
        void startFrameLocked(steady_clock::time_point now);
        void collisionLocked(bool sendJammer, steady_clock::time_point now);
        void finishFrameLocked(bool sent, steady_clock::time_point now);
        bool busIdleLocked(steady_clock::time_point now) const;

        public:
        CivSerial(string portName, int baudRate, bool isRadio);
        ~CivSerial();
        CivSerial(const CivSerial&) = delete;
        CivSerial& operator=(const CivSerial&) = delete;

        // Call regularly from the main loop. Starts queued frames, handles
        // retries and echo timeouts. Never blocks.
        void Service();

        // Queue a complete serialised frame (FE FE .. FD) for sending.
        bool SendFrame(const vector<uint8_t>& frame);

        bool HasAvailableBytes() const;
        int WaitingBytes() const;
        void PopBytes(std::vector<uint8_t>& buffer);
        void PopBytes(std::deque<uint8_t>& buffer);

        bool IsSending() const;     // A frame is queued or in progress
        ulong CollisionCount() const;
        ulong DroppedFrameCount() const;
    };
}

#endif // CIV_SERIAL_H
