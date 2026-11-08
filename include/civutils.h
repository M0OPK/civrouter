#ifndef CIV_UTILS_H
#define CIV_UTILS_H

#include <algorithm>
#include <cassert>
#include <cctype>
#include <cstdint>
#include <iomanip>
#include <ios>
#include <sstream>
#include <string>
#include <vector>
namespace civ 
{
    using namespace std;

    class CmdOptions
    {
        public:
        string Filename;
        bool HelpRequested;

        CmdOptions()
        {
            Filename = string();
            HelpRequested = false;
        }
    };

    class CivUtils
    {
        public:
            static void PushInt(vector<uint8_t>& config, int input)
            {
                config.push_back(input & 0xff);
                config.push_back((input >> 8) & 0xff);
                config.push_back((input >> 16) & 0xff);
                config.push_back((input >> 24) & 0xff);
            }

            static int PopInt(vector<uint8_t>& config)
            {
                int result = (uint8_t)config[0];
                config.erase(config.begin());
                result += (uint8_t)config[0] << 8;
                config.erase(config.begin());
                result += (uint8_t)config[0] << 16;
                config.erase(config.begin());
                result += (uint8_t)config[0] << 24;
                config.erase(config.begin());
                return result;
            }

            static void PushString(vector<uint8_t>& config, string& input)
            {
                // Push length first
                PushInt(config, (int)input.length());

                for (char character : input)
                {
                    config.push_back(character);
                }
            }

            static string PopString(vector<uint8_t>& config)
            {
                // Get length
                int length = PopInt(config);
                string result = "";
                if (length > 0)
                {
                    for (int pos = 0; pos < length; pos++)
                    {
                        result += config[0];
                        config.erase(config.begin());
                    }
                }
                return result;
            }

            static string StringToLower(string input)
            {
                string newString = string(input);
                transform(newString.begin(), newString.end(), newString.begin(), [](unsigned char c){ return tolower(c); });
                return newString;
            }

            static uint8_t HexStringToUint8(string input)
            {
                auto value = stoul(input, nullptr, 16);
                assert(value <= 0xff);
                return (uint8_t)value;
            }

            static string BoolToYesNo(bool value)
            {
                if (value)
                    return "Y";
                return "N";
            }

            static string Uint8ToHex(uint8_t value)
            {
                stringstream result;
                result << hex << setw(2) << setfill('0') << (int)value;
                return result.str();
            }

            static CmdOptions DecodeCmdOptions(char **argv, int &argc)
            {
                auto result = CmdOptions();
                for (int argCount = 1; argCount < argc; argCount++)
                {
                    auto cmdParameter = string(argv[argCount]);
                    if (StringToLower(cmdParameter) == "-f" && argCount+1 < argc)
                    {
                        result.Filename = string(argv[++argCount]);
                    }
                    else if(StringToLower(cmdParameter) == "-h")
                    {
                        result.HelpRequested = true;
                    }
                }

                return result;
            }

            static string Uint8VectorToHexString(vector<uint8_t>& input)
            {
                string result;
                for(auto byte : input)
                {
                    result += Uint8ToHex(byte) + "";
                }
                return result;
            }

            static const string GetHomeFolder()
            {
        #ifdef _WIN32
                return "";
        #else
                if (getenv("HOME") != nullptr)
                    return getenv("HOME");
                return "";
        #endif
            }
    };
}
#endif // CIV_UTILS_H