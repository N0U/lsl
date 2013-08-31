/* Copyright (C) 2007 The SpringLobby Team. All rights reserved. */

#include "unitsync.h"

#include <algorithm>
#include <cmath>
#include <stdexcept>
#include <clocale>
#include <set>
#include <fstream>
#include <boost/algorithm/string.hpp>
#include <boost/foreach.hpp>
#include <boost/format.hpp>
#include <iterator>

#include "c_api.h"
#include "image.h"

#include <lslutils/config.h>
#include <lslutils/debug.h>
#include <lslutils/conversion.h>
#include <lslutils/misc.h>
#include <lslutils/globalsmanager.h>

#define LOCK_UNITSYNC boost::mutex::scoped_lock lock_criticalsection(m_lock)

#define ASSERT_EXCEPTION(cond,msg) do { if (!(cond)) { LSL_THROW( unitsync, msg ); } } while (0)

namespace LSL {

Unitsync::Unitsync():
	m_cache_thread( NULL ),
	m_map_image_cache( 3, "m_map_image_cache" ),         // may take about 3M per image ( 1024x1024 24 bpp minimap )
	m_tiny_minimap_cache( 200, "m_tiny_minimap_cache" ), // takes at most 30k per image (   100x100 24 bpp minimap )
	m_mapinfo_cache( 1000000, "m_mapinfo_cache" ),       // this one is just misused as thread safe std::map ...
	m_sides_cache( 200, "m_sides_cache" )                // another misuse
{}


Unitsync::~Unitsync()
{
	delete m_cache_thread;
}

bool CompareStringNoCase(const std::string& first, const std::string& second)
{
	static std::locale l("C");
	static boost::is_iless il(l);
	try {
		//this fails on certain names
		return il(first,second);
	}
	catch(...) {
		return first < second;
	}
}

bool Unitsync::FastLoadUnitSyncLib( const std::string& unitsyncloc )
{
	LOCK_UNITSYNC;
	if (!_LoadUnitSyncLib( unitsyncloc ))
		return false;

	m_mods_list.clear();
	m_mod_array.clear();
	m_unsorted_mod_array.clear();
	m_mods_unchained_hash.clear();

	const int numMods = susynclib().GetPrimaryModCount();
	std::string name;
	for ( int i = 0; i < numMods; i++ )
	{
		try
		{
			name = susynclib().GetPrimaryModName( i );
			m_mods_list[name] = "fakehash";
			m_mod_array.push_back( name );
			m_shortname_to_name_map[
					std::make_pair(susynclib().GetPrimaryModShortName( i ),
								   susynclib().GetPrimaryModVersion( i )) ] = name;
		} catch (...) { continue; }
	}
	m_unsorted_mod_array = m_mod_array;
	return true;
}
bool Unitsync::FastLoadUnitSyncLibInit()
{
	LOCK_UNITSYNC;
	if (m_cache_thread == NULL) {
		m_cache_thread = new WorkerThread();
	}
	if ( IsLoaded() ) {
        m_cache_path = Util::config().GetCachePath().string();
		PopulateArchiveList();
	}
	return true;
}

bool Unitsync::LoadUnitSyncLib( const std::string& unitsyncloc )
{
	LOCK_UNITSYNC;
	if (m_cache_thread == NULL) {
		m_cache_thread = new WorkerThread();
	}
	bool ret = _LoadUnitSyncLib( unitsyncloc );
	if (ret)
	{
    m_cache_path = LSL::Util::config().GetCachePath().string();
		PopulateArchiveList();
    //this bit will be used to notify GUI listeners that they may need to update after usync reload
//    GetGlobalEventSender(GlobalEvents::OnUnitsyncReloaded).SendEvent( 0 );
	}
	return ret;
}

void Unitsync::PopulateArchiveList()
{
	m_maps_list.clear();
	m_mods_list.clear();
	m_mod_array.clear();
	m_map_array.clear();
	m_unsorted_mod_array.clear();
	m_unsorted_map_array.clear();
	m_map_image_cache.Clear();
	m_mapinfo_cache.Clear();
	m_maps_unchained_hash.clear();
	m_mods_unchained_hash.clear();
	m_shortname_to_name_map.clear();

	int numMaps = susynclib().GetMapCount();
	for ( int i = 0; i < numMaps; i++ )
	{
		std::string name, hash, archivename, unchainedhash;
		try
		{
			name = susynclib().GetMapName( i );
			hash = susynclib().GetMapChecksum( i );
			int count = susynclib().GetMapArchiveCount( i );
			if ( count > 0 )
			{
				archivename =  susynclib().GetMapArchiveName( 0 );
				unchainedhash = susynclib().GetArchiveChecksum( archivename );
			}
			//PrefetchMap( name ); // DEBUG
		} catch (...) { continue; }
		try
		{
			m_maps_list[name] = hash;
			if ( !unchainedhash.empty() ) m_maps_unchained_hash[name] = unchainedhash;
			if ( !archivename.empty() ) m_maps_archive_name[name] = archivename;
			assert(!name.empty());
			m_map_array.push_back( name );
		} catch (...)
		{
			LslError( "Found map with hash collision: %s hash: %s", name.c_str(), hash.c_str() );
		}
	}
	int numMods = susynclib().GetPrimaryModCount();
	for ( int i = 0; i < numMods; i++ )
	{
		std::string name, hash, archivename, unchainedhash;
		try
		{
			name = susynclib().GetPrimaryModName( i );
			hash = susynclib().GetPrimaryModChecksum( i );
			int count = susynclib().GetPrimaryModArchiveCount( i );
			if ( count > 0 )
			{
				archivename = susynclib().GetPrimaryModArchive( i );
				unchainedhash = susynclib().GetArchiveChecksum( archivename );
			}
		} catch (...) { continue; }
		try
		{
			m_mods_list[name] = hash;
			if ( !unchainedhash.empty() )  m_mods_unchained_hash[name] = unchainedhash;
			if ( !archivename.empty() ) m_mods_archive_name[name] = archivename;
			m_mod_array.push_back( name );
			m_shortname_to_name_map[
					std::make_pair(susynclib().GetPrimaryModShortName( i ),
								   susynclib().GetPrimaryModVersion( i )) ] = name;
		} catch (...)
		{
			LslError( "Found game with hash collision: %s hash: %s", name.c_str(), hash.c_str() );
		}
	}
	m_unsorted_mod_array = m_mod_array;
	m_unsorted_map_array = m_map_array;
	std::sort( m_map_array.begin(), m_map_array.end() , &CompareStringNoCase );
	std::sort( m_mod_array.begin(), m_mod_array.end() , &CompareStringNoCase  );
}



bool Unitsync::_LoadUnitSyncLib( const std::string& unitsyncloc )
{
	try {
        susynclib().Load( unitsyncloc, LSL::Util::config().GetForcedSpringConfigFilePath().string() );
	} catch (...) {
		return false;
	}
	return true;
}


void Unitsync::FreeUnitSyncLib()
{
	LOCK_UNITSYNC;

	susynclib().Unload();
}


bool Unitsync::IsLoaded() const
{
	return susynclib().IsLoaded();
}


std::string Unitsync::GetSpringVersion() const
{

	std::string ret;
	try
	{
		ret = susynclib().GetSpringVersion();
	}
	catch (...){}
	return ret;
}


bool Unitsync::VersionSupports( GameFeature feature ) const
{
	return susynclib().VersionSupports( feature );
}


int Unitsync::GetNumMods() const
{

	return m_mod_array.size();
}


StringVector Unitsync::GetModList() const
{
	return m_mod_array;
}


int Unitsync::GetModIndex( const std::string& name ) const
{
	return Util::IndexInSequence( m_mod_array, name );
}


bool Unitsync::ModExists( const std::string& modname ) const
{
	return (m_mods_list.find(modname) != m_mods_list.end());
}


bool Unitsync::ModExists( const std::string& modname, const std::string& hash ) const
{
	LocalArchivesVector::const_iterator itor = m_mods_list.find(modname);
	if ( itor == m_mods_list.end() ) return false;
	return itor->second == hash;
}

bool Unitsync::ModExistsCheckHash( const std::string& hash ) const
{
	LocalArchivesVector::const_iterator itor = m_mods_list.begin();
	for ( ; itor != m_mods_list.end(); ++itor ) {
		if ( itor->second == hash )
			return true;
	}
	return false;
}

UnitsyncMod Unitsync::GetMod( const std::string& modname )
{
	UnitsyncMod m;
	m.name = modname;
	m.hash = m_mods_list[modname];
	return m;
}


UnitsyncMod Unitsync::GetMod( int index )
{
	UnitsyncMod m;
	m.name = m_mod_array[index];
	m.hash = m_mods_list[m.name];
	return m;
}

int Unitsync::GetNumMaps() const
{
	return m_map_array.size();
}

StringVector Unitsync::GetMapList() const
{
	return m_map_array;
}

StringVector Unitsync::GetModValidMapList( const std::string& modname ) const
{
    StringVector ret;
	try {
		unsigned int mapcount = susynclib().GetValidMapCount( modname );
		for ( unsigned int i = 0; i < mapcount; i++ )
			ret.push_back( susynclib().GetValidMapName( i ) );
	} catch ( Exceptions::unitsync& e ) {}
	return ret;
}

bool Unitsync::MapExists( const std::string& mapname ) const
{
	return (m_maps_list.find(mapname) != m_maps_list.end());
}

bool Unitsync::MapExists( const std::string& mapname, const std::string& hash ) const
{
	LocalArchivesVector::const_iterator itor = m_maps_list.find(mapname);
	if ( itor == m_maps_list.end() ) return false;
	return itor->second == hash;
}

UnitsyncMap Unitsync::GetMap( const std::string& mapname )
{
	UnitsyncMap m;
	m.name = mapname;
	m.hash = m_maps_list[mapname];
	return m;
}

UnitsyncMap Unitsync::GetMap( int index )
{
	UnitsyncMap m;
	m.name = m_map_array[index];
	m.hash = m_maps_list[m.name];
	return m;
}

UnitsyncMap Unitsync::GetMapEx( int index )
{
	UnitsyncMap m;
	if ( index < 0 )
		return m;
	m.name = m_map_array[index];
	m.hash = m_maps_list[m.name];
	m.info = _GetMapInfoEx( m.name );
	return m;
}

void GetOptionEntry(const int i, GameOptions& ret)
{
	//all section values for options are converted to lower case
	//since usync returns the key of section type keys lower case
	//otherwise comapring would be a real hassle
	const std::string key = susynclib().GetOptionKey(i);
	const std::string name = susynclib().GetOptionName(i);
	const std::string section_str = boost::algorithm::to_lower_copy( susynclib().GetOptionSection(i) );
	switch (susynclib().GetOptionType(i))
	{
	case Enum::opt_float:
	{
		ret.float_map[key] = mmOptionFloat( name, key,
											susynclib().GetOptionDesc(i), susynclib().GetOptionNumberDef(i),
											susynclib().GetOptionNumberStep(i),
											susynclib().GetOptionNumberMin(i), susynclib().GetOptionNumberMax(i),
											section_str, susynclib().GetOptionStyle(i) );
		break;
	}
	case Enum::opt_bool:
	{
		ret.bool_map[key] = mmOptionBool( name, key,
										  susynclib().GetOptionDesc(i), susynclib().GetOptionBoolDef(i),
										  section_str, susynclib().GetOptionStyle(i) );
		break;
	}
	case Enum::opt_string:
	{
		ret.string_map[key] = mmOptionString( name, key,
											  susynclib().GetOptionDesc(i), susynclib().GetOptionStringDef(i),
											  susynclib().GetOptionStringMaxLen(i),
											  section_str, susynclib().GetOptionStyle(i) );
		break;
	}
	case Enum::opt_list:
	{
		ret.list_map[key] = mmOptionList(name,key,
										 susynclib().GetOptionDesc(i),susynclib().GetOptionListDef(i),
										 section_str,susynclib().GetOptionStyle(i));

		int listItemCount = susynclib().GetOptionListCount(i);
		for (int j = 0; j < listItemCount; ++j)
		{
			std::string descr = susynclib().GetOptionListItemDesc(i,j);
			ret.list_map[key].addItem(susynclib().GetOptionListItemKey(i,j),susynclib().GetOptionListItemName(i,j), descr);
		}
		break;
	}
	case Enum::opt_section:
	{
		ret.section_map[key] = mmOptionSection( name, key, susynclib().GetOptionDesc(i),
												section_str, susynclib().GetOptionStyle(i) );
	}
	}
}


GameOptions Unitsync::GetMapOptions( const std::string& name )
{
	GameOptions ret;
	int count = susynclib().GetMapOptionCount(name);
	for (int i = 0; i < count; ++i)
	{
		GetOptionEntry( i, ret );
	}
	return ret;
}

StringVector Unitsync::GetMapDeps( const std::string& mapname )
{
	StringVector ret;
	try
	{
		ret = susynclib().GetMapDeps( Util::IndexInSequence( m_unsorted_map_array, mapname ) );
	}
	catch( Exceptions::unitsync& u ) {}
	return ret;
}

UnitsyncMap Unitsync::GetMapEx( const std::string& mapname )
{
	const int i = GetMapIndex( mapname );
	if( i < 0 )
		LSL_THROW( unitsync, "Map does not exist");
	return GetMapEx( i );
}

int Unitsync::GetMapIndex( const std::string& name ) const
{
	return Util::IndexInSequence( m_map_array, name );
}

GameOptions Unitsync::GetModOptions( const std::string& name )
{
	GameOptions ret;
	int count = susynclib().GetModOptionCount(name);
	for (int i = 0; i < count; ++i)
	{
		GetOptionEntry(i, ret );
	}
	return ret;
}

GameOptions Unitsync::GetModCustomizations( const std::string& modname )
{
	GameOptions ret;
	int count = susynclib().GetCustomOptionCount( modname, "LobbyOptions.lua" );
	for (int i = 0; i < count; ++i) {
		GetOptionEntry(i, ret );
	}
	return ret;
}

GameOptions Unitsync::GetSkirmishOptions( const std::string& modname, const std::string& skirmish_name )
{
	GameOptions ret;
	int count = susynclib().GetCustomOptionCount( modname, skirmish_name );
	for (int i = 0; i < count; ++i) {
		GetOptionEntry(i, ret );
	}
	return ret;
}

StringVector Unitsync::GetModDeps( const std::string& modname ) const
{
    StringVector ret;
	try
	{
		ret = susynclib().GetModDeps( Util::IndexInSequence( m_unsorted_mod_array, modname ) );
	}
	catch( Exceptions::unitsync& u ) {}
	return ret;
}

StringVector Unitsync::GetSides( const std::string& modname )
{
    StringVector ret;
	if ( ! m_sides_cache.TryGet( modname, ret ) ) {
		try
		{
			ret = susynclib().GetSides( modname );
			m_sides_cache.Add( modname, ret );
		}
		catch( Exceptions::unitsync& u ) {}
	}
	return ret;
}


UnitsyncImage Unitsync::GetSidePicture( const std::string& modname, const std::string& SideName ) const
{
	std::string ImgName("SidePics");
	ImgName += "/";
	ImgName += boost::to_lower_copy( SideName );
	try {
		return GetImage( modname, ImgName + ".png", false );
	}
	catch ( Exceptions::unitsync& u ){}
	return GetImage( modname, ImgName + ".bmp", true );
}

UnitsyncImage Unitsync::GetImage( const std::string& modname, const std::string& image_path, bool useWhiteAsTransparent  ) const
{
	susynclib().SetCurrentMod( modname );
	int ini = susynclib().OpenFileVFS ( image_path );
	if( !ini )
		LSL_THROW( unitsync, "cannot find image");
	int FileSize = susynclib().FileSizeVFS(ini);
	if (FileSize == 0) {
		susynclib().CloseFileVFS(ini);
		LSL_THROW( unitsync, "image has size 0" );
	}
	Util::uninitialized_array<char> FileContent(FileSize);
	susynclib().ReadFileVFS(ini, FileContent, FileSize);
	return UnitsyncImage::FromVfsFileData( FileContent, FileSize, image_path, useWhiteAsTransparent );
}

StringVector Unitsync::GetAIList( const std::string& modname ) const
{
    StringVector ret;
	if ( usync().VersionSupports( USYNC_GetSkirmishAI ) )
	{
		int total = susynclib().GetSkirmishAICount( modname );
		for ( int i = 0; i < total; i++ )
		{
            StringVector infos = susynclib().GetAIInfo( i );
			const int namepos = Util::IndexInSequence( infos, "shortName");
			const int versionpos = Util::IndexInSequence( infos, "version");
			std::string ainame;
			if ( namepos != lslNotFound ) ainame += infos[namepos +1];
			if ( versionpos != lslNotFound ) ainame += " " + infos[versionpos +1];
			ret.push_back( ainame );
		}
	}
	else
	{
		// list dynamic link libraries
        const StringVector dlllist = susynclib().FindFilesVFS( Util::Lib::CanonicalizeName("AI/Bot-libs/*", Util::Lib::Module ) );
		for( int i = 0; i < long(dlllist.size()); i++ )
		{
			if ( Util::IndexInSequence( ret, Util::BeforeLast( dlllist[i], "/" ) ) == lslNotFound )
				ret.push_back ( dlllist[i] ); // don't add duplicates //TODO(koshi) make ret a set instead :)
		}
		// list jar files (java AIs)
        const StringVector jarlist = susynclib().FindFilesVFS("AI/Bot-libs/*.jar");
		for( int i = 0; i < long(jarlist.size()); i++ )
		{
			if ( Util::IndexInSequence( ret, Util::BeforeLast( jarlist[i], "/" ) ) == lslNotFound )
				ret.push_back ( jarlist[i] ); // don't add duplicates //TODO(koshi) make ret a set instead :)
		}
	}
	//std::sort(ret.begin(), ret.end());
	return ret;
}

void Unitsync::UnSetCurrentMod()
{
	try
	{
		susynclib().UnSetCurrentMod();
	} catch( std::runtime_error ) {}
}

StringVector Unitsync::GetAIInfos( int index ) const
{
    StringVector ret;
	try
	{
		ret = susynclib().GetAIInfo( index );
	}
	catch ( std::runtime_error ) {}
	return ret;
}

GameOptions Unitsync::GetAIOptions( const std::string& modname, int index )
{
	GameOptions ret;
	int count = susynclib().GetAIOptionCount(modname, index);
	for (int i = 0; i < count; ++i)
	{
		GetOptionEntry(i, ret );
	}
	return ret;
}

int Unitsync::GetNumUnits( const std::string& modname ) const
{
	susynclib().AddAllArchives( susynclib().GetPrimaryModArchive( Util::IndexInSequence( m_unsorted_mod_array, modname ) ) );
	susynclib().ProcessUnitsNoChecksum();
	return susynclib().GetUnitCount();
}

StringVector Unitsync::GetUnitsList( const std::string& modname )
{
	StringVector cache;
	try
	{
		cache = GetCacheFile( GetFileCachePath( modname, "", true ) + ".units" );
	} catch(...)
	{
		susynclib().SetCurrentMod( modname );
		while ( susynclib().ProcessUnitsNoChecksum() > 0 ) {}
		const int unitcount = susynclib().GetUnitCount();
		for ( int i = 0; i < unitcount; i++ )
		{
			cache.push_back( susynclib().GetFullUnitName(i) + " (" + susynclib().GetUnitName(i) + ")" );
		}
		SetCacheFile( GetFileCachePath( modname, "", true ) + ".units", cache );
	}
	return cache;
}

UnitsyncImage Unitsync::GetMinimap( const std::string& mapname )
{
	return _GetMapImage( mapname, ".minimap.png", &UnitsyncLib::GetMinimap );
}

UnitsyncImage Unitsync::GetMinimap( const std::string& mapname, int width, int height )
{
	const bool tiny = ( width <= 100 && height <= 100 );
	UnitsyncImage img;
	if ( tiny && m_tiny_minimap_cache.TryGet( mapname, img ) )
	{
		lslSize image_size = lslSize(img.GetWidth(), img.GetHeight()).MakeFit( lslSize(width, height) );
		if ( image_size.GetWidth() != img.GetWidth() || image_size.GetHeight() != img.GetHeight() )
			img.Rescale( image_size.GetWidth(), image_size.GetHeight() );

		return img;
	}

	img = GetMinimap( mapname );
	// special resizing code because minimap is always square,
	// and we need to resize it to the correct aspect ratio.
	if (img.GetWidth() > 1 && img.GetHeight() > 1)
	{
		try {
			MapInfo mapinfo = _GetMapInfoEx( mapname );

			lslSize image_size = lslSize(mapinfo.width, mapinfo.height).MakeFit( lslSize(width, height) );
			img.Rescale( image_size.GetWidth(), image_size.GetHeight() );
		}
		catch (...) {
			img = UnitsyncImage( 1, 1 );
		}
	}
	if ( tiny )
		m_tiny_minimap_cache.Add( mapname, img );
	return img;
}

UnitsyncImage Unitsync::GetMetalmap( const std::string& mapname )
{
	return _GetMapImage( mapname, ".metalmap.png", &UnitsyncLib::GetMetalmap );
}

UnitsyncImage Unitsync::GetMetalmap( const std::string& mapname, int width, int height )
{
	return _GetScaledMapImage( mapname, &Unitsync::GetMetalmap, width, height );
}

UnitsyncImage Unitsync::GetHeightmap( const std::string& mapname )
{
	return _GetMapImage( mapname, ".heightmap.png", &UnitsyncLib::GetHeightmap );
}

UnitsyncImage Unitsync::GetHeightmap( const std::string& mapname, int width, int height )
{
	return _GetScaledMapImage( mapname, &Unitsync::GetHeightmap, width, height );
}

UnitsyncImage Unitsync::_GetMapImage( const std::string& mapname, const std::string& imagename, UnitsyncImage (UnitsyncLib::*loadMethod)(const std::string&) )
{
	UnitsyncImage img;
	if ( m_map_image_cache.TryGet( mapname + imagename, img ) )
		return img;

	std::string originalsizepath = GetFileCachePath( mapname, m_maps_unchained_hash[mapname], false ) + imagename;
	try
	{
		img = UnitsyncImage( originalsizepath );
	}
	catch (...)
	{
		try
		{
			img = (susynclib().*loadMethod)( mapname );
			img.Save( originalsizepath );
		}
		catch (...)
		{
			img = UnitsyncImage( 1, 1 );
		}
	}
	m_map_image_cache.Add( mapname + imagename, img );
	return img;
}

UnitsyncImage Unitsync::_GetScaledMapImage( const std::string& mapname, UnitsyncImage (Unitsync::*loadMethod)(const std::string&), int width, int height )
{
	UnitsyncImage img = (this->*loadMethod) ( mapname );
	if (img.GetWidth() > 1 && img.GetHeight() > 1)
	{
		lslSize image_size = lslSize(img.GetWidth(), img.GetHeight()).MakeFit( lslSize(width, height) );
		img.Rescale( image_size.GetWidth(), image_size.GetHeight() );
	}
	return img;
}

MapInfo Unitsync::_GetMapInfoEx( const std::string& mapname )
{
	MapInfo info;
	if ( m_mapinfo_cache.TryGet( mapname, info ) )
		return info;

    StringVector cache;
	try {
		try
		{
			cache = GetCacheFile( GetFileCachePath( mapname, m_maps_unchained_hash[mapname], false ) + ".infoex" );

			ASSERT_EXCEPTION( cache.size() >= 11, "not enough lines found in cache info ex");
			info.author = cache[0];
			info.tidalStrength =  Util::FromString<long>( cache[1] );
			info.gravity = Util::FromString<long>( cache[2] );
			info.maxMetal = Util::FromString<double>( cache[3] );
			info.extractorRadius = Util::FromString<double>( cache[4] );
			info.minWind = Util::FromString<long>( cache[5] );
			info.maxWind = Util::FromString<long>( cache[6] );
			info.width = Util::FromString<long>( cache[7] );
			info.height = Util::FromString<long>( cache[8] );
            const StringVector posinfo = Util::StringTokenize( cache[9], " ");
			BOOST_FOREACH( const std::string pos, posinfo )
			{
				StartPos position;
				position.x = Util::FromString<long>( Util::BeforeFirst( pos,  "-" ) );
				position.y = Util::FromString<long>( Util::AfterFirst( pos, "-" ) );
				info.positions.push_back( position );
			}
			const unsigned int LineCount = cache.size();
			for ( unsigned int i = 10; i < LineCount; i++ )
				info.description += cache[i] + "\n";
		}
		catch (...)
		{
			const int index = Util::IndexInSequence( m_unsorted_map_array, mapname);
			ASSERT_EXCEPTION(index>=0, "Map not found");

			info = susynclib().GetMapInfoEx( index, 1 );

			cache.push_back ( info.author );
			cache.push_back( Util::ToString( info.tidalStrength ) );
			cache.push_back( Util::ToString( info.gravity ) );
			cache.push_back( Util::ToString( info.maxMetal ) );
			cache.push_back( Util::ToString( info.extractorRadius ) );
			cache.push_back( Util::ToString( info.minWind ) );
			cache.push_back( Util::ToString( info.maxWind )  );
			cache.push_back( Util::ToString( info.width ) );
			cache.push_back( Util::ToString( info.height ) );

			std::string postring;
			for ( unsigned int i = 0; i < info.positions.size(); i++)
			{
				postring += Util::ToString( info.positions[i].x ) + "-" + Util::ToString( info.positions[i].y ) + " ";
			}
			cache.push_back( postring );

			const StringVector descrtokens = Util::StringTokenize( info.description, "\n" );
			BOOST_FOREACH( const std::string descrtoken, descrtokens ) {
				cache.push_back( descrtoken );
			}
			SetCacheFile( GetFileCachePath( mapname, m_maps_unchained_hash[mapname], false ) + ".infoex", cache );
		}
	}
	catch ( ... ) {
		info.width = 1;
		info.height = 1;
	}

	m_mapinfo_cache.Add( mapname, info );

	return info;
}

StringVector Unitsync::FindFilesVFS( const std::string& pattern ) const
{
	return susynclib().FindFilesVFS( pattern );
}

bool Unitsync::ReloadUnitSyncLib()
{
    return LoadUnitSyncLib( LSL::Util::config().GetCurrentUsedUnitSync().string() );
}


void Unitsync::SetSpringDataPath( const std::string& path )
{
	susynclib().SetSpringConfigString( "SpringData", path );
}

bool Unitsync::GetSpringDataPath(std::string& path)
{
  path = susynclib().GetSpringDataDir();
  return true;
}

std::string Unitsync::GetFileCachePath( const std::string& name, const std::string& hash, bool IsMod )
{
	std::string ret = m_cache_path;
	if ( !name.empty() )
		ret += name;
	else
		return std::string();
	if ( !hash.empty() )
		ret += "-" + hash;
	else
	{
		if ( IsMod )
			ret += "-" + m_mods_list[name];
		else
		{
			ret += "-" + m_maps_list[name];
		}
	}
	return ret;
}

StringVector Unitsync::GetCacheFile( const std::string& path ) const
{
	StringVector ret;
	std::ifstream file( path.c_str() );
	ASSERT_EXCEPTION( file.good() , (boost::format( "cache file( %s ) not found" ) % path).str() );
	std::string line;
	while(std::getline(file,line))
	{
		ret.push_back( line );
	}
	return ret;
}

void Unitsync::SetCacheFile( const std::string& path, const StringVector& data )
{
	std::ofstream file( path.c_str(), std::ios::trunc );
	ASSERT_EXCEPTION( file.good() , (boost::format( "cache file( %s ) not found" ) % path).str() );
	BOOST_FOREACH( const std::string line, data )
	{
		file << line << std::endl;
	}
	file.flush();
	file.close();
}

StringVector  Unitsync::GetPlaybackList( bool ReplayType ) const
{
    StringVector ret;
	if ( !IsLoaded() ) return ret;

	if ( ReplayType )
		return susynclib().FindFilesVFS( "demos/*.sdf" );
	else
		return susynclib().FindFilesVFS( "Saves/*.ssf" );
}

bool Unitsync::FileExists( const std::string& name ) const
{
	int handle = susynclib().OpenFileVFS(name);
	if ( handle == 0 ) return false;
	susynclib().CloseFileVFS(handle);
	return true;
}

std::string Unitsync::GetArchivePath( const std::string& name ) const
{
	return susynclib().GetArchivePath( name );
}

StringVector Unitsync::GetScreenshotFilenames() const
{
	if ( !IsLoaded() )
        return StringVector();

    StringVector ret = susynclib().FindFilesVFS( "screenshots/*.*" );
	std::set<std::string> ret_set ( ret.begin(), ret.end() );
//	for ( int i = 0; i < long(ret.size() - 1); ++i ) {
//		if ( ret[i] == ret[i+1] )
//			ret.RemoveAt( i+1 );
//	}
    ret = StringVector ( ret_set.begin(), ret_set.end() );
	std::sort( ret.begin(), ret.end() );
	return ret;
}

std::string Unitsync::GetDefaultNick()
{
	std::string name = susynclib().GetSpringConfigString( "name", "Player" );
	if ( name.empty() ) {
		susynclib().SetSpringConfigString( "name", "Player" );
		return "Player";
	}
	return name;
}

void Unitsync::SetDefaultNick( const std::string& nick )
{
	susynclib().SetSpringConfigString( "name", nick );
}

////////////////////////////////////////////////////////////////////////////////
////////////////////////////// Unitsync prefetch/background thread code

namespace
{
typedef UnitsyncImage (Unitsync::*LoadMethodPtr)(const std::string&);
typedef UnitsyncImage (Unitsync::*ScaledLoadMethodPtr)(const std::string&, int, int);

class CacheMapWorkItem : public WorkItem
{
public:
	Unitsync* m_usync;
	std::string m_mapname;
	LoadMethodPtr m_loadMethod;

