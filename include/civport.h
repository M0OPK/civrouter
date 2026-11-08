#ifndef CIV_PORT_H
#define CIV_PORT_H
#include "civconfig.h"
#include "civframe.h"
#include <civserial.h>
#include <cstdint>
#include <deque>
#include <utility>

namespace civ 
{
    using namespace std;
    class CivPort
    {
        private:
            CivSerial serialPort;
            const CivSerialPortInfo* serialConfig;
            bool radioPort;
            deque<CivFrame> frameBuffer;
            deque<uint8_t> serialBuffer;
            CivFrame lastFrame;
            deque<pair<ulong, CivFrame>> frameInProgressBuffer;
            void init(const CivSerialPortInfo* portInfo);

        public:
            CivPort(const CivSerialPortInfo* portInfo);
            bool Update();
            bool FrameReady() const;
            int FrameBufferSize() const;
            CivFrame PopFrame();
            bool SendFrame(CivFrame& frame);
            const bool IsRadioPort() const;
            const bool PortAcceptsInfo() const;
            const bool PortSendsInfo() const;
            const bool PortAcceptsTranceive() const;
            const bool PortSendsTranceive() const;
            const string& PortName() const;
            const uint8_t RealAddress() const;
            const uint8_t VirtualAddress() const;
    };
}

#endif // CIV_PORT_H