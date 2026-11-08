#include "civserial.h"
#include <civport.h>
#include <cstdint>
#include <utility>
using namespace civ;

    CivPort::CivPort(const CivSerialPortInfo* portInfo) : serialPort(portInfo->PortName(), portInfo->BaudRate(), portInfo->IsRadio())
    {
        this->serialConfig = portInfo;
        init(portInfo);
    }

    void CivPort::init(const CivSerialPortInfo* portInfo)
    {

    }

    bool CivPort::Update()
    {
        bool foundData = false;

        // Start queued frames, handle collision retries and echo timeouts.
        // Echo checking and jamming happen in CivSerial as bytes arrive.
        serialPort.Service();

        // Check if there's data to process
        if (serialPort.HasAvailableBytes())
        {
            serialPort.PopBytes(serialBuffer);
            auto bufferInfo = CivFrame::SearchBuffer(serialBuffer);
            while (bufferInfo.second > 0)
            {
                auto frame = CivFrame(serialBuffer);
                // Check if we should accept this frame
                /*if (!frame.IsBroadcast() || serialConfig->AcceptTransceive())
                {*/
                    frameBuffer.push_back(frame);
                    bufferInfo = CivFrame::SearchBuffer(serialBuffer);
                    foundData = true;
                /*}*/
            }
        }
        return foundData;
    }
    
    bool CivPort::FrameReady() const
    {
        return !frameBuffer.empty();
    }

    int CivPort::FrameBufferSize() const
    {
        return frameBuffer.size();
    }

    CivFrame CivPort::PopFrame()
    {
        auto frame = frameBuffer[0];
        frameBuffer.pop_front();
        return frame;
    }

    bool CivPort::SendFrame(CivFrame& frame)
    {
        // See if we should send this frame
        /*if (frame.IsBroadcast() && !serialConfig->SendTransceive())
            return true;*/

        if (!frame.ValidFrame())
            return false;
        vector<uint8_t> buffer;
        frame.Serialize(buffer);

        // Queued; CivSerial sends it when the bus is free and retries on collision
        return serialPort.SendFrame(buffer);
    }

    const bool CivPort::IsRadioPort() const
    {
        return this->serialConfig->IsRadio();
    }

    const bool CivPort::PortAcceptsInfo() const
    {
        return this->serialConfig->ReceiveInfoUpdates();
    }

    const bool CivPort::PortSendsInfo() const
    {
        return this->serialConfig->SendInfoUpdates();
    }

    const bool CivPort::PortAcceptsTranceive() const
    {
        return this->serialConfig->AcceptTransceive();
    }

    const bool CivPort::PortSendsTranceive() const
    {
        return this->serialConfig->SendTransceive();
    }
    const string& CivPort::PortName() const
    {
        return this->serialConfig->PortName();
    }

    const uint8_t CivPort::RealAddress() const
    {
        return this->serialConfig->PortId();
    }

    const uint8_t CivPort::VirtualAddress() const
    {
        return this->serialConfig->PortVirtualId();
    }
