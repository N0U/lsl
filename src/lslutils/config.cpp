#include "config.h"

#include <lslutils/conversion.h>

#include "misc.h"
#include "globalsmanager.h"

namespace bf = boost::filesystem;

namespace LSL {
namespace Util {

// FIXME chose better defaults
Config::Config():
	Cache(bf::path("cache")),
	CurrentUsedUnitSync(bf::path("unitsync")),
	CurrentUsedSpringBinary(bf::path("spring"))
{
}

Config& config()
{
	static LSL::Util::LineInfo<Config> m( AT );
	static LSL::Util::GlobalObjectHolder<Config, LSL::Util::LineInfo<Config> > m_sett( m );
	return m_sett;
}

lslColor Config::GetBattleLastColor() const
{
	//    auto
	return lslColor();
}

bf::path Config::GetCachePath() const
{
	return Cache;
}

bf::path Config::GetCurrentUsedUnitSync() const
{
	return CurrentUsedUnitSync;
}


bf::path Config::GetCurrentUsedSpringBinary() const
{
	return CurrentUsedSpringBinary;
}

void Config::ConfigurePaths(
	boost::filesystem::path Cache,
	boost::filesystem::path CurrentUsedUnitSync,
	boost::filesystem::path CurrentUsedSpringBinary
)
{
	this->Cache = Cache;
	this->CurrentUsedUnitSync = CurrentUsedUnitSync;
	this->CurrentUsedSpringBinary = CurrentUsedSpringBinary;
}

} // namespace Util
}// namespace LSL {
