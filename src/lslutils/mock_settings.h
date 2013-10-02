#ifndef LSL_MOCK_SETTINGS_H
#define LSL_MOCK_SETTINGS_H

#include <string>
#include <vector>
#include <map>

#include "misc.h"

class Settings {
public:
	std::string GetCachePath() const { return std::string(); }
	std::string GetForcedSpringConfigFilePath() const { return std::string(); }
	std::string GetCurrentUsedUnitSync() const { return std::string("libunitsync.so"); }
	std::string GetCurrentUsedDataDir() const { return std::string(); }
	std::string GetCurrentUsedSpringBinary() const { return std::string(); }
	std::string GetCurrentUsedSpringConfigFilePath() const { return std::string(); }
	std::vector<std::string> GetPresetList() const { return std::vector<std::string>(); }
	std::map<std::string,std::string> GetHostingPreset(std::string, int) const { return std::map<std::string,std::string>(); }
	LSL::lslColor GetBattleLastColor(){}
	int GetClientPort(){return 0;}
	int GetBattleLastSideSel(std::string) { return 0;}
	void SetMapLastStartPosType(std::string, std::string){}
	void SetMapLastRectPreset(std::string, std::vector<LSL::Util::SettStartBox>&){}
	std::string GetMapLastStartPosType(std::string){return std::string();}

	std::vector<LSL::Util::SettStartBox> GetMapLastRectPreset(std::string){return std::vector<LSL::Util::SettStartBox>();}
	int GetBattleLastAutoAnnounceDescription() { return 0;}
	int GetBattleLastAutoSpectTime() {return 0;}

	void SetHostingPreset(std::string, int, std::map<std::basic_string<char>, std::basic_string<char> >) {}
	void DeletePreset(std::string){}
	void SaveSettings() {}
};

static Settings& sett() {
	static Settings a;
	return a;
}

template <class T>
std::string STD_STRING(const T& t)
{
	return std::string(t);
}

#endif // LSL_MOCK_SETTINGS_H
