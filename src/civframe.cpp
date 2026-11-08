#include "civutils.h"
#include <civframe.h>
#include <cstddef>
#include <iostream>
#include <iterator>
#include <utility>

namespace civ 
{
    using namespace std;
    pair<size_t, size_t> CivFrame::SearchBuffer(const deque<uint8_t>& buffer)
    {
        auto result = pair<size_t, size_t>(0,0);
        bool completeFrame = false;
        bool preambleFound = false;
        bool startFound = false;
        bool toFound = false;
        bool fromFound = false;
        size_t bufferLoc = 0;
        while(bufferLoc < buffer.size() && !completeFrame)
        {
            if (!preambleFound && buffer[bufferLoc] != CIV_PREAMBLE_BYTE)
            {
                bufferLoc++;
                continue;
            }
            else if (!preambleFound)
            {
                preambleFound = true;
                result.first = bufferLoc;
            }
            else if (preambleFound && !startFound && buffer[bufferLoc] != CIV_PREAMBLE_BYTE)
            {
                preambleFound = false;
            }
            else if (preambleFound && !startFound)
            {
                startFound = true;
            }
            else if (startFound && !fromFound && !toFound)
            {
                toFound = true;
            }
            else if (!fromFound && toFound)
            {
                fromFound = true;
            }
            else if (toFound && buffer[bufferLoc] == CIV_END_BYTE)
            {
                completeFrame = true;
                result.second = (bufferLoc - result.first) + 1;
                break;
            }
            bufferLoc++;
        }
        return result;
    }

    CivFrame::CivFrame()
    {
        fromAddress = 0;
        toAddress = 0;
    }

    CivFrame::CivFrame(deque<uint8_t>& buffer, int startHint)
    {
        bool completeFrame = false;
        bool preambleFound = false;
        bool startFound = false;
        bool toFound = false;
        bool fromFound = false;
        size_t bufferLoc = 0;
        size_t bufferStart = 0;

        fullFrame = false;

        // If we have a start hint, start search from there.
        if (startHint != 0 && startHint >= 0 && startHint < buffer.size() && buffer[startHint] == CIV_PREAMBLE_BYTE)
        {
            bufferLoc = startHint;
            bufferStart = startHint;
        }

        while(bufferLoc < buffer.size() && !completeFrame)
        {
            if (!preambleFound && buffer[bufferLoc] != CIV_PREAMBLE_BYTE)
            {
                continue;
            }
            else if (!preambleFound)
            {
                preambleFound = true;
            }
            else if (preambleFound && !startFound && buffer[bufferLoc] != CIV_PREAMBLE_BYTE)
            {
                preambleFound = false;
            }
            else if (preambleFound && !startFound)
            {
                startFound = true;
            }
            else if (startFound && !fromFound && !toFound)
            {
                toAddress = buffer[bufferLoc];
                toFound = true;
            }
            else if (!fromFound && toFound)
            {
                fromAddress = buffer[bufferLoc];
                fromFound = true;
            }
            else if (fromFound && buffer[bufferLoc] == CIV_END_BYTE)
            {
                completeFrame = true;
            }
            else if (toFound)
            {
                payload.push_back(buffer[bufferLoc]);
            }
            bufferLoc++;
        }

        // If we got a complete frame, then delete the bytes from the buffer
        // From the start to the end of the frame
        if (completeFrame)
        {
            for (int pos = bufferStart; pos < bufferLoc; pos++)
            {
                buffer.pop_front();
            }
            fullFrame = true;
        }
    }

    bool CivFrame::ValidFrame() const
    {
        return ((fromAddress != CIV_ADDRESS_BROADCAST || toAddress != CIV_ADDRESS_BROADCAST) && !payload.empty() && fullFrame);
    }

    uint8_t CivFrame::GetFromAddress() const
    {
        return fromAddress;
    }

    uint8_t CivFrame::GetToAddress() const
    {
        return toAddress;
    }

    const uint8_t* CivFrame::GetPayload() const
    {
        return &payload[0];
    }

    void CivFrame::GetPayload(vector<uint8_t>& payload)
    {
        payload = this->payload;
    }

    bool CivFrame::IsBroadcast() const
    {
        return toAddress == CIV_ADDRESS_BROADCAST;
    }

    void CivFrame::SetFromAddress(uint8_t fromAddress)
    {
        this->fromAddress = fromAddress;
    }

    void CivFrame::SetToAddress(uint8_t toAddress)
    {
        this->toAddress = toAddress;
    }

    bool CivFrame::IsChangeCommand() const
    {
        // Payload > 2 to not send request frame, only setters
        return fromAddress != CIV_ADDRESS_BROADCAST && toAddress != CIV_ADDRESS_BROADCAST && payload.size() > 2 && 
            (payload[0] == CIV_TRANCEIVE_SET_FREQUENCY || payload[0] == CIV_TRANCEIVE_SET_MODE || payload[0] == CIV_SET_FREQUENCY || payload[0] == CIV_SET_MODE || 
                (payload[0] == CIV_SET_READ_VFO_FREQUENCY && payload[1] == 0x00) || (payload[0] == CIV_SET_READ_VFO_MODE && payload[1] == 0x00));
    }

    void CivFrame::MakeTransceive()
    {
        if (!IsChangeCommand())
            return;

        if ((payload[0] == CIV_SET_READ_VFO_FREQUENCY || payload[0] == CIV_SET_READ_VFO_MODE) && payload[1] == 0x00)
        {
            // Set to transceive version of command
            payload[0] -= CIV_SET_READ_VFO_FREQUENCY;

            // Remove extra byte in position 1
            payload.erase(next(payload.begin()));
        }
        else if (payload[0] >= CIV_SET_FREQUENCY)
        {
            // Set to transceive version of command
            payload[0] -= CIV_SET_FREQUENCY;
        }
        SetToAddress(CIV_ADDRESS_BROADCAST);
    }

    void CivFrame::Serialize(vector<u_int8_t>& buffer) const
    {
        buffer.push_back(CIV_PREAMBLE_BYTE);
        buffer.push_back(CIV_PREAMBLE_BYTE);
        buffer.push_back(toAddress);
        buffer.push_back(fromAddress);
        for(auto byte : payload)
        {
            buffer.push_back(byte);
        }
        buffer.push_back(CIV_END_BYTE);
    }

    void CivFrame::DumpFrame() const
    {
        cout << "From: " << CivUtils::Uint8ToHex(GetFromAddress()) << " To: " << CivUtils::Uint8ToHex(GetToAddress()) << " Data:";
        for(auto byte : payload)
        {
            cout << " " << CivUtils::Uint8ToHex(byte);
        }
        cout << endl;
    }

    int CivFrame::FrameSize() const
    {
        return payload.size() + 5;
    }    
}