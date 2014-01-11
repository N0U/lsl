
#include "springbundle.h"
#include "sharedlib.h"
#include "signatures.h"
#include <string>
#include <boost/filesystem.hpp>

#include <lslutils/misc.h>

namespace LSL {


#ifdef __APPLE__
#define LIBEXT ".dylib"
#elif __WIN32__
#define LIBEXT ".dll"
#else
#define LIBEXT ".so"
#endif

#ifdef __WIN32__
#define EXEEXT ".exe"
#define SEP "\\"
#else
#define SEP "/"
#define EXEEXT ""
#endif

std::string SpringBundle::GetLibExtension()
{
#ifdef __APPLE__
	return std::string(".dylib");
#elif __WIN32__
	return std::string(".dll");
#else
	return std::string(".so");
#endif
}

bool SpringBundle::GetBundleVersion(bool force)
{
	if (!force && !version.empty()) //get version only once
		return true;
	if (!Util::FileExists(unitsync)) {
		return false;
	}
	void* temphandle = _LoadLibrary(unitsync);
	std::string functionname = "GetSpringVersion";
	GetSpringVersionPtr getspringversion =(GetSpringVersionPtr)GetLibFuncPtr( temphandle, functionname);
	if( !getspringversion ) {
		_FreeLibrary(temphandle);
		LslError("getspringversion: function not found %s", unitsync.c_str());
		return false;
	}
	functionname = "IsSpringReleaseVersion";
	IsSpringReleaseVersionPtr isspringreleaseversion =(IsSpringReleaseVersionPtr)GetLibFuncPtr( temphandle, functionname);

	functionname = "GetSpringVersionPatchset";
	GetSpringVersionPatchsetPtr getspringversionpatcheset =(GetSpringVersionPatchsetPtr)GetLibFuncPtr( temphandle, functionname);

	version = getspringversion();
	if (isspringreleaseversion && getspringversionpatcheset && isspringreleaseversion()) {
		version += ".";
		version += getspringversionpatcheset();
	}
	return !version.empty();
}

bool SpringBundle::IsValid()
{
	if (valid) return true; //verify only once
	if (!Util::FileExists(path)) {
		return false;
	}
	if (!Util::FileExists(spring)) {
		return false;
	}
	valid = GetBundleVersion();
	return valid;
}

bool SpringBundle::AutoFindUnitsync(const std::string& unitsyncpath)
{
	if (!unitsync.empty() && (Util::FileExists(unitsync)))
		return true;
	boost::filesystem::path tmp(unitsyncpath + SEP + "unitsync" + GetLibExtension());
	if (Util::FileExists(tmp.string())) {
		unitsync = tmp.string();
		return true;
	}

	tmp = unitsyncpath + SEP + "libunitsync" + GetLibExtension();
	if (Util::FileExists(tmp.string())) {
		unitsync = tmp.string();
		return true;
	}
	return false;
}

bool SpringBundle::AutoComplete(std::string searchpath)
{
	// try to find unitsync file name from path
	if (unitsync.empty()) {
		if (!searchpath.empty() && (AutoFindUnitsync(searchpath))) {}
		else if (!path.empty())
			AutoFindUnitsync(path);
	}
	//try to find path from unitsync
	if (path.empty() && !unitsync.empty()) {
		const boost::filesystem::path tmp(unitsync);
		if (Util::FileExists(tmp.parent_path().string()))
			path = tmp.parent_path().string();
	}
	//try to find path from spring
	if (path.empty() && !spring.empty()) {
		const boost::filesystem::path tmp(spring);
		if (Util::FileExists(tmp.parent_path().string()))
			path = tmp.parent_path().string();
	}
	if (spring.empty()) {
		boost::filesystem::path tmp(path);
		tmp /= "spring" EXEEXT;
		if (Util::FileExists(tmp.string())) {
			spring = tmp.string();
		} else {
			tmp = searchpath;
			tmp /= "spring" EXEEXT;
			if (Util::FileExists(tmp.string())) {
				spring = tmp.string();
			}
		}
	}
	if (version.empty()) {
		GetBundleVersion();
	}
	//printf("%s %s %s %s %s\n", __FUNCTION__, searchpath.c_str(), unitsync.c_str(), spring.c_str(), version.c_str());
	return IsValid();
}


};

