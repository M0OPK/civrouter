#ifndef CIV_FRAME_H
#define CIV_FRAME_H

#include <cstddef>
#include <cstdint>
#include <deque>
#include <sys/types.h>
#include <vector>
#include <civutils.h>
namespace civ
{
    using namespace std;
    const uint8_t CIV_TRANCEIVE_SET_FREQUENCY = 0x00;
    const uint8_t CIV_TRANCEIVE_SET_MODE = 0x01;
    const uint8_t CIV_READ_FREQUENCY = 0x03;
    const uint8_t CIV_READ_MODE = 0x04;
    const uint8_t CIV_SET_FREQUENCY = 0x05;
    const uint8_t CIV_SET_MODE = 0x06;
    const uint8_t CIV_SET_READ_VFO_FREQUENCY = 0x25;
    const uint8_t CIV_SET_READ_VFO_MODE = 0x26;
    const uint8_t CIV_NG_BYTE = 0xfa;
    const uint8_t CIV_OK_BYTE = 0xfb;
    const uint8_t CIV_JAMMER_BYTE = 0xfc;
    const uint8_t CIV_END_BYTE = 0xfd;
    const uint8_t CIV_PREAMBLE_BYTE = 0xfe;
    const uint8_t CIV_ADDRESS_BROADCAST = 0x00;
    const uint8_t CIV_ADDRESS_CONTROLLER_BASE = 0xe0;

    class CivFrame
    {
        private:
        uint8_t fromAddress;
        uint8_t toAddress;
        vector<uint8_t> payload;
        bool fullFrame;

        public:
        static pair<size_t, size_t> SearchBuffer(const deque<uint8_t>& buffer);
        CivFrame();
        CivFrame(deque<uint8_t>& buffer, int startHint = 0);
        bool ValidFrame() const;
        uint8_t GetFromAddress() const;
        uint8_t GetToAddress() const;
        const uint8_t* GetPayload() const;
        void GetPayload(vector<uint8_t>& payload);
        bool IsBroadcast() const;
        void SetFromAddress(uint8_t fromAddress);
        void SetToAddress(uint8_t toAddress);
        void MakeTransceive();
        bool IsChangeCommand() const;
        void Serialize(vector<u_int8_t>& buffer) const;
        void DumpFrame() const;
        int FrameSize() const;
    };
}
#endif // CIV_FRAME_H