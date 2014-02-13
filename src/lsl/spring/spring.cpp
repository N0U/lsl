/* Copyright (C) 2007 The SpringLobby Team. All rights reserved. */
//
// Class: Spring
//

#ifdef _MSC_VER
#ifndef NOMINMAX
#define NOMINMAX
#endif // NOMINMAX
#include <winsock2.h>
#endif // _MSC_VER

#include <stdexcept>
#include <vector>
#include <algorithm>
#include <fstream>
#include <clocale>

#include "spring.h"
#include "springprocess.h"
#include <lslutils/debug.h>
#include <lslutils/conversion.h>
#include <lslutils/config.h>
#include <lslutils/mock_settings.h>
#include <lsl/container/userlist.h>
#include <lsl/battle/battle.h>
#include <lsl/battle/singleplayer.h>
#include <lsl/battle/offline.h>
#include <lsl/user/user.h>
#include <lslunitsync/unitsync.h>
#include <lslunitsync/optionswrapper.h>
#include <lsl/battle/tdfcontainer.h>
#include <lslutils/globalsmanager.h>

#include <boost/filesystem.hpp>
#include <boost/filesystem/fstream.hpp>
#include <boost/algorithm/string.hpp>

namespace LSL {

namespace BF = boost::filesystem;
namespace BA = boost::algorithm;

Spring& spring()
{
    static LSL::Util::LineInfo<Spring> m( AT );
    static LSL::Util::GlobalObjectHolder<Spring, LSL::Util::LineInfo<Spring> > m_spring( m );
    return m_spring;
}

Spring::Spring()
    : m_process(0),
      m_running(false)
{}

Spring::~Spring()
{
    delete m_process;
}

bool Spring::IsRunning() const
{
    return m_running;
}

bool Spring::RunReplay ( const std::string& filename )
{
    LslDebug( "launching spring with replay: %s",  filename.c_str() );

    return LaunchSpring( "\"" + filename + "\"");
}

bool Spring::Run(const IBattlePtr battle )
{
    BF::path path = sett().GetCurrentUsedDataDir();
    path /= "script.txt";
    try {
        BF::ofstream f( path );
        if ( !f.is_open() ) {
            LslError( "Access denied to script.txt at %s", path.string().c_str() );
        }
        battle->DisableHostStatusInProxyMode( true );
        f << WriteScriptTxt(battle);
        battle->DisableHostStatusInProxyMode( false );
        f.close();
    }
    catch ( std::exception& e ) {
        LslError( "Couldn't write script.txt, exception caught:\n %s", e.what() );
        return false;
    }
    catch (...) {
        LslError( "Couldn't write script.txt" );
        return false;
    }

    std::string cmd;
    //! TODO
//    if ( battle->GetAutoHost().GetEnabled() )
//    {
//        // -m, --minimise          Start minimised
//        // -q [T], --quit=[T]      Quit immediately on game over or after T seconds
//        cmd = "--minimise";
//    }
    cmd += std::string(" \"" + path.string() +  "\"");
    return LaunchSpring( cmd );
}

//bool Spring::Run( Battle::SinglePlayerBattle& battle )
//{
//    BF::path path = sett().GetCurrentUsedDataDir();
//    path /= "script.txt";
//    std::string cmd = "\"" + path.string().c_str() + "\"";
//    try {
//        BF::ofstream f( path );
//        if ( !f.is_open() ) {
//            LslError( "Access denied to script.txt at %s", path.string().c_str() );
//        }
//        battle->DisableHostStatusInProxyMode( true );
//        f << WriteScriptTxt(battle);
//        battle->DisableHostStatusInProxyMode( false );
//        f.close();
//    }
//    catch ( std::exception& e ) {
//        LslError( "Couldn't write script.txt, exception caught:\n %s", e.what() );
//        return false;
//    }
//    catch (...) {
//        LslError( "Couldn't write script.txt" );
//        return false;
//    }
//    return LaunchSpring( cmd );
//}

//bool Spring::Run(NoGuiSinglePlayerBattle &battle)
//{
//    BF::path path = sett().GetCurrentUsedDataDir();
//    path /= "script.txt";
//    std::string cmd = "\"" + path.string().c_str() + "\"";
//    try {
//        BF::ofstream f( path );
//        if ( !f.is_open() ) {
//            LslError( "Access denied to script.txt at %s", path.string().c_str() );
//        }
//        battle->DisableHostStatusInProxyMode( true );
//        f << WriteScriptTxt(battle);
//        battle->DisableHostStatusInProxyMode( false );
//        f.close();
//    }
//    catch ( std::exception& e ) {
//        LslError( "Couldn't write script.txt, exception caught:\n %s", e.what() );
//        return false;
//    }
//    catch (...) {
//        LslError( "Couldn't write script.txt" );
//        return false;
//    }
//    return LaunchSpring( cmd );
//}

bool Spring::Run(const std::string& script)
{
    BF::path path = sett().GetCurrentUsedDataDir();
    path /= "script.txt";
    std::string cmd = std::string(" \"" + path.string() +  "\"");
    try {
        BF::ofstream f( path );
        if ( !f.is_open() ) {
            LslError( "Access denied to script.txt at %s", path.string().c_str() );
        }
        f << script;
        f.close();
    }
    catch ( std::exception& e ) {
        LslError( "Couldn't write script.txt, exception caught:\n %s", e.what() );
        return false;
    }
    catch (...) {
        LslError( "Couldn't write script.txt" );
        return false;
    }
    return LaunchSpring( cmd );
}

bool Spring::Run( Battle::OfflineBattle& battle )
{
    std::string path = battle.GetPlayBackFilePath();
    return LaunchSpring( "\"" + path + "\"" );
}

bool Spring::LaunchSpring( const std::string& params  )
{
    if ( m_running )
    {
        LslError( "Spring already running!" );
        return false;
    }
    if ( !Util::FileExists( sett().GetCurrentUsedSpringBinary() ) ) {
        LslError( "spring binary not found at set location: %s", sett().GetCurrentUsedSpringBinary().c_str() );
        return false;
    }

    std::string configfileflags = sett().GetCurrentUsedSpringConfigFilePath();
    if ( !configfileflags.empty() )
    {
        configfileflags = "--config=\"" + configfileflags + "\" ";
    }

    std::string cmd =  "\"" + sett().GetCurrentUsedSpringBinary();
#ifdef __WXMAC__
    wxChar sep = wxFileName::GetPathSeparator();
    if ( sett().GetCurrentUsedSpringBinary().AfterLast('.') == "app" )
        cmd += sep + std::string("Contents") + sep + std::string("MacOS") + sep + std::string("spring"); // append app bundle inner path
#endif
    cmd += "\" " + configfileflags + params;

    LslDebug( "spring call params: %s", cmd.c_str() );
    if ( m_process == 0 )
        m_process = new SpringProcess( *this );
    m_process->Create();
    m_process->SetCommand( cmd );
    m_process->Run();

    m_running = true;
    return true;
}


void Spring::OnTerminated(int event )
{
    m_running = false;

    int exit_code = 0l;
    sig_springStopped(exit_code,"");
}


std::string Spring::WriteScriptTxt( const IBattlePtr battle ) const
{
    std::stringstream ret;

    TDF::TDFWriter tdf(ret);

    // Start generating the script.
    tdf.EnterSection( "GAME" );

    tdf.Append( "HostIP", battle->GetHostIp() );
    if ( battle->IsFounderMe() )
    {
        if ( battle->GetNatType() == Enum::NAT_Hole_punching )
            tdf.Append( "HostPort", battle->GetMyInternalUdpSourcePort() );
        else
            tdf.Append("HostPort", battle->GetHostPort() );
    }
    else
    {
        tdf.Append( "HostPort", battle->GetHostPort() );
        if ( battle->GetNatType() == Enum::NAT_Hole_punching )
        {
            tdf.Append( "SourcePort", battle->GetMyInternalUdpSourcePort() );
        }
        else if ( sett().GetClientPort() != 0)
        {
            tdf.Append( "SourcePort", sett().GetClientPort() ); /// this allows to play with broken router by setting SourcePort to some forwarded port.
        }
    }
    tdf.Append( "IsHost", battle->IsFounderMe() );

    const ConstCommonUserPtr  me = battle->GetMe();
    tdf.Append("MyPlayerName", me->Nick() );

    if ( !me->BattleStatus().scriptPassword.empty() )
    {
        tdf.Append( "MyPasswd", me->BattleStatus().scriptPassword );
    }

    if ( !battle->IsFounderMe() )
    {
        tdf.LeaveSection();
        return ret.str();
    }

    /**********************************************************************************
                                                                        Host-only section
            **********************************************************************************/

    tdf.AppendLineBreak();

    tdf.Append("ModHash", battle->LoadMod().hash );
    tdf.Append("MapHash", battle->LoadMap().hash );

    tdf.Append( "Mapname", battle->GetHostMapName() );
    tdf.Append( "GameType", battle->GetHostModName() );

    tdf.AppendLineBreak();

    switch ( battle->GetBattleType() )
    {
    case Enum::BT_Played: break;
    case Enum::BT_Replay:
    {
        std::string path = battle->GetPlayBackFilePath();
        //!TODO this did nothing with wx?!?
//        if ( path.find("/") != std::string::npos )
//            path.BeforeLast('/');
        tdf.Append( "DemoFile", path );
        tdf.AppendLineBreak();
        break;
    }
    case Enum::BT_Savegame:
    {
        std::string path = battle->GetPlayBackFilePath();
        //!TODO this did nothing with wx?!?
//        if ( path.find("/") != std::string::npos )
//            path.BeforeLast('/');
        tdf.Append( "Savefile", path );
        tdf.AppendLineBreak();
        break;
    }
    default:
        break;
    }

    long startpostype = Util::FromString<long>(
            battle->CustomBattleOptions()->getSingleValue( "startpostype", LSL::OptionsWrapper::EngineOption ) );

    std::vector<StartPos> remap_positions;
    if ( battle->IsProxy() && ( startpostype != Enum::ST_Pick ) && ( startpostype != Enum::ST_Choose ) )
    {
        std::set<int> parsedteams;
        unsigned int NumTeams = 0;
        for( const ConstCommonUserPtr usr: battle->Users() )
        {
            const UserBattleStatus& status = usr->BattleStatus();
            if ( status.spectator )
                continue;
            if ( parsedteams.find( status.team ) != parsedteams.end() )
                continue; // skip duplicates
            parsedteams.insert( status.team );
            NumTeams++;
        }

        MapInfo infos = battle->LoadMap().info;
        unsigned int nummapstartpositions = infos.positions.size();
        unsigned int copysize = std::min( nummapstartpositions, NumTeams );
        remap_positions = std::vector<StartPos> ( infos.positions.begin(), infos.positions.begin() + copysize ); // only add the first x positions

        if ( startpostype == Enum::ST_Random )
        {
            random_shuffle( remap_positions.begin(), remap_positions.end() ); // shuffle the positions
        }

    }
    if ( battle->IsProxy() )
    {
        if ( ( startpostype == Enum::ST_Random ) || ( startpostype == Enum::ST_Fixed ) )
        {
            tdf.Append( "startpostype", Enum::ST_Pick );
        }
        else tdf.Append( "startpostype", startpostype );
    }
    else tdf.Append( "startpostype", startpostype );

    tdf.EnterSection( "mapoptions" );
    LSL::OptionsWrapper::stringTripleVec optlistMap = battle->CustomBattleOptions()->getOptions( LSL::OptionsWrapper::MapOption );
    for (LSL::OptionsWrapper::stringTripleVec::const_iterator it = optlistMap.begin(); it != optlistMap.end(); ++it)
    {
        tdf.Append(it->first,it->second.second);
    }
    tdf.LeaveSection();


    tdf.EnterSection("modoptions");
    tdf.Append( "relayhoststartpostype", startpostype ); // also save the original wanted setting
    LSL::OptionsWrapper::stringTripleVec optlistMod = battle->CustomBattleOptions()->getOptions( LSL::OptionsWrapper::ModOption );
    for (LSL::OptionsWrapper::stringTripleVec::const_iterator it = optlistMod.begin(); it != optlistMod.end(); ++it)
    {
        tdf.Append(it->first,it->second.second);
    }
    tdf.LeaveSection();

    std::map<std::string,int> units = battle->RestrictedUnits();
    tdf.Append( "NumRestrictions", units.size());
    tdf.EnterSection( "RESTRICT" );
    int restrictcount = 0;
    for ( std::map<std::string, int>::const_iterator itor = units.begin(); itor != units.end(); ++itor )
    {
        tdf.Append("Unit" + Util::ToString( restrictcount ), itor->first );
        tdf.Append("Limit" + Util::ToString( restrictcount ), itor->second );
        restrictcount++;
    }
    tdf.LeaveSection();


    tdf.AppendLineBreak();

    if ( battle->IsProxy() )
    {
        tdf.Append( "NumPlayers", battle->GetNumPlayers() -1 );
        tdf.Append( "NumUsers", battle->GetNumUsers() -1 );
    }
    else
    {
        tdf.Append( "NumPlayers", battle->GetNumPlayers() );
        tdf.Append( "NumUsers", battle->GetNumUsers() );
    }
    tdf.AppendLineBreak();

    typedef std::map<int, int> ProgressiveTeamsVec;
    typedef ProgressiveTeamsVec::iterator ProgressiveTeamsVecIter;
    ProgressiveTeamsVec teams_to_sorted_teams; // original team -> progressive team
    int free_team = 0;
    std::map<const ConstCommonUserPtr, int> player_to_number; // player -> ordernumber
    srand ( time(NULL) );
    int i = 0;
    const unsigned int NumUsers = battle->Users().size();
    for( const ConstCommonUserPtr user: battle->Users() )
    {
        const UserBattleStatus& status = user->BattleStatus();
        if ( !status.spectator )
        {
            ProgressiveTeamsVecIter itor = teams_to_sorted_teams.find ( status.team );
            if ( itor == teams_to_sorted_teams.end() )
            {
                teams_to_sorted_teams[status.team] = free_team;
                free_team++;
            }
        }
        if ( battle->IsProxy() && ( user->Nick() == battle->GetFounder()->Nick() ) ) continue;
        if ( status.IsBot() ) continue;
        tdf.EnterSection( "PLAYER" + Util::ToString( i ) );
        tdf.Append( "Name", user->Nick() );
        tdf.Append( "CountryCode", BA::to_lower_copy( user->GetCountry() ) );
        tdf.Append( "Spectator", status.spectator );
        tdf.Append( "Rank", (int)user->GetRank() );
        tdf.Append( "IsFromDemo", int(status.isfromdemo) );
        if ( !status.scriptPassword.empty() )
        {
            tdf.Append( "Password", status.scriptPassword );
        }
        if ( !status.spectator )
        {
            tdf.Append( "Team", teams_to_sorted_teams[status.team] );
        }
        else
        {
            int speccteam = 0;
            if ( !teams_to_sorted_teams.empty() )
                speccteam = rand() % teams_to_sorted_teams.size();
            tdf.Append( "Team", speccteam );
        }
        tdf.LeaveSection();
        player_to_number[user] = i;
        i++;
    }

    if ( usync().VersionSupports( LSL::USYNC_GetSkirmishAI ) )
    {
        unsigned int i = 0;
        for( const ConstCommonUserPtr user: battle->Users() )
        {
            const UserBattleStatus& status = user->BattleStatus();
            if ( !status.IsBot() ) continue;
            tdf.EnterSection( "AI" + Util::ToString( i ) );
            tdf.Append( "Name", user->Nick() ); // AI's nick;
            tdf.Append( "ShortName", status.aishortname ); // AI libtype
            tdf.Append( "Version", status.aiversion ); // AI libtype version
            tdf.Append( "Team", teams_to_sorted_teams[status.team] );
            tdf.Append( "IsFromDemo", int(status.isfromdemo) );
            tdf.Append( "Host", player_to_number[battle->GetUser( status.owner )] );
            tdf.EnterSection( "Options" );
            int optionmapindex = battle->CustomBattleOptions()->GetAIOptionIndex( user->Nick() );
            if ( optionmapindex > 0 )
            {
                LSL::OptionsWrapper::stringTripleVec optlistMod_ = battle->CustomBattleOptions()->getOptions( (LSL::OptionsWrapper::GameOption)optionmapindex );
                for (LSL::OptionsWrapper::stringTripleVec::const_iterator it = optlistMod_.begin(); it != optlistMod_.end(); ++it)
                {
                    tdf.Append(it->first,it->second.second);
                }
            }
            tdf.LeaveSection();
            tdf.LeaveSection();
            player_to_number[user] = i;
            i++;
        }
    }

    tdf.AppendLineBreak();

    std::set<int> parsedteams;
    StringVector sides = usync().GetSides( battle->GetHostModName() );
    for( const ConstCommonUserPtr usr: battle->Users() )
    {
        const UserBattleStatus& status = usr->BattleStatus();
        if ( status.spectator ) continue;
        if ( parsedteams.find( status.team ) != parsedteams.end() ) continue; // skip duplicates
        parsedteams.insert( status.team );

        tdf.EnterSection( "TEAM" + Util::ToString( teams_to_sorted_teams[status.team] ) );
        if ( !usync().VersionSupports( LSL::USYNC_GetSkirmishAI ) && status.IsBot() )
        {
            tdf.Append( "AIDLL", status.aishortname );
            tdf.Append( "TeamLeader", player_to_number[battle->GetUser( status.owner )] ); // bot owner is the team leader
        }
        else
        {
            if ( status.IsBot() )
            {
                tdf.Append( "TeamLeader", player_to_number[battle->GetUser( status.owner )] );
            }
            else
            {
                tdf.Append( "TeamLeader", player_to_number[usr] );
            }
        }
        if ( battle->IsProxy() )
        {
            if ( startpostype == Enum::ST_Pick )
            {
                tdf.Append("StartPosX", status.pos.x );
                tdf.Append("StartPosZ", status.pos.y );
            }
            else if ( ( startpostype == Enum::ST_Fixed ) || ( startpostype == Enum::ST_Random ) )
            {
                int teamnumber = teams_to_sorted_teams[status.team];
                if ( teamnumber < int(remap_positions.size()) ) // don't overflow
                {
                    StartPos position = remap_positions[teamnumber];
                    tdf.Append("StartPosX", position.x );
                    tdf.Append("StartPosZ", position.y );
                }
            }
        }
        else
        {
            if ( startpostype == Enum::ST_Pick )
            {
                tdf.Append("StartPosX", status.pos.x );
                tdf.Append("StartPosZ", status.pos.y );
            }
        }

        tdf.Append( "AllyTeam",status.ally );

        std::string colorstring =
                Util::ToString( status.color.Red()/255.0 ) + ' ' +
                Util::ToString( status.color.Green()/255.0 ) + ' ' +
                Util::ToString( status.color.Blue()/255.0 );
        tdf.Append( "RGBColor", colorstring);

        unsigned int side = status.side;
        if ( side < sides.size() ) tdf.Append( "Side", sides[side] );
        tdf.Append( "Handicap", status.handicap );
        tdf.LeaveSection();
    }

    tdf.AppendLineBreak();

    unsigned int maxiter = std::max( NumUsers, battle->GetLastRectIdx() + 1 );
    std::set<int> parsedallys;
    for ( unsigned int i = 0; i < maxiter; i++ )
    {
        const ConstCommonUserPtr  usr = battle->Users()[i];
        const UserBattleStatus& status = usr->BattleStatus();
        Battle::BattleStartRect sr = battle->GetStartRect( i );
        if ( status.spectator && !sr.IsOk() )
            continue;
        int ally = status.ally;
        if ( status.spectator )
            ally = i;
        if ( parsedallys.find( ally ) != parsedallys.end() )
            continue; // skip duplicates
        sr = battle->GetStartRect( ally );
        parsedallys.insert( ally );

        tdf.EnterSection( "ALLYTEAM" + Util::ToString( ally ) );
        tdf.Append( "NumAllies", 0 );
        if ( startpostype == Enum::ST_Choose )
        {
            if ( sr.IsOk() )
            {
                const char* old_locale = std::setlocale(LC_NUMERIC, "C");

                tdf.Append( "StartRectLeft", boost::format( "%.3f" ) % ( sr.left / 200.0 ) );
                tdf.Append( "StartRectTop", boost::format( "%.3f" ) % ( sr.top / 200.0 ) );
                tdf.Append( "StartRectRight", boost::format( "%.3f" ) % ( sr.right / 200.0 ) );
                tdf.Append( "StartRectBottom", boost::format( "%.3f" ) % ( sr.bottom / 200.0 ) );

                std::setlocale(LC_NUMERIC, old_locale);
            }
        }
        tdf.LeaveSection();
    }

    tdf.LeaveSection();

    return ret.str();
}

} // namespace LSL {
