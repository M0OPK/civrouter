#include <algorithm>
#include <civconfig.h>
#include <cstdint>
#include <iostream>
#include <iterator>
#include <ostream>
#include <regex>
#include <string>
#include <string_view>
#include <sys/types.h>
#include <utility>

namespace civ
{
    CivSerialPortInfo::CivSerialPortInfo(string portName, int baudRate, uint8_t portId, uint8_t portVirtualId, bool isRadio, bool sendTransceive, bool acceptTransceive, bool sendInfoUpdates, bool acceptInfoUpdates)
    {
        this->init(portName, baudRate, portId, portVirtualId, isRadio, sendTransceive, acceptTransceive, sendInfoUpdates, acceptInfoUpdates);
    }

    // Will be used in esp32 version
    CivSerialPortInfo::CivSerialPortInfo(vector<uint8_t>& config)
    {
        this->portName = CivUtils::PopString(config);
        this->portId = config[0];
        config.erase(config.begin());
        this->portVirtualId = config[0];
        config.erase(config.begin());
        this->baudRate = CivUtils::PopInt(config);
        uint8_t flags = config[0];
        config.erase(config.begin());
        this->isRadio = (flags >> 7 & 0x01) == 0x01;
        this->sendTransceive = (flags >> 6 & 0x01) == 0x01;
        this->acceptTransceive = (flags >> 5 & 0x01) == 0x01;
        this->sendInfoUpdates = (flags >> 4 & 0x01) == 0x01;
        this->acceptInfoUpdates = (flags >> 3 & 0x01) == 0x01;
    }

    CivSerialPortInfo::CivSerialPortInfo(vector<pair<string, string>> portData, CivConfigMode mode)
    {
        // First set out defaults
        init("", 0);
        this->isRadio = (mode == MODE_RADIO);

        for(auto portItem : portData)
        {
            if (portItem.first == "portaddress")
                this->portName = portItem.second;
            else if (portItem.first == "baudrate")
                this->baudRate = atoi(portItem.second.c_str());
            else if (portItem.first == "realaddress")
                this->portId = CivUtils::HexStringToUint8(portItem.second.c_str());
            else if (portItem.first == "translateaddress")
                this->portVirtualId = CivUtils::HexStringToUint8(portItem.second.c_str());
            else if (portItem.first == "accepttransceive")
                this->acceptTransceive = (CivUtils::StringToLower(portItem.second) == "true");
            else if (portItem.first == "sendtransceive")
                this->sendTransceive = (CivUtils::StringToLower(portItem.second) == "true");
            else if (portItem.first == "sendinfoupdates")
                this->sendInfoUpdates = (CivUtils::StringToLower(portItem.second) == "true");
            else if (portItem.first == "acceptinfoupdates")
                this->acceptInfoUpdates = (CivUtils::StringToLower(portItem.second) == "true");
        }
    }

    void CivSerialPortInfo::init(string portName, int baudRate, uint8_t portId, uint8_t portVirtualId, bool isRadio, bool sendTransceive, bool acceptTransceive, bool sendInfoUpdates, bool acceptInfoUpdates)
    {
        this->portName = portName;
        this->baudRate = baudRate;
        this->portId = portId;
        this->portVirtualId = portVirtualId;
        this->isRadio = isRadio;
        this->sendTransceive = sendTransceive;
        this->acceptTransceive = acceptTransceive;
        this->sendInfoUpdates = sendInfoUpdates;
        this->acceptInfoUpdates = acceptInfoUpdates;
    }

    const string& CivSerialPortInfo::PortName() const
    {
        return portName;
    }

    const uint8_t CivSerialPortInfo::PortId() const
    {
        return portId;
    }

    const uint8_t CivSerialPortInfo::PortVirtualId() const
    {
        return portVirtualId;
    }

    const int CivSerialPortInfo::BaudRate() const
    {
        return baudRate;
    }

    const bool CivSerialPortInfo::IsRadio() const
    {
        return isRadio;
    }

    const bool CivSerialPortInfo::SendTransceive() const
    {
        return sendTransceive;
    }

    const bool CivSerialPortInfo::AcceptTransceive() const
    {
        return acceptTransceive;
    }