	void Run()
	{
		(m_usync->*m_loadMethod)( m_mapname );
	}

	CacheMapWorkItem( Unitsync* usync, const std::string& mapname, LoadMethodPtr loadMethod )
		: m_usync(usync), m_mapname(mapname.c_str()), m_loadMethod(loadMethod) {}
};

class CacheMinimapWorkItem : public WorkItem
{
public:
	std::string m_mapname;

	void Run()
	{
		// Fetch rescaled minimap using this specialized class instead of
		// CacheMapWorkItem with a pointer to Unitsync::GetMinimap,
		// to ensure Unitsync::_GetMapInfoEx will be called too, and
		// hence it's data cached.

		// This reduces main thread blocking while waiting for WorkerThread
		// to release it's lock while e.g. scrolling through battle list.

		// 98x98 because battle list map preview is 98x98
		usync().GetMinimap( m_mapname, 98, 98 );
	}

	CacheMinimapWorkItem( const std::string& mapname )
		: m_mapname(mapname.c_str()) {}
};

class GetMapImageAsyncResult : public WorkItem // TODO: rename
{
public:
	void Run()
	{
		try
		{
			RunCore();
		}
		catch (...)
		{
			// Event without mapname means some async job failed.
			// This is sufficient for now, we just need symmetry between
			// number of initiated async jobs and number of finished/failed
			// async jobs.
			m_mapname = std::string();
		}
		PostEvent();
	}

protected:
	Unitsync* m_usync;
	std::string m_mapname;
	int m_evtHandlerId;
	int m_evtId;

	void PostEvent()
	{
		m_usync->PostEvent( m_mapname );
	}

	virtual void RunCore() = 0;

	GetMapImageAsyncResult( Unitsync* usync, const std::string& mapname, int evtId ):
		m_usync(usync),
		m_mapname(mapname.c_str()),
		m_evtHandlerId(0),
		m_evtId(evtId)
	{}
};

class GetMapImageAsyncWorkItem : public GetMapImageAsyncResult
{
public:
	void RunCore()
	{
		(m_usync->*m_loadMethod)( m_mapname );
	}

	LoadMethodPtr m_loadMethod;

	GetMapImageAsyncWorkItem( Unitsync* usync, const std::string& mapname, LoadMethodPtr loadMethod )
		: GetMapImageAsyncResult( usync, mapname, 1 ), m_loadMethod(loadMethod) {}
};

class GetScaledMapImageAsyncWorkItem : public GetMapImageAsyncResult
{
public:
	void RunCore()
	{
		(m_usync->*m_loadMethod)( m_mapname, m_width, m_height );
	}

	int m_width;
	int m_height;
	ScaledLoadMethodPtr m_loadMethod;

	GetScaledMapImageAsyncWorkItem( Unitsync* usync, const std::string& mapname, int w, int h, ScaledLoadMethodPtr loadMethod )
		: GetMapImageAsyncResult( usync, mapname, 2 ), m_width(w), m_height(h), m_loadMethod(loadMethod) {}
};

class GetMapExAsyncWorkItem : public GetMapImageAsyncResult
{
public:
	void RunCore()
	{
		m_usync->GetMapEx( m_mapname );
	}

