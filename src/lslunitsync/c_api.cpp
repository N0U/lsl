/* Copyright (C) 2007 The SpringLobby Team. All rights reserved. */

#include "c_api.h"

#include <stdexcept>
#include <cmath>

#include <lslutils/logging.h>
#include <lslutils/misc.h>
#include <lslutils/globalsmanager.h>
#include <lslutils/debug.h>
#include <lslutils/conversion.h>

#include "image.h"
#include "loader.h"
#include "sharedlib.h"

#define UNITSYNC_EXCEPTION(cond,msg) do { if(!(cond))\
	LSL_THROW(unitsync,msg); } while(0)

#define CHECK_FUNCTION( arg ) \
	do { if ( !(arg) ) LSL_THROW( function_missing, "arg" ); } while (0)

#define LOCK_UNITSYNC boost::mutex::scoped_lock lock_criticalsection(m_lock)

//! Macro that checks if a function is present/loaded, unitsync is loaded, and locks it on call.
#define InitLib( arg ) \
	LOCK_UNITSYNC; \
	UNITSYNC_EXCEPTION( m_loaded, "Unitsync function not loaded:" #arg ); \
	CHECK_FUNCTION( arg );


namespace LSL {

UnitsyncLib::UnitsyncLib():
	m_loaded(false),
	m_libhandle(NULL),
	m_path(std::string()),
	m_init(NULL),
	m_uninit(NULL)
{
}


UnitsyncLib::~UnitsyncLib()
{
  Unload();
}

void UnitsyncLib::Load( const std::string& path)
{
	LOCK_UNITSYNC;
	_Load( path );
	_Init();
}


void UnitsyncLib::_Load( const std::string& path )
{
	assert(!path.empty());
	if ( _IsLoaded() && path == m_path ) return;

	_Unload();

	m_path = path;

	// Load the library.
	LslDebug( "Loading from: %s", path.c_str() );
	m_libhandle = _LoadLibrary(path);

	// Load all function from library.
	try {
		UnitsyncFunctionLoader::Basic( this );
		UnitsyncFunctionLoader::Map( this );
		UnitsyncFunctionLoader::Mod( this );
		UnitsyncFunctionLoader::Config( this );
		UnitsyncFunctionLoader::MMOptions( this );
		UnitsyncFunctionLoader::LuaParser( this );
		// only when we end up here unitsync was succesfully loaded.
		m_loaded = true;
	}
	catch ( std::exception& e )
	{
		// don't uninit unitsync in _Unload -- it hasn't been init'ed yet
		m_uninit = NULL;
		_Unload();
		LSL_THROW( unitsync, e.what() );
	}
}

void UnitsyncLib::_Init()
{
  if ( _IsLoaded() && m_init != NULL )
	{
		m_current_mod = std::string();
		m_init( true, 1 );
		auto errors = GetUnitsyncErrors();
		for(const std::string error: errors ) {
			LslError( "%s", error.c_str() );
		}
	}
}

void UnitsyncLib::_RemoveAllArchives()
{
	if (m_remove_all_archives)
		m_remove_all_archives();
	else
		_Init();
}

void UnitsyncLib::Unload()
{
	if ( !_IsLoaded() ) return;// dont even lock anything if unloaded.
	LOCK_UNITSYNC;
	_Unload();
}

void UnitsyncLib::_Unload()
{
	// as soon as we enter m_uninit unitsync technically isn't loaded anymore.
	m_loaded = false;

	m_path = std::string();

	// can't call UnSetCurrentMod() because it takes the unitsync lock
	m_current_mod = std::string();

	if (m_uninit)
		m_uninit();
	_FreeLibrary(m_libhandle);
	m_libhandle = NULL;
	m_init = NULL;
	m_uninit = NULL;
}

bool UnitsyncLib::IsLoaded() const
{
	return m_loaded;
}

bool UnitsyncLib::_IsLoaded() const
{
	return m_loaded;
}

void UnitsyncLib::AssertUnitsyncOk() const
{
	UNITSYNC_EXCEPTION( m_loaded, "Unitsync not loaded.");
	UNITSYNC_EXCEPTION( m_get_next_error, "Function was not in unitsync library.");
	UNITSYNC_EXCEPTION( false, m_get_next_error() );
}

std::vector<std::string> UnitsyncLib::GetUnitsyncErrors() const
{
	std::vector<std::string> ret;
	try
	{
		UNITSYNC_EXCEPTION( m_loaded, "Unitsync not loaded.");
		UNITSYNC_EXCEPTION( m_get_next_error, "Function was not in unitsync library.");

		const char* msg = m_get_next_error();
		while ( msg )
		{
			ret.push_back( msg );
			msg = m_get_next_error();
		}
		return ret;
	}
	catch ( std::runtime_error &e )
	{
		ret.push_back( e.what() );
		return ret;
	}
}

bool UnitsyncLib::VersionSupports( LSL::GameFeature feature ) const
{
	LOCK_UNITSYNC;
	switch (feature)
	{
		case LSL::USYNC_Sett_Handler: return m_set_spring_config_string;
		case LSL::USYNC_GetInfoMap:   return m_get_infomap_size;
		case LSL::USYNC_GetDataDir:   return m_get_writeable_data_dir;
		case LSL::USYNC_GetSkirmishAI:   return m_get_skirmish_ai_count;
		default: return false;
	}
}

void UnitsyncLib::_ConvertSpringMapInfo( const SpringMapInfo& in, MapInfo& out )
{
	out.author = in.author;
	out.description = in.description;
	out.extractorRadius = in.extractorRadius;
	out.gravity = in.gravity;
	out.tidalStrength = in.tidalStrength;
	out.maxMetal = in.maxMetal;
	out.minWind = in.minWind;
	out.maxWind = in.maxWind;
	out.width = in.width;
	out.height = in.height;
	out.positions = std::vector<StartPos>( in.positions, in.positions + in.posCount );
}

void UnitsyncLib::SetCurrentMod( const std::string& modname )
{
	InitLib( m_init ); // assumes the others are fine
	// (m_add_all_archives, m_get_mod_archive, m_get_mod_index)

	_SetCurrentMod( modname );
}

void UnitsyncLib::_SetCurrentMod( const std::string& modname )
{
	if ( m_current_mod != modname )
	{
		if ( !m_current_mod.empty() ) _RemoveAllArchives();
		m_add_all_archives( m_get_mod_archive( m_get_mod_index( modname.c_str() ) ) );
		m_current_mod = modname;
	}
}

void UnitsyncLib::UnSetCurrentMod( )
{
	LOCK_UNITSYNC;
	if ( !m_current_mod.empty() ) _RemoveAllArchives();
	m_current_mod = std::string();
}

int UnitsyncLib::GetModIndex( const std::string& name )
{
	return GetPrimaryModIndex( name );
}


//////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//  -- The UnitSync functions --
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////


std::string UnitsyncLib::GetSpringVersion()
{
	InitLib( m_get_spring_version );
	std::string version = m_get_spring_version();
	if (m_is_spring_release_version && m_get_spring_version_patchset && m_is_spring_release_version()) {
		version += ".";
		version += m_get_spring_version_patchset();
	}
	return version;
}

std::string UnitsyncLib::GetSpringDataDir()
{
	InitLib( m_get_writeable_data_dir );
	return m_get_writeable_data_dir();
}

int UnitsyncLib::GetSpringDataDirCount()
{
	InitLib( m_get_data_dir_count);
	return m_get_data_dir_count();
}

std::string UnitsyncLib::GetSpringDataDirByIndex( const int index )
{
	InitLib( m_get_data_dir_by_index );
	return m_get_data_dir_by_index( index );
}

std::string UnitsyncLib::GetConfigFilePath()
{
	InitLib( m_get_spring_config_file_path );
	return m_get_spring_config_file_path();
}

int UnitsyncLib::GetMapCount()
{
	InitLib( m_get_map_count );
	return m_get_map_count();
}

std::string UnitsyncLib::GetMapChecksum( int index )
{
	InitLib( m_get_map_checksum );
	return Util::ToString( (unsigned int)m_get_map_checksum( index ) );
}

std::string UnitsyncLib::GetMapName( int index )
{
	InitLib( m_get_map_name );
	return m_get_map_name( index );
}

int UnitsyncLib::GetMapArchiveCount( int index )
{
	InitLib( m_get_map_archive_count );
	return m_get_map_archive_count( m_get_map_name( index ) );
}

std::string UnitsyncLib::GetMapArchiveName( int arnr )
{
	InitLib( m_get_map_archive_name );
	return m_get_map_archive_name( arnr );
}

UnitsyncLib::StringVector UnitsyncLib::GetMapDeps( int index )
{
	int count = GetMapArchiveCount( index );
	StringVector ret;
	for ( int i = 0; i < count; i++ )
	{
		ret.push_back( GetMapArchiveName( i ) );
	}
	return ret;
}

MapInfo UnitsyncLib::GetMapInfoEx( int index, int version )
{
  if (m_get_map_description == NULL) {
		// old fetch method
		InitLib( m_get_map_info_ex );

		const std::string& mapName =  m_get_map_name( index );

		char tmpdesc[256];
		char tmpauth[256];

		MapInfo info;
		SpringMapInfo tm;
		tm.description = &tmpdesc[0];
		tm.author = &tmpauth[0];

		bool result = m_get_map_info_ex( mapName.c_str(), &tm, version );
		if (!result)
			LSL_THROW( unitsync, "Failed to get map infos");
		_ConvertSpringMapInfo( tm, info );
		return info;
	} else {
		// new fetch method
		InitLib( m_get_map_description )

		MapInfo info;
		const char* desc = m_get_map_description( index);
		if (desc == NULL)
			info.description="";
		else
			info.description = desc;
		info.tidalStrength = m_get_map_tidalStrength(index);
		info.gravity = m_get_map_gravity(index);

		const int resCount = m_get_map_resource_count(index);
		if (resCount > 0) {
			const int resourceIndex = 0;
			info.maxMetal = m_get_map_resource_max(index, resourceIndex);
			info.extractorRadius = m_get_map_resource_extractorRadius(index, resourceIndex);
		} else {
			info.maxMetal = 0.0f;
			info.extractorRadius = 0.0f;
		}

		info.minWind = m_get_map_windMin(index);
		info.maxWind = m_get_map_windMax(index);

		info.width = m_get_map_width(index);
		info.height = m_get_map_height(index);
		const int posCount = m_get_map_pos_count(index);
		for (int p = 0; p < posCount; ++p) {
			StartPos sp;
			sp.x = m_get_map_pos_x(index, p);
			sp.y = m_get_map_pos_z(index, p);
			info.positions.push_back(sp);
		}
		const char* author = m_get_map_author(index);
		if (author == NULL)
			info.author = "";
		else
			info.author = m_get_map_author(index);
		return info;
	}
}

UnitsyncImage UnitsyncLib::GetMinimap( const std::string& mapFileName )
{
	InitLib( m_get_minimap );
	const int miplevel = 1;  // miplevel should not be 10 ffs
	const int width  = 1024 >> miplevel;
	const int height = 1024 >> miplevel;
	// this unitsync call returns a pointer to a static buffer
	unsigned short* colors = (unsigned short*)m_get_minimap( mapFileName.c_str(), miplevel );
	if (!colors)
		LSL_THROW( unitsync, "Get minimap failed");
	UnitsyncImage img = UnitsyncImage::FromMinimapData( colors, width, height );
	img.RescaleIfBigger();
	return img;
}

UnitsyncImage UnitsyncLib::GetMetalmap( const std::string& mapFileName )
{
	InitLib( m_get_infomap_size ); // assume GetInfoMap is available too
	int width = 0, height = 0, retval;
	retval = m_get_infomap_size(mapFileName.c_str(), "metal", &width, &height);
	if ( !(retval != 0 && width * height != 0) )
		LSL_THROW( unitsync, "Get metalmap size failed");
	Util::uninitialized_array<unsigned char> grayscale(width * height);
	retval = m_get_infomap(mapFileName.c_str(), "metal", grayscale, 1 /*byte per pixel*/);
	if ( retval == 0 )
		LSL_THROW( unitsync, "Get metalmap failed");
	UnitsyncImage img = UnitsyncImage::FromMetalmapData(grayscale, width, height);
	img.RescaleIfBigger();
	return img;
}

UnitsyncImage UnitsyncLib::GetHeightmap( const std::string& mapFileName )
{
	InitLib( m_get_infomap_size ); // assume GetInfoMap is available too
	int width = 0, height = 0, retval;
	retval = m_get_infomap_size(mapFileName.c_str(), "height", &width, &height);
	if ( !(retval != 0 && width * height != 0) )
		LSL_THROW( unitsync, "Get heightmap size failed");
	Util::uninitialized_array<unsigned short> grayscale(width * height);
	retval = m_get_infomap(mapFileName.c_str(), "height", grayscale, 2 /*byte per pixel*/);
	if ( retval == 0 )
		LSL_THROW( unitsync, "Get heightmap failed");
	UnitsyncImage img = UnitsyncImage::FromHeightmapData( grayscale, width, height );
	img.RescaleIfBigger();
	return img;
}

std::string UnitsyncLib::GetPrimaryModChecksum( int index )
{
	InitLib( m_get_mod_checksum );
	return Util::ToString( (unsigned int)m_get_mod_checksum( index ) );
}

int UnitsyncLib::GetPrimaryModIndex( const std::string& modName )
{
	InitLib( m_get_mod_index );
	return m_get_mod_index( modName.c_str() );
}

std::string UnitsyncLib::GetPrimaryModName( int index )
{
	InitLib( m_get_mod_name );
	return m_get_mod_name( index );
}

int UnitsyncLib::GetPrimaryModCount()
{
	InitLib( m_get_mod_count );
	return m_get_mod_count();
}

std::string UnitsyncLib::GetPrimaryModArchive( int index )
{
	InitLib( m_get_mod_archive );
	if (!m_get_mod_count)
		LSL_THROW( unitsync, "Function was not in unitsync library.");
	int count = m_get_mod_count();
	if (index >= count)
		LSL_THROW( unitsync, "index out of bounds");
	return m_get_mod_archive( index );
}

std::string UnitsyncLib::GetPrimaryModShortName( int index )
{
	InitLib( m_get_primary_mod_short_name );
	return m_get_primary_mod_short_name( index );
}

std::string UnitsyncLib::GetPrimaryModVersion( int index )
{
	InitLib( m_get_primary_mod_version );
	return m_get_primary_mod_version( index );
}

std::string UnitsyncLib::GetPrimaryModMutator( int index )
{
	InitLib( m_get_primary_mod_mutator );
	return m_get_primary_mod_mutator( index );
}

std::string UnitsyncLib::GetPrimaryModGame( int index )
{
	InitLib( m_get_primary_mod_game );
	return m_get_primary_mod_game( index );
}

std::string UnitsyncLib::GetPrimaryModShortGame( int index )
{
	InitLib( m_get_primary_mod_short_game );
	return m_get_primary_mod_short_game( index );
}

std::string UnitsyncLib::GetPrimaryModDescription( int index )
{
	InitLib( m_get_primary_mod_description );
	return m_get_primary_mod_description( index );
}

int UnitsyncLib::GetPrimaryModArchiveCount( int index )
{
	InitLib( m_get_primary_mod_archive_count );
	return m_get_primary_mod_archive_count( index );
}

std::string UnitsyncLib::GetPrimaryModArchiveList( int arnr )
{
	InitLib( m_get_primary_mod_archive_list );
	return m_get_primary_mod_archive_list( arnr );
}

std::string UnitsyncLib::GetPrimaryModChecksumFromName( const std::string& name )
{
	InitLib( m_get_primary_mod_checksum_from_name );
	return Util::ToString( (unsigned int)m_get_primary_mod_checksum_from_name( name.c_str() ) );
}

UnitsyncLib::StringVector UnitsyncLib::GetModDeps( int index )
{
	int count = GetPrimaryModArchiveCount( index );
	StringVector ret;
	for ( int i = 0; i < count; i++ )
		ret.push_back( GetPrimaryModArchiveList( i ) );
	return ret;
}

UnitsyncLib::StringVector UnitsyncLib::GetSides( const std::string& modName )
{
	InitLib( m_get_side_count );
	if (!m_get_side_name)
		LSL_THROW( function_missing, "m_get_side_name");
	_SetCurrentMod( modName );
	int count = m_get_side_count();
	StringVector ret;
	for ( int i = 0; i < count; i ++ )
		ret.push_back( m_get_side_name( i ) );
	return ret;
}

void UnitsyncLib::AddAllArchives( const std::string& root )
{
	InitLib( m_add_all_archives );
    m_add_all_archives( root.c_str() );
}

void UnitsyncLib::AddArchive(const std::string &name)
{
	InitLib( m_add_archive);
	m_add_archive(name.c_str());
}

std::string UnitsyncLib::GetFullUnitName( int index )
{
	InitLib( m_get_unit_full_name );
	return m_get_unit_full_name( index );
}

std::string UnitsyncLib::GetUnitName( int index )
{
	InitLib( m_get_unit_name );
	return m_get_unit_name( index );
}

int UnitsyncLib::GetUnitCount()
{
	InitLib( m_get_unit_count );
	return m_get_unit_count();
}

int UnitsyncLib::ProcessUnitsNoChecksum()
{
	InitLib( m_proc_units_nocheck );
	return m_proc_units_nocheck();
}

UnitsyncLib::StringVector UnitsyncLib::FindFilesVFS( const std::string& name )
{
	InitLib( m_find_files_vfs );
	CHECK_FUNCTION( m_init_find_vfs );
	int handle = m_init_find_vfs( name.c_str() );
	StringVector ret;
	//thanks to assbars awesome edit we now get different invalid values from init and find
	if ( handle != -1 ) {
		do
		{
			char buffer[1025];
			handle = m_find_files_vfs( handle, &buffer[0], 1024 );
			buffer[1024] = 0;
			ret.push_back( &buffer[0] );
		}while ( handle );
	}
	return ret;
}

int UnitsyncLib::OpenFileVFS( const std::string& name )
{
	InitLib( m_open_file_vfs );
	return m_open_file_vfs( name.c_str() );
}

int UnitsyncLib::FileSizeVFS( int handle )
{
	InitLib( m_file_size_vfs );
	return m_file_size_vfs( handle );
}

int UnitsyncLib::ReadFileVFS( int handle, void* buffer, int bufferLength )
{
	InitLib( m_read_file_vfs );
	return m_read_file_vfs( handle, buffer, bufferLength );
}

void UnitsyncLib::CloseFileVFS( int handle )
{
	InitLib( m_close_file_vfs );
	m_close_file_vfs( handle );
}

unsigned int UnitsyncLib::GetValidMapCount( const std::string& modname )
{
	InitLib( m_get_mod_valid_map_count );
	_SetCurrentMod( modname );
	return m_get_mod_valid_map_count();
}

std::string UnitsyncLib::GetValidMapName( unsigned int MapIndex )
{
	InitLib( m_get_valid_map );
	return m_get_valid_map( MapIndex );
}

int UnitsyncLib::GetMapOptionCount( const std::string& name )
{
	InitLib( m_get_map_option_count );
	if (name.empty())
		LSL_THROW( unitsync, "tried to pass empty mapname to unitsync");
	return m_get_map_option_count( name.c_str() );
}

int UnitsyncLib::GetCustomOptionCount( const std::string& archive_name, const std::string& filename )
{
	InitLib( m_get_custom_option_count );
	if (archive_name.empty())
		LSL_THROW( unitsync, "tried to pass empty archive_name to unitsync");
	_RemoveAllArchives();
	m_add_all_archives( archive_name.c_str() );
	return m_get_custom_option_count( filename.c_str() );
}

int UnitsyncLib::GetModOptionCount( const std::string& name )
{
	InitLib( m_get_mod_option_count );
	if (name.empty())
		LSL_THROW( unitsync, "tried to pass empty modname to unitsync");
	_SetCurrentMod( name );
	return m_get_mod_option_count();
}

int UnitsyncLib::GetAIOptionCount( const std::string& modname, int aiIndex )
{
	InitLib( m_get_skirmish_ai_option_count );
	_SetCurrentMod( modname );
	CHECK_FUNCTION( m_get_skirmish_ai_count );
	if ( !(( aiIndex >= 0 ) && ( aiIndex < m_get_skirmish_ai_count() )) )
		LSL_THROW( unitsync, "aiIndex out of bounds");
	return m_get_skirmish_ai_option_count( aiIndex );
}

std::string UnitsyncLib::GetOptionKey( int optIndex )
{
	InitLib( m_get_option_key );
	return m_get_option_key( optIndex );
}

std::string UnitsyncLib::GetOptionName( int optIndex )
{
	InitLib( m_get_option_name );
	return m_get_option_name( optIndex );
}

std::string UnitsyncLib::GetOptionDesc( int optIndex )
{
	InitLib( m_get_option_desc );
	return m_get_option_desc( optIndex );
}

std::string UnitsyncLib::GetOptionSection( int optIndex )
{
	InitLib( m_get_option_section );
	return m_get_option_section( optIndex );
}

std::string UnitsyncLib::GetOptionStyle( int optIndex )
{
	InitLib( m_get_option_style );
	return m_get_option_style( optIndex );
}

int UnitsyncLib::GetOptionType( int optIndex )
{
	InitLib( m_get_option_type );
	return m_get_option_type( optIndex );
}

int UnitsyncLib::GetOptionBoolDef( int optIndex )
{
	InitLib( m_get_option_bool_def );
	return m_get_option_bool_def( optIndex );
}

float UnitsyncLib::GetOptionNumberDef( int optIndex )
{
	InitLib( m_get_option_number_def );
	return m_get_option_number_def( optIndex );
}

float UnitsyncLib::GetOptionNumberMin( int optIndex )
{
	InitLib( m_get_option_number_min );
	return m_get_option_number_min( optIndex );
}

float UnitsyncLib::GetOptionNumberMax( int optIndex )
{
	InitLib( m_get_option_number_max );
	return m_get_option_number_max( optIndex );
}

float UnitsyncLib::GetOptionNumberStep( int optIndex )
{
	InitLib( m_get_option_number_step );
	return m_get_option_number_step( optIndex );
}

std::string UnitsyncLib::GetOptionStringDef( int optIndex )
{
	InitLib( m_get_option_string_def );
	return m_get_option_string_def( optIndex );
}

int UnitsyncLib::GetOptionStringMaxLen( int optIndex )
{
	InitLib( m_get_option_string_max_len );
	return m_get_option_string_max_len( optIndex );
}

int UnitsyncLib::GetOptionListCount( int optIndex )
{
	InitLib( m_get_option_list_count );
	return m_get_option_list_count( optIndex );
}

std::string UnitsyncLib::GetOptionListDef( int optIndex )
{
	InitLib( m_get_option_list_def );
	return m_get_option_list_def( optIndex );
}

std::string UnitsyncLib::GetOptionListItemKey( int optIndex, int itemIndex )
{
	InitLib( m_get_option_list_item_key );
	return m_get_option_list_item_key( optIndex, itemIndex  );
}

std::string UnitsyncLib::GetOptionListItemName( int optIndex, int itemIndex )
{
	InitLib( m_get_option_list_item_name );
	return m_get_option_list_item_name( optIndex, itemIndex  );
}

std::string UnitsyncLib::GetOptionListItemDesc( int optIndex, int itemIndex )
{
	InitLib( m_get_option_list_item_desc );
	return m_get_option_list_item_desc( optIndex, itemIndex  );
}

int UnitsyncLib::OpenArchive( const std::string& name )
{
	InitLib( m_open_archive );
	return m_open_archive( name.c_str() );
}

void UnitsyncLib::CloseArchive( int archive )
{
	InitLib( m_close_archive );
	m_close_archive( archive );
}

int UnitsyncLib::FindFilesArchive( int archive, int cur, std::string& nameBuf )
{
	InitLib( m_find_Files_archive );
	char buffer[1025];
	int size = 1024;
	bool ret = m_find_Files_archive( archive, cur, &buffer[0], &size );
	buffer[1024] = 0;
	nameBuf = &buffer[0];
	return ret;
}

int UnitsyncLib::OpenArchiveFile( int archive, const std::string& name )
{
	InitLib( m_open_archive_file );
	return m_open_archive_file( archive, name.c_str() );
}

int UnitsyncLib::ReadArchiveFile( int archive, int handle, void* buffer, int numBytes)
{
	InitLib( m_read_archive_file );
	return m_read_archive_file( archive, handle, buffer, numBytes );
}

void UnitsyncLib::CloseArchiveFile( int archive, int handle )
{
	InitLib( m_close_archive_file );
	m_close_archive_file( archive, handle );
}

int UnitsyncLib::SizeArchiveFile( int archive, int handle )
{
	InitLib( m_size_archive_file );
	return m_size_archive_file( archive, handle );
}

std::string UnitsyncLib::GetArchivePath( const std::string& name )
{
	InitLib( m_get_archive_path );
	return m_get_archive_path( name.c_str() );
}

int UnitsyncLib::GetSpringConfigInt( const std::string& key, int defValue )
{
	InitLib( m_get_spring_config_int );
	return m_get_spring_config_int( key.c_str(), defValue );
}

std::string UnitsyncLib::GetSpringConfigString( const std::string& key, const std::string& defValue )
{
	InitLib( m_get_spring_config_string );
	return m_get_spring_config_string( key.c_str(), defValue.c_str() );
}

float UnitsyncLib::GetSpringConfigFloat( const std::string& key, const float defValue )
{
	InitLib( m_get_spring_config_float );
	return m_get_spring_config_float( key.c_str(), defValue );
}

void UnitsyncLib::SetSpringConfigString( const std::string& key, const std::string& value )
{
	InitLib( m_set_spring_config_string );
	m_set_spring_config_string( key.c_str(), value.c_str() );
}

void UnitsyncLib::SetSpringConfigInt( const std::string& key, int value )
{
	InitLib( m_set_spring_config_int );
	m_set_spring_config_int( key.c_str(), value );
}


void UnitsyncLib::SetSpringConfigFloat( const std::string& key, const float value )
{
	InitLib( m_set_spring_config_float );

	m_set_spring_config_float( key.c_str(), value );
}

int UnitsyncLib::GetSkirmishAICount( const std::string& modname )
{
	InitLib( m_get_skirmish_ai_count );
	_SetCurrentMod( modname );
	return m_get_skirmish_ai_count();
}

UnitsyncLib::StringVector UnitsyncLib::GetAIInfo( int aiIndex )
{
	InitLib( m_get_skirmish_ai_count );
	CHECK_FUNCTION( m_get_skirmish_ai_info_count );
	CHECK_FUNCTION( m_get_skirmish_ai_info_description );
	CHECK_FUNCTION( m_get_skirmish_ai_info_key );
	CHECK_FUNCTION( m_get_skirmish_ai_info_value );

	StringVector ret;
	if ( !(( aiIndex >= 0 ) && ( aiIndex < m_get_skirmish_ai_count() )) )
		LSL_THROW( unitsync, "aiIndex out of bounds");

	int infoCount = m_get_skirmish_ai_info_count( aiIndex );
	for( int i = 0; i < infoCount; i++ )
	{
		ret.push_back( m_get_skirmish_ai_info_key( i ) );
		ret.push_back( m_get_skirmish_ai_info_value( i ) );
		ret.push_back( m_get_skirmish_ai_info_description( i ) );
	}
	return ret;
}

std::string UnitsyncLib::GetArchiveChecksum( const std::string& VFSPath )
{
	InitLib( m_get_archive_checksum );
	return Util::ToString( m_get_archive_checksum( VFSPath.c_str() ) );
}

/// lua parser

void UnitsyncLib::CloseParser()
{
	InitLib( m_parser_close );
	m_parser_close();
}

bool UnitsyncLib::OpenParserFile( const std::string& filename, const std::string& filemodes, const std::string& accessModes )
{
	InitLib( m_parser_open_file );
	return m_parser_open_file( filename.c_str(), filemodes.c_str(), accessModes.c_str() );
}

bool UnitsyncLib::OpenParserSource( const std::string& source, const std::string& accessModes )
{
	InitLib( m_parser_open_source );
	return m_parser_open_source( source.c_str(), accessModes.c_str() );
}

bool UnitsyncLib::ParserExecute()
{
	InitLib( m_parser_execute );
	return m_parser_execute();
}

std::string UnitsyncLib::ParserErrorLog()
{
	InitLib( m_parser_error_log );
	return m_parser_error_log();
}

void UnitsyncLib::ParserAddTable( int key, bool override )
{
	InitLib( m_parser_add_table_int );
	m_parser_add_table_int( key, override );
}

void UnitsyncLib::ParserAddTable( const std::string& key, bool override )
{
	InitLib( m_parser_add_table_string );
	m_parser_add_table_string( key.c_str(), override );
}

void UnitsyncLib::ParserEndTable()
{
	InitLib( m_parser_end_table );
	m_parser_end_table();
}

void UnitsyncLib::ParserAddTableValue( int key, int val )
{
	InitLib( m_parser_add_int_key_int_value );
	m_parser_add_int_key_int_value( key, val );
}

void UnitsyncLib::ParserAddTableValue( const std::string& key, int val )
{
	InitLib( m_parser_add_string_key_int_value );
	m_parser_add_string_key_int_value( key.c_str(), val );
}

void UnitsyncLib::ParserAddTableValue( int key, bool val )
{
	InitLib( m_parser_add_int_key_int_value );
	m_parser_add_int_key_int_value( key, val );
}

void UnitsyncLib::ParserAddTableValue( const std::string& key, bool val )
{
	InitLib( m_parser_add_string_key_int_value );
	m_parser_add_string_key_int_value( key.c_str(), val );
}

void UnitsyncLib::ParserAddTableValue( int key, const std::string& val )
{
	InitLib( m_parser_add_int_key_string_value );
	m_parser_add_int_key_string_value( key, val.c_str() );
}

void UnitsyncLib::ParserAddTableValue( const std::string& key, const std::string& val )
{
	InitLib( m_parser_add_string_key_string_value );
	m_parser_add_string_key_string_value( key.c_str(), val.c_str() );
}

void UnitsyncLib::ParserAddTableValue( int key, float val )
{
	InitLib( m_parser_add_int_key_float_value );
	m_parser_add_int_key_float_value( key, val );
}

void UnitsyncLib::ParserAddTableValue( const std::string& key, float val )
{
	InitLib( m_parser_add_string_key_float_value );
	m_parser_add_string_key_float_value( key.c_str(), val );
}

bool UnitsyncLib::ParserGetRootTable()
{
	InitLib( m_parser_root_table );
	return m_parser_root_table();
}

bool UnitsyncLib::ParserGetRootTableExpression( const std::string& exp )
{
	InitLib( m_parser_root_table_expression );
	return m_parser_root_table_expression( exp.c_str() );
}

bool UnitsyncLib::ParserGetSubTableInt( int key )
{
	InitLib( m_parser_sub_table_int );
	return m_parser_sub_table_int( key );
}

bool UnitsyncLib::ParserGetSubTableString( const std::string& key )
{
	InitLib( m_parser_sub_table_string );
	return m_parser_sub_table_string( key.c_str() );
}

bool UnitsyncLib::ParserGetSubTableInt( const std::string& exp )
{
	InitLib( m_parser_sub_table_expression );
	return m_parser_sub_table_expression( exp.c_str() );
}

void UnitsyncLib::ParserPopTable()
{
	InitLib( m_parser_pop_table );
	m_parser_pop_table();
}

bool UnitsyncLib::ParserKeyExists( int key )
{
	InitLib( m_parser_key_int_exists );
	return m_parser_key_int_exists( key );
}

bool UnitsyncLib::ParserKeyExists( const std::string& key )
{
	InitLib( m_parser_key_string_exists );
	return m_parser_key_string_exists( key.c_str() );
}

int UnitsyncLib::ParserGetKeyType( int key )
{
	InitLib( m_parser_int_key_get_type );
	return m_parser_int_key_get_type( key );
}

int UnitsyncLib::ParserGetKeyType( const std::string& key )
{
	InitLib( m_parser_string_key_get_type );
	return m_parser_string_key_get_type( key.c_str() );
}

int UnitsyncLib::ParserGetIntKeyListCount()
{
	InitLib( m_parser_int_key_get_list_count );
	return m_parser_int_key_get_list_count();
}

int UnitsyncLib::ParserGetIntKeyListEntry( int index )
{
	InitLib( m_parser_int_key_get_list_entry );
	return m_parser_int_key_get_list_entry( index );
}

int UnitsyncLib::ParserGetStringKeyListCount()
{
	InitLib( m_parser_string_key_get_list_count );
	return m_parser_string_key_get_list_count();
}

int UnitsyncLib::ParserGetStringKeyListEntry( int index )
{
	InitLib( m_parser_int_key_get_list_entry );
	return m_parser_int_key_get_list_entry( index );
}

int UnitsyncLib::GetKeyValue( int key, int defval )
{
	InitLib( m_parser_int_key_get_int_value );
	return m_parser_int_key_get_int_value( key, defval );
}

bool UnitsyncLib::GetKeyValue( int key, bool defval )
{
	InitLib( m_parser_int_key_get_bool_value );
	return m_parser_int_key_get_bool_value( key, defval );
}

std::string UnitsyncLib::GetKeyValue( int key, const std::string& defval )
{
	InitLib( m_parser_int_key_get_string_value );
	return m_parser_int_key_get_string_value( key, defval.c_str() );
}

float UnitsyncLib::GetKeyValue( int key, float defval )
{
	InitLib( m_parser_int_key_get_float_value );
	return m_parser_int_key_get_float_value( key, defval );
}

int UnitsyncLib::GetKeyValue( const std::string& key, int defval )
{
	InitLib( m_parser_string_key_get_int_value );
	return m_parser_string_key_get_int_value( key.c_str(), defval );
}

bool UnitsyncLib::GetKeyValue( const std::string& key, bool defval )
{
	InitLib( m_parser_string_key_get_bool_value );
	return m_parser_string_key_get_bool_value( key.c_str(), defval );
}

std::string UnitsyncLib::GetKeyValue( const std::string& key, const std::string& defval )
{
	InitLib( m_parser_string_key_get_string_value );
	return m_parser_string_key_get_string_value( key.c_str(), defval.c_str() );
}

float UnitsyncLib::GetKeyValue( const std::string& key, float defval )
{
	InitLib( m_parser_string_key_get_float_value );
	return m_parser_string_key_get_float_value( key.c_str(), defval );
}

UnitsyncLib& susynclib()
{
	static LSL::Util::LineInfo<UnitsyncLib> m( AT );
	static LSL::Util::GlobalObjectHolder<UnitsyncLib, LSL::Util::LineInfo<UnitsyncLib> > ss( m );
	return ss;
}

} //namespace LSL
