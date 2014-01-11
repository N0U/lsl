#ifndef LIBSPRINGLOBBY_HEADERGUARD_SPRINGBUNDLE_H
#define LIBSPRINGLOBBY_HEADERGUARD_SPRINGBUNDLE_H

#include <string>

namespace LSL {

class SpringBundle
{
public:
	SpringBundle(): valid(false){};
	bool GetBundleVersion(bool force = false);
	// try to fill missing information by guessing
	bool AutoComplete(std::string searchpath="");
	bool IsValid();
	std::string unitsync;
	std::string spring;
	std::string version;
	std::string path;
private:
	bool AutoFindUnitsync(const std::string& unitsyncpath);
	std::string GetLibExtension();
	bool valid;
};

};

#endif