	GetMapExAsyncWorkItem( Unitsync* usync, const std::string& mapname )
		: GetMapImageAsyncResult( usync, mapname, 3 ) {}
};
}


void Unitsync::PrefetchMap( const std::string& mapname )
{
	// Use a simple hash based on 3 characters from the mapname
	// (without '.smf') as negative priority for the WorkItems.
	// This ensures WorkItems for the same map are put together,
	// which improves caching performance.

	// Measured improvement: 60% more cache hits while populating replay tab.
	// 50% hits without, 80% hits with this code.  (cache size 20 images)

	const int length = std::max(0, int(mapname.length()) - 4);
	const int hash = ( mapname[length * 1/4] << 16 )
			| ( mapname[length * 2/4] << 8  )
			| mapname[length * 3/4];
	const int priority = -hash;

	if (! m_cache_thread )
	{
		LslDebug( "cache thread not initialized %s", "PrefetchMap" );
		return;
	}
	{
		CacheMinimapWorkItem* work;

		work = new CacheMinimapWorkItem( mapname );
		m_cache_thread->DoWork( work, priority );
	}
	{
		CacheMapWorkItem* work;

		work = new CacheMapWorkItem( this, mapname, &Unitsync::GetMetalmap );
		m_cache_thread->DoWork( work, priority );

		work = new CacheMapWorkItem( this, mapname, &Unitsync::GetHeightmap );
		m_cache_thread->DoWork( work, priority );
	}
}

boost::signals2::connection Unitsync::RegisterEvtHandler( const StringSignalSlotType& handler )
{
    return m_async_ops_complete_sig.connect( handler );
}

void Unitsync::UnregisterEvtHandler(boost::signals2::connection &conn )
{
    conn.disconnect();
}

void Unitsync::PostEvent( const std::string& evt )
{
	m_async_ops_complete_sig( evt );
}

void Unitsync::_GetMapImageAsync( const std::string& mapname, UnitsyncImage (Unitsync::*loadMethod)(const std::string&) )
{
	if (! m_cache_thread )
	{
		LslDebug( "cache thread not initialised -- %s", mapname.c_str() );
		return;
	}
	GetMapImageAsyncWorkItem* work;
	work = new GetMapImageAsyncWorkItem( this, mapname, loadMethod );
	m_cache_thread->DoWork( work, 100 );
}

void Unitsync::GetMinimapAsync( const std::string& mapname )
{
	_GetMapImageAsync( mapname, &Unitsync::GetMinimap );
}

void Unitsync::GetMinimapAsync( const std::string& mapname, int width, int height )
{
	if (! m_cache_thread )
	{
		LslError( "cache thread not initialised" );
		return;
	}
	GetScaledMapImageAsyncWorkItem* work;
	work = new GetScaledMapImageAsyncWorkItem( this, mapname, width, height, &Unitsync::GetMinimap );
	m_cache_thread->DoWork( work, 100 );
}

void Unitsync::GetMetalmapAsync( const std::string& mapname )
{
	_GetMapImageAsync( mapname, &Unitsync::GetMetalmap );
}

void Unitsync::GetMetalmapAsync( const std::string& mapname, int /*width*/, int /*height*/ )
{
	GetMetalmapAsync( mapname );
}

void Unitsync::GetHeightmapAsync( const std::string& mapname )
{
	_GetMapImageAsync( mapname, &Unitsync::GetHeightmap );
}

void Unitsync::GetHeightmapAsync( const std::string& mapname, int /*width*/, int /*height*/ )
{
	GetHeightmapAsync( mapname );
}

void Unitsync::GetMapExAsync( const std::string& mapname )
{
	if (! m_cache_thread )
	{
		LslDebug( "cache thread not initialized %s", "GetMapExAsync" );
		return;
	}
	GetMapExAsyncWorkItem* work;
	work = new GetMapExAsyncWorkItem( this, mapname );
	m_cache_thread->DoWork( work, 200 /* higher prio then GetMinimapAsync */ );
}

std::string Unitsync::GetTextfileAsString( const std::string& modname, const std::string& file_path )
{
	susynclib().SetCurrentMod( modname );

	int ini = susynclib().OpenFileVFS ( file_path );
	if ( !ini )
		return std::string();
	int FileSize = susynclib().FileSizeVFS(ini);
	if (FileSize == 0) {
		susynclib().CloseFileVFS(ini);
		return std::string();
	}
	Util::uninitialized_array<char> FileContent(FileSize);
	susynclib().ReadFileVFS(ini, FileContent, FileSize);
	return std::string( FileContent, size_t( FileSize ) );
}

std::string Unitsync::GetNameForShortname( const std::string& shortname, const std::string& version) const
{
	ShortnameVersionToNameMap::const_iterator it
			=  m_shortname_to_name_map.find( std::make_pair(shortname,version) );
	if ( it != m_shortname_to_name_map.end() )
		return it->second;
	return std::string();
}

void Unitsync::AddReloadEvent()
{
  //previously this inserted a command into aa  queue that was worked on in
  //the main thread. direct reloading from any non-main thread would crash horribly
  ReloadUnitSyncLib();
}

Unitsync& usync() {
    static LSL::Util::LineInfo<Unitsync> m( AT );
    static LSL::Util::GlobalObjectHolder<Unitsync, LSL::Util::LineInfo<Unitsync> > m_sync( m );
	return m_sync;
}


} // namespace LSL