    const bool CivSerialPortInfo::SendInfoUpdates() const
    {
        return sendInfoUpdates;
    }

    const bool CivSerialPortInfo::ReceiveInfoUpdates() const
    {
        return acceptInfoUpdates;
    }

    // Will be used in esp32 version
    void CivSerialPortInfo::GetConfigBlob(vector<uint8_t>& config)
    {            
        CivUtils::PushString(config, portName);
        config.push_back(portId);
        config.push_back(portVirtualId);
        CivUtils::PushInt(config, baudRate);
        uint8_t flags = 0;
        if (isRadio)
            flags |= (1 << 7);
        if (sendTransceive)
            flags |= (1 << 6);
        if (acceptTransceive)
            flags |= (1 << 5);
        if (sendInfoUpdates)
            flags |= (1 << 4);
        if (acceptInfoUpdates)
            flags |= (1 << 3);
        config.push_back(flags);
    }

    const vector<uint8_t> CivSerialPortInfo::GetConfigBlob()
    {
        vector<uint8_t> config = vector<uint8_t>();
        GetConfigBlob(config);
        return config;
    }

    void CivConfig::init()
    {
        this->serialPortInfo = vector<CivSerialPortInfo>();
    }

    void CivConfig::init(ifstream* fileStream)
    {
        string configLine;
        string cleanLine;
        regex trimLine("^(\\s*)(.*?)(\\s*)$");  // Remove leading and trailing space from what remains
        regex getValue("^(\\S*)(\\s*)(.*)$");
        regex getValueString("^(\\S*)(\\s*)\\\"(.*)\\\"\\s*?$");
        vector<string> configLines;
        while(getline(*fileStream, configLine))
        {
            // Find first position of the comment marker
            size_t comment_index = configLine.find_first_of("#");

            if (comment_index >= configLine.length())
            {
                // If comment character not found copy whole config line
                cleanLine = configLine;
            }
            else if (comment_index == 0)
            {
                // If comment character is the first character use empty text for line
                cleanLine = "";
            }
            else
            {
                // Otherwise return all text up to the comment
                cleanLine = configLine.substr(0, comment_index);
            }
            
            cleanLine = regex_replace(cleanLine, trimLine, "$2");
            
            if (cleanLine.length() > 0)
                configLines.push_back(cleanLine);
        }

        CivConfigMode mode = MODE_NONE;   // No mode set
        vector<pair<string, string>> currentPortData;
        for (string line : configLines)
        {
            string lowerLine = CivUtils::StringToLower(line);
            string_view line_sv = string_view(line);
            if (line.front() == '[' && line.back() == ']')
            {
                // First check if we have data to make a port with
                if (!currentPortData.empty())
                {
                    serialPortInfo.push_back(CivSerialPortInfo(currentPortData, mode));
                    currentPortData.clear();
                }

                if (lowerLine == "[common]")
                {
                    mode = MODE_COMMON;
                }
                else if (lowerLine == "[radio]")
                {
                    mode = MODE_RADIO;
                    currentPortData.clear();
                }
                else if (lowerLine == "[virtualserial]")
                {
                    mode = MODE_VIRTUALPORT;
                    currentPortData.clear();
                }
                else
                {
                    cerr << "Unexpected section: " << lowerLine << endl;
                }
            }
            else
            {
                string line_item_name = regex_replace(line, getValue, "$1");
                string line_item_value;
                if (line.find("\"") == string::npos)
                {
                    line_item_value = regex_replace(line, getValue, "$3");
                }
                else
                {
                    line_item_value = regex_replace(line, getValueString, "$3");
                }
                 
                if (line_item_name.length() != 0 && line_item_value.length() != 0)
                {
                    if (mode == MODE_COMMON)
                    {
                        if (CivUtils::StringToLower(line_item_name) == "debug")
                        {
                            this->debugMode = (CivUtils::StringToLower(line_item_value) == "true");
                        }
                    }
                    else if (mode == MODE_RADIO || mode == MODE_VIRTUALPORT)
                    {
                        currentPortData.push_back(pair<string, string>(CivUtils::StringToLower(line_item_name), line_item_value));
                    }
                    //cout << line_item_name << " = " << line_item_value << endl;
                }
                else
                {
                    cout << line << endl;
                }
            }
        }

        // Add the last port, if there is one
        if (!currentPortData.empty())
        {
            serialPortInfo.push_back(CivSerialPortInfo(currentPortData, mode));
            currentPortData.clear();
        }
    }

