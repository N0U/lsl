#ifndef LSL_MOCK_SETTINGS_H
#define LSL_MOCK_SETTINGS_H

#include <string>
#include <vector>
#include <map>

class Settings {
public:
    std::string GetCachePath() const { return std::string(); }
    std::string GetForcedSpringConfigFilePath() const { return std::string(); }
    std::string GetCurrentUsedUnitSync() const { return std::string(); }
    std::string GetCurrentUsedDataDir() const { return std::string(); }
    std::string GetCurrentUsedSpringBinary() const { return std::string(); }
    std::string GetCurrentUsedSpringConfigFilePath() const { return std::string(); }
    std::vector<std::string> GetPresetList() const { return std::vector<std::string>(); }
    std::map<std::string,std::string> GetHostingPreset(std::string, int) const { return std::map<std::string,std::string>(); }
    void SetHostingPreset(std::string, int, std::map<std::basic_string<char>, std::basic_string<char> >) {}
    void DeletePreset(std::string){}
    void SaveSettings() {}
};

Settings& sett() {
    static Settings a;
    return a;
}

template <class T>
std::string STD_STRING(const T& t)
{
    return std::string(t);
}

#endif // LSL_MOCK_SETTINGS_H
