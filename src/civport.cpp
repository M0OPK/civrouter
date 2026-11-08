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
        // First check if there's jammers to process
        if (IsRadioPort())
        {
            if (serialPort.JammerSentFlag())
            {
                while (serialPort.JammerSentFlag())
                {
                    cout << "Jammer Sent" << " frames in progress " << frameInProgressBuffer.size() << endl;
                    // See if we have the frame
                    auto sentJammerPosition = serialPort.ConsumeSentJammer();
                    if (!frameInProgressBuffer.empty())
                    {
                        for(auto frame : frameInProgressBuffer)
                        {
                            if (frame.first <= sentJammerPosition && (frame.first + frame.second.FrameSize()) >= sentJammerPosition )
                            {
                                cout << "Resent frame" << endl;
                                serialPort.WaitJammer();
                                serialPort.FlushBuffers();
                                SendFrame(frame.second);
                                break;
                            }
                        }
                    }
                    else 
                    {
                        cout << "Resent last frame (buffer was empty)" << endl;
                        serialPort.WaitJammer();
                        serialPort.FlushBuffers();
                        SendFrame(lastFrame);
                    }
                }
            }
            if (serialPort.JammerReceivedFlag())
            {
                while (serialPort.JammerReceivedFlag())
                {
                    cout << "Jammer Received" << " frames in progress " << frameInProgressBuffer.size() << endl;
                    // See if we have the frame
                    auto receivedJammerPosition = serialPort.ConsumeReceivedJammer();
                    if (!frameInProgressBuffer.empty())
                    {
                        for(auto frame : frameInProgressBuffer)
                        {
                            cout << "Jammer position " << receivedJammerPosition << " frame start " << frame.first << " frame end " << frame.first + frame.second.FrameSize() << endl;
                            if (frame.first <= receivedJammerPosition && (frame.first + frame.second.FrameSize()) >= receivedJammerPosition )
                            {
                                cout << "Resent frame" << endl;
                                serialPort.WaitJammer();
                                serialPort.FlushBuffers();
                                SendFrame(frame.second);
                                break;
                            }
                        }
                    }
                    else 
                    {
                        cout << "Resent last frame (buffer was empty)" << endl;
                        serialPort.WaitJammer();
                        serialPort.FlushBuffers();
                        SendFrame(lastFrame);
                    }
                }
            }

            // Clear down in progress buffer
            if (!frameInProgressBuffer.empty())
            {
                auto confirmedSentCount = serialPort.GetConfirmedSentCount();
                //cout << "Frame in progress: " << frameInProgressBuffer.size() << " confirmed bytes " << confirmedSentCount << "/" << serialPort.GetSentCount() << endl;
                while(!frameInProgressBuffer.empty() && (frameInProgressBuffer[0].first + frameInProgressBuffer[0].second.FrameSize()) <= confirmedSentCount)
                {
                    frameInProgressBuffer.pop_front();
                }
            }
        }

        // Check if there's data to process
        if (serialPort.HasAvailableBytes())
        {
            vector<uint8_t> buffer;
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
        // Store the frame in the inprogress buffer
        if (IsRadioPort())
        {
            frameInProgressBuffer.push_back(pair<ulong, CivFrame>(serialPort.GetSentCount(), frame));
        }
        while (!serialPort.SendBytes(&buffer[0], buffer.size()))
        {
            auto timeout = serialPort.TimeoutErrorFlag();
            //auto jammerReceived = serialPort.JammerReceivedFlag();
            //auto jammerSent = serialPort.JammerSentFlag();
            if (timeout)
                return false;
            /*else if (jammerReceived || jammerSent)
            {
                serialPort.WaitJammer();
                serialPort.FlushBuffers();
            }*/
        }
        //serialPort.WaitSend();
        lastFrame = frame;
        return true;
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