    CivConfig::CivConfig()
    {
        this->init();
    }

    CivConfig::CivConfig(vector<uint8_t> config)
    {
        this->init();
        int version = CivUtils::PopInt(config);
        assert(version == configVersion);
        int portCount = CivUtils::PopInt(config);
        for(int portNum = 0; portNum < portCount; portNum++)
        {
            auto port = CivSerialPortInfo(config);
            this->AddPort(port);
        }
    }

    CivConfig::CivConfig(string configFile)
    {
        ifstream configStream(configFile);
        if (!configStream.is_open())
        {
            cerr << "Error opening file " << configFile << endl;
            return;
        }
        this->init(&configStream);
        configStream.close();
    }

    CivConfig::CivConfig(ifstream* fileStream)
    {
        this->init(fileStream);
    }

    void CivConfig::AddPort(CivSerialPortInfo info)
    {
        serialPortInfo.push_back(info);
    }

    void CivConfig::AddPort(string portName, int baudRate, uint8_t portId, uint8_t portVirtualId, bool isRadio, bool sendTransceive, bool acceptTransceive, bool sendInfoUpdates, bool acceptInfoUpdates)
    {
        CivSerialPortInfo info(portName, baudRate, portId, portVirtualId, isRadio, sendTransceive, acceptTransceive, sendInfoUpdates, acceptInfoUpdates);
        serialPortInfo.push_back(info);
    }

    bool CivConfig::DebugMode() const
    {
        return this->debugMode;
    }

    const CivSerialPortInfo* CivConfig::GetPort(const string portName) const
    {
        auto iter = find_if(serialPortInfo.begin(), serialPortInfo.end(), [&portName](const CivSerialPortInfo& portObj) 
        { 
            return portObj.PortName() == portName; 
        });

        if (iter == serialPortInfo.end())
            return nullptr;

        auto idx = std::distance(serialPortInfo.begin(), iter);
        return &serialPortInfo[idx];
    }

    const vector<CivSerialPortInfo>* CivConfig::GetPorts() const
    {
        return &serialPortInfo;
    }

    const vector<string> CivConfig::GetPortNames() const
    {
        vector<string> portNames;
        for(auto info : serialPortInfo)
        {
            portNames.push_back(info.PortName());
        }
        return portNames;
    }

    const vector<uint8_t> CivConfig::GetConfigBlob()
    {
        vector<uint8_t> config = vector<uint8_t>();
        CivUtils::PushInt(config, configVersion);
        
        // Add number of ports
        CivUtils::PushInt(config, serialPortInfo.size());

        for (CivSerialPortInfo port : serialPortInfo)
        {
            port.GetConfigBlob(config);
        }
        return config;
    }

    void CivConfig::DumpConfig() const
    {
        cout << "Configuration:" << endl;
        cout << "Debug mode: " << this->debugMode << endl;
        cout << endl;
        if (!this->serialPortInfo.empty())
        {
            cout << "Ports:" << endl;
            for(auto port : *this->GetPorts())
            {
                if (port.IsRadio())
                    cout << "  Mode: Radio";
                else
                    cout << "  Mode: Port";
                cout << "    Name              : " << port.PortName() << endl;
                cout << "    ID                : " << CivUtils::Uint8ToHex(port.PortId()) << endl;
                cout << "    Virtual ID        : " << CivUtils::Uint8ToHex(port.PortVirtualId()) << endl;
                cout << "    Baud Rate         : " << port.BaudRate() << endl;
                cout << "    Accept Transceive : " << CivUtils::BoolToYesNo(port.AcceptTransceive()) << endl;
                cout << "    Send   Transceive : " << CivUtils::BoolToYesNo(port.SendTransceive()) << endl;
                cout << "    Accept Info       : " << CivUtils::BoolToYesNo(port.ReceiveInfoUpdates()) << endl;
                cout << "    Send Info         : " << CivUtils::BoolToYesNo(port.SendInfoUpdates()) << endl;
            }
        }
    }
}