#ifndef CIV_CONFIG_H
#define CIV_CONFIG_H

#include <cassert>
#include <cstdint>
#include <fstream>
#include <string>
#include <vector>
#include <civutils.h>
namespace civ
{
    using namespace std;

    enum CivConfigMode
    {
        MODE_NONE,
        MODE_COMMON,
        MODE_RADIO,
        MODE_VIRTUALPORT,
        MODE_INVALID
    };

    class CivSerialPortInfo
    {
        private:
        string portName;
        uint8_t portId;
        uint8_t portVirtualId;
        int baudRate;
        bool isRadio;
        bool sendTransceive;
        bool acceptTransceive;
        bool sendInfoUpdates;
        bool acceptInfoUpdates;

        public:
        CivSerialPortInfo(string portName, int baudRate, uint8_t portId = 0x80, uint8_t portVirtualId = 0x00, bool isRadio = false, bool sendTransceive = true, bool acceptTransceive = false, bool sendInfoUpdates = true, bool acceptInfoUpdates = true);
        CivSerialPortInfo(vector<uint8_t>& config);
        CivSerialPortInfo(vector<pair<string, string>> portData, CivConfigMode mode);
        void init(string portName, int baudRate, uint8_t portId = 0x80, uint8_t portVirtualId = 0x00, bool isRadio = false, bool sendTransceive = true, bool acceptTransceive = false, bool sendInfoUpdates = true, bool acceptInfoUpdates = true);
        const string& PortName() const;
        const uint8_t PortId() const;
        const uint8_t PortVirtualId() const;
        const int BaudRate() const;
        const bool IsRadio() const;
        const bool SendTransceive() const;
        const bool AcceptTransceive() const;
        const bool SendInfoUpdates() const;
        const bool ReceiveInfoUpdates() const;
        void GetConfigBlob(vector<uint8_t>& config);
        const vector<uint8_t> GetConfigBlob();
    };

    class CivConfig
    {
        friend CivSerialPortInfo;
        
        private:
        const int configVersion = 1;
        bool debugMode;
        vector<CivSerialPortInfo> serialPortInfo;
        void init();
        void init(ifstream* fileStream);

        public:
        CivConfig();
        CivConfig(vector<uint8_t> config);
        CivConfig(string configFile);
        CivConfig(ifstream* fileStream);
        bool DebugMode() const;
        void AddPort(CivSerialPortInfo info);
        void AddPort(string portName, int baudRate, uint8_t portId = 0x80, uint8_t portVirtualId = 0x00, bool isRadio = false, bool sendTransceive = true, bool acceptTransceive = false, bool sendInfoUpdates = true, bool acceptInfoUpdates = true);
        const CivSerialPortInfo* GetPort(string portName) const;
        const vector<CivSerialPortInfo>* GetPorts() const;
        const vector<string> GetPortNames() const;
        const vector<uint8_t> GetConfigBlob();
        void DumpConfig() const;

    };

}
#endif // CIV_CONFIG_H