#include "config.h"

#include <lslutils/conversion.h>

#include "misc.h"
#include "globalsmanager.h"

//SL includes -- bad
#if HAVE_SPRINGLOBBY
    #include "settings.h"
    #include "utils/conversion.h"
#else
#include "mock_settings.h"
#endif

namespace bf = boost::filesystem;

namespace LSL {
namespace Util {
Config::Config()
{
}

Config& config() {
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
    return bf::path(STD_STRING(sett().GetCachePath()));
}

bf::path Config::GetCurrentUsedUnitSync() const
{
    return bf::path(STD_STRING(sett().GetCurrentUsedUnitSync()));
}

bf::path Config::GetCurrentUsedDataDir() const
{
    return bf::path(STD_STRING(sett().GetCurrentUsedDataDir()));
}

bf::path Config::GetCurrentUsedSpringBinary() const
{
    return bf::path(STD_STRING(sett().GetCurrentUsedSpringBinary()));
}

bf::path Config::GetCurrentUsedSpringConfigFilePath() const
{
    return bf::path(STD_STRING(sett().GetCurrentUsedSpringConfigFilePath()));
}


} // namespace Util
}// namespace LSL {
