#ifndef CIV_SERIAL_H
#define CIV_SERIAL_H

#include <async_comm/serial.h>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <deque>
#include <sys/types.h>
#include <vector>

namespace civ 
{
    using namespace std;
    using namespace std::chrono;
    const uint8_t JAMMER_SEQUENCE[5] = { 0xfc, 0xfc, 0xfc, 0xfc, 0xfc};
    const int MAX_SYMBOL_DELAY = 5;   // Max number of symbols to delay for when jammed
    const int SEND_TIMEOUT_US = 100000;

    // Abstracted serial access (for Linux/Windows using async_comm library)
    class CivSerial
    {
        private:
        std::shared_ptr<async_comm::Serial> serialPort;
        deque<uint8_t> serialBuffer;
        deque<uint8_t> sentBytes;
        deque<ulong> sentJammers;
        deque<ulong> receivedJammers;
        string portName;
        bool isRadio;
        bool timeoutError;
        int guardTimeus;
        int symbolTimeus;
        ulong sentOdometer;
        ulong sentConfirmedOdometer;
        void init(string portName, int baudRate, bool isRadio);
        void civCallback(const uint8_t* buf, size_t len);
        time_point<steady_clock, duration<long, ratio<1, 1000000000>>> lastRx;

        public:
        CivSerial(string portName, int baudRate, bool isRadio);
        ~CivSerial();
        int WaitingBytes() const;
        bool HasAvailableBytes() const;
        bool JammerSentFlag() const;
        bool JammerReceivedFlag() const;
        ulong ConsumeSentJammer();
        ulong ConsumeReceivedJammer();
        bool TimeoutErrorFlag();
        uint8_t PopByte();
        uint8_t PeekByte() const;
        bool CanSend() const;
        void PopBytes(std::vector<uint8_t>& buffer);
        void PopBytes(std::deque<uint8_t>& buffer);
        bool SendByte(const uint8_t value);
        bool SendBytes(const uint8_t* buffer, const size_t len);
        bool SendBytes(std::vector<uint8_t>& buffer);
        void FlushBuffers();
        void WaitJammer() const;
        void WaitGuard() const;
        bool WaitSend();
        ulong GetSentCount() const;
        ulong GetConfirmedSentCount() const;
    };
}

#endif // CIV_SERIAL_H