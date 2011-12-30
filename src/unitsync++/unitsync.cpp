/* Copyright (C) 2007 The SpringLobby Team. All rights reserved. */

#include "unitsync.h"

#include <algorithm>
#include <cmath>
#include <stdexcept>
#include <clocale>

#include "c_api.h"
#include "utils/debug.h"
#include "utils/conversion.h"
#include "utils/misc.h"

#define LOCK_UNITSYNC boost::mutex::scoped_lock lock_criticalsection(m_lock)

const wxEventType UnitSyncAsyncOperationCompletedEvt = wxNewEventType();
const wxEventType wxUnitsyncReloadEvent = wxNewEventType();

namespace LSL {

SpringUnitSync& usync()
{
	static LineInfo<SpringUnitSync> m( AT );
	static GlobalObjectHolder<SpringUnitSync, LineInfo<SpringUnitSync> > m_sync( m );
	return m_sync;
}


SpringUnitSync::SpringUnitSync()
	: m_cache_thread( NULL )
	, m_map_image_cache( 3, _T("m_map_image_cache") )         // may take about 3M per image ( 1024x1024 24 bpp minimap )
	, m_tiny_minimap_cache( 200, _T("m_tiny_minimap_cache") ) // takes at most 30k per image (   100x100 24 bpp minimap )
	, m_mapinfo_cache( 1000000, _T("m_mapinfo_cache") )       // this one is just misused as thread safe std::map ...
	, m_sides_cache( 200, _T("m_sides_cache") )               // another misuse
{
	Connect( wxUnitsyncReloadEvent, wxCommandEventHandler( SpringUnitSync::OnReload ), NULL, this );
}


SpringUnitSync::~SpringUnitSync()
{
	Disconnect( wxUnitsyncReloadEvent, wxCommandEventHandler( SpringUnitSync::OnReload ), NULL, this );
	if ( m_cache_thread )
		m_cache_thread->Wait();
	delete m_cache_thread;
}

static int CompareStringNoCase(const std::string& first, const std::string& second)
{
	return first.CmpNoCase(second);
}

bool SpringUnitSync::FastLoadUnitSyncLib( const std::string& unitsyncloc )
{
	LOCK_UNITSYNC;
	if (!_LoadUnitSyncLib( unitsyncloc ))
		return false;

	m_mods_list.clear();
	m_mod_array.Clear();
	m_unsorted_mod_array.Clear();
	m_mods_unchained_hash.clear();

	const int numMods = susynclib().GetPrimaryModCount();
	std::string name, hash;
	for ( int i = 0; i < numMods; i++ )
	{
		try
		{
			name = susynclib().GetPrimaryModName( i );
			m_mods_list[name] = _T("fakehash");
			m_mod_array.Add( name );
			m_shortname_to_name_map[
					std::make_pair(susynclib().GetPrimaryModShortName( i ),
								   susynclib().GetPrimaryModVersion( i )) ] = name;
		} catch (...) { continue; }
	}
	m_unsorted_mod_array = m_mod_array;
	return true;
}
bool SpringUnitSync::FastLoadUnitSyncLibInit()
{
	LOCK_UNITSYNC;
	m_cache_thread = new WorkerThread();
	m_cache_thread->Create();
	m_cache_thread->SetPriority( WXTHREAD_MIN_PRIORITY );
	m_cache_thread->Run();

	UiEvents::ScopedStatusMessage staus(_("loading unitsync"), 0);

	if ( IsLoaded() )
	{
		m_cache_path = sett().GetCachePath();
		PopulateArchiveList();
	}
	return true;
}

bool SpringUnitSync::LoadUnitSyncLib( const std::string& unitsyncloc )
{
	LOCK_UNITSYNC;
	m_cache_thread = new WorkerThread();
	m_cache_thread->Create();
	m_cache_thread->SetPriority( WXTHREAD_MIN_PRIORITY );
	m_cache_thread->Run();

	UiEvents::ScopedStatusMessage staus(_("loading unitsync"), 0);

	bool ret = _LoadUnitSyncLib( unitsyncloc );
	if (ret)
	{
		m_cache_path = sett().GetCachePath();
		PopulateArchiveList();
		GetGlobalEventSender(GlobalEvents::OnUnitsyncReloaded).SendEvent( 0 );
	}
	return ret;
}

void SpringUnitSync::PopulateArchiveList()
{
	m_maps_list.clear();
	m_mods_list.clear();
	m_mod_array.Clear();
	m_map_array.Clear();
	m_unsorted_mod_array.Clear();
	m_unsorted_map_array.Clear();
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
			if ( !unchainedhash.IsEmpty() ) m_maps_unchained_hash[name] = unchainedhash;
			if ( !archivename.IsEmpty() ) m_maps_archive_name[name] = archivename;
			m_map_array.Add( name );
		} catch (...)
		{
			wxLogError( _T("Found map with hash collision: ") + name + _T(" hash: ") + hash );
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
			if ( !unchainedhash.IsEmpty() )  m_mods_unchained_hash[name] = unchainedhash;
			if ( !archivename.IsEmpty() ) m_mods_archive_name[name] = archivename;
			m_mod_array.Add( name );
			m_shortname_to_name_map[
					std::make_pair(susynclib().GetPrimaryModShortName( i ),
								   susynclib().GetPrimaryModVersion( i )) ] = name;
		} catch (...)
		{
			wxLogError( _T("Found game with hash collision: ") + name + _T(" hash: ") + hash );
		}
	}
	m_unsorted_mod_array = m_mod_array;
	m_unsorted_map_array = m_map_array;
	m_map_array.Sort(CompareStringNoCase);
	m_mod_array.Sort(CompareStringNoCase);
}



bool SpringUnitSync::_LoadUnitSyncLib( const std::string& unitsyncloc )
{
	try {
		susynclib().Load( unitsyncloc, sett().GetForcedSpringConfigFilePath() );
	} catch (...) {
		return false;
	}
	return true;
}


void SpringUnitSync::FreeUnitSyncLib()
{
	LOCK_UNITSYNC;

	susynclib().Unload();
}


bool SpringUnitSync::IsLoaded() const
{
	return susynclib().IsLoaded();
}


std::string SpringUnitSync::GetSpringVersion() const
{

	std::string ret;
	try
	{
		ret = susynclib().GetSpringVersion();
	}
	catch (...){}
	return ret;
}


bool SpringUnitSync::VersionSupports( GameFeature feature ) const
{
	return susynclib().VersionSupports( feature );
}


int SpringUnitSync::GetNumMods() const
{

	return m_mod_array.GetCount();
}


wxArrayString SpringUnitSync::GetModList() const
{
	return m_mod_array;
}


int SpringUnitSync::GetModIndex( const std::string& name ) const
{
	int result = m_mod_array.Index( name );
	if ( result == wxNOT_FOUND ) result = -1;
	return result;
}


bool SpringUnitSync::ModExists( const std::string& modname ) const
{
	return (m_mods_list.find(modname) != m_mods_list.end());
}


bool SpringUnitSync::ModExists( const std::string& modname, const std::string& hash ) const
{
	LocalArchivesVector::const_iterator itor = m_mods_list.find(modname);
	if ( itor == m_mods_list.end() ) return false;
	return itor->second == hash;
}

bool SpringUnitSync::ModExistsCheckHash( const std::string& hash ) const
{
	LocalArchivesVector::const_iterator itor = m_mods_list.begin();
	for ( ; itor != m_mods_list.end(); ++itor ) {
		if ( itor->second == hash )
			return true;
	}
	return false;
}

UnitSyncMod SpringUnitSync::GetMod( const std::string& modname )
{
	wxLogDebugFunc( _T("modname = \"") + modname + _T("\"") );
	UnitSyncMod m;

	m.name = modname;
	m.hash = m_mods_list[modname];

	return m;
}


UnitSyncMod SpringUnitSync::GetMod( int index )
{

	UnitSyncMod m;
	m.name = m_mod_array[index];
	m.hash = m_mods_list[m.name];

	return m;
}

int SpringUnitSync::GetNumMaps() const
{

	return m_map_array.GetCount();
}


wxArrayString SpringUnitSync::GetMapList() const
{
	return m_map_array;
}


wxArrayString SpringUnitSync::GetModValidMapList( const std::string& modname ) const
{
	wxArrayString ret;
	try
	{
		unsigned int mapcount = susynclib().GetValidMapCount( modname );
		for ( unsigned int i = 0; i < mapcount; i++ ) ret.Add( susynclib().GetValidMapName( i ) );
	} catch ( assert_exception& e ) {}
	return ret;
}


bool SpringUnitSync::MapExists( const std::string& mapname ) const
{
	return (m_maps_list.find(mapname) != m_maps_list.end());
}


bool SpringUnitSync::MapExists( const std::string& mapname, const std::string& hash ) const
{
	LocalArchivesVector::const_iterator itor = m_maps_list.find(mapname);
	if ( itor == m_maps_list.end() ) return false;
	return itor->second == hash;
}


UnitSyncMap SpringUnitSync::GetMap( const std::string& mapname )
{

	UnitSyncMap m;

	m.name = mapname;
	m.hash = m_maps_list[mapname];

	return m;
}


UnitSyncMap SpringUnitSync::GetMap( int index )
{

	UnitSyncMap m;

	m.name = m_map_array[index];
	m.hash = m_maps_list[m.name];

	return m;
}


UnitSyncMap SpringUnitSync::GetMapEx( int index )
{
	UnitSyncMap m;

	if ( index < 0 ) return m;

	m.name = m_map_array[index];

	m.hash = m_maps_list[m.name];

	m.info = _GetMapInfoEx( m.name );

	return m;
}

void GetOptionEntry( const int i, GameOptions& ret)
{
	//all section values for options are converted to lower case
	//since usync returns the key of section type keys lower case
	//otherwise comapring would be a real hassle
	std::string key = susynclib().GetOptionKey(i);
	std::string name = susynclib().GetOptionName(i);
	switch (susynclib().GetOptionType(i))
	{
	case opt_float:
	{
		ret.float_map[key] = mmOptionFloat( name, key,
											susynclib().GetOptionDesc(i), susynclib().GetOptionNumberDef(i),
											susynclib().GetOptionNumberStep(i),
											susynclib().GetOptionNumberMin(i), susynclib().GetOptionNumberMax(i),
											susynclib().GetOptionSection(i).Lower(), susynclib().GetOptionStyle(i) );
		break;
	}
	case opt_bool:
	{
		ret.bool_map[key] = mmOptionBool( name, key,
										  susynclib().GetOptionDesc(i), susynclib().GetOptionBoolDef(i),
										  susynclib().GetOptionSection(i).Lower(), susynclib().GetOptionStyle(i) );
		break;
	}
	case opt_string:
	{
		ret.string_map[key] = mmOptionString( name, key,
											  susynclib().GetOptionDesc(i), susynclib().GetOptionStringDef(i),
											  susynclib().GetOptionStringMaxLen(i),
											  susynclib().GetOptionSection(i).Lower(), susynclib().GetOptionStyle(i) );
		break;
	}
	case opt_list:
	{
		ret.list_map[key] = mmOptionList(name,key,
										 susynclib().GetOptionDesc(i),susynclib().GetOptionListDef(i),
										 susynclib().GetOptionSection(i).Lower(),susynclib().GetOptionStyle(i));

		int listItemCount = susynclib().GetOptionListCount(i);
		for (int j = 0; j < listItemCount; ++j)
		{
			std::string descr = susynclib().GetOptionListItemDesc(i,j);
			ret.list_map[key].addItem(susynclib().GetOptionListItemKey(i,j),susynclib().GetOptionListItemName(i,j), descr);
		}
		break;
	}
	case opt_section:
	{
		ret.section_map[key] = mmOptionSection( name, key, susynclib().GetOptionDesc(i),
												susynclib().GetOptionSection(i).Lower(), susynclib().GetOptionStyle(i) );
	}
	}
}


GameOptions SpringUnitSync::GetMapOptions( const std::string& name )
{
	wxLogDebugFunc( name );
	GameOptions ret;
	int count = susynclib().GetMapOptionCount(name);
	for (int i = 0; i < count; ++i)
	{
		GetOptionEntry( i, ret );
	}
	return ret;
}

wxArrayString SpringUnitSync::GetMapDeps( const std::string& mapname )
{
	wxArrayString ret;
	try
	{
		ret = susynclib().GetMapDeps( m_unsorted_map_array.Index( mapname ) );
	}
	catch( unitsync_assert ) {}
	return ret;
}


UnitSyncMap SpringUnitSync::GetMapEx( const std::string& mapname )
{

	int i = GetMapIndex( mapname );
	ASSERT_LOGIC( i >= 0, _T("Map does not exist") );
	return GetMapEx( i );
}


int SpringUnitSync::GetMapIndex( const std::string& name ) const
{
	int result = m_map_array.Index( name );
	if ( result == wxNOT_FOUND ) result = -1;
	return result;
}


GameOptions SpringUnitSync::GetModOptions( const std::string& name )
{
	wxLogDebugFunc( name );
	GameOptions ret;
	int count = susynclib().GetModOptionCount(name);
	for (int i = 0; i < count; ++i)
	{
		GetOptionEntry( i, ret );
	}
	return ret;
}

GameOptions SpringUnitSync::GetModCustomizations( const std::string& modname )
{
	wxLogDebugFunc( modname );

	GameOptions ret;
	int count = susynclib().GetCustomOptionCount( modname, _T("LobbyOptions.lua") );
	for (int i = 0; i < count; ++i) {
		GetOptionEntry( i, ret );
	}
	return ret;
}

GameOptions SpringUnitSync::GetSkirmishOptions( const std::string& modname, const std::string& skirmish_name )
{
	wxLogDebugFunc( modname );

	GameOptions ret;
	int count = susynclib().GetCustomOptionCount( modname, skirmish_name );
	for (int i = 0; i < count; ++i) {
		GetOptionEntry( i, ret );
	}
	return ret;
}

wxArrayString SpringUnitSync::GetModDeps( const std::string& modname ) const
{
	wxArrayString ret;
	try
	{
		ret = susynclib().GetModDeps( m_unsorted_mod_array.Index( modname ) );
	}
	catch( unitsync_assert ) {}
	return ret;
}

wxArrayString SpringUnitSync::GetSides( const std::string& modname )
{
	wxArrayString ret;
	if ( ! m_sides_cache.TryGet( modname, ret ) ) {
		try
		{
			ret = susynclib().GetSides( modname );
			m_sides_cache.Add( modname, ret );
		}
		catch( unitsync_assert ) {}
	}
	return ret;
}


UnitsyncImage SpringUnitSync::GetSidePicture( const std::string& modname, const std::string& SideName ) const
{
	std::string ImgName = _T("SidePics");
	ImgName += _T("/");
	ImgName += SideName.Upper();

	try {
		return GetImage( modname, ImgName + _T(".png"), false );
	}
	catch ( assert_exception& e){}
	return GetImage( modname, ImgName + _T(".bmp"), true );
}

UnitsyncImage SpringUnitSync::GetImage( const std::string& modname, const std::string& image_path, bool useWhiteAsTransparent  ) const
{


	UnitsyncImage cache;

	susynclib().SetCurrentMod( modname );

	int ini = susynclib().OpenFileVFS ( image_path );
	ASSERT_EXCEPTION( ini, _T("cannot find side image") );

	int FileSize = susynclib().FileSizeVFS(ini);
	if (FileSize == 0) {
		susynclib().CloseFileVFS(ini);
		ASSERT_EXCEPTION( FileSize, _T("image has size 0") );
	}

	uninitialized_array<char> FileContent(FileSize);
	susynclib().ReadFileVFS(ini, FileContent, FileSize);
	wxMemoryInputStream FileContentStream( FileContent, FileSize );

	cache.LoadFile( FileContentStream, wxBITMAP_TYPE_ANY, -1);
	cache.InitAlpha();
	if ( useWhiteAsTransparent )
	{
		for ( int x = 0; x < cache.GetWidth(); x++ )
			for ( int y = 0; y < cache.GetHeight(); y++ )
				if ( cache.GetBlue( x, y ) == 255 && cache.GetGreen( x, y ) == 255 && cache.GetRed( x, y ) == 255 )
					cache.SetAlpha( x, y, 0 ); // set pixel to be transparent
	}
	return cache;
}
#ifdef SL_QT_MODE
#include <QImage>
QImage SpringUnitSync::GetQImage( const std::string& modname, const std::string& image_path, bool useWhiteAsTransparent  ) const
{
	QImage cache;

	susynclib().SetCurrentMod( modname );

	int ini = susynclib().OpenFileVFS ( image_path );
	ASSERT_EXCEPTION( ini, _T("cannot find side image") );

	int FileSize = susynclib().FileSizeVFS(ini);
	if (FileSize == 0) {
		susynclib().CloseFileVFS(ini);
		ASSERT_EXCEPTION( FileSize, _T("image has size 0") );
	}

	uninitialized_array<char> FileContent(FileSize);
	QByteArray cache_data;
	cache_data.resize(FileSize);
	susynclib().ReadFileVFS(ini, cache_data.data(), FileSize);

	bool hu = cache.loadFromData( cache_data );
	assert( hu );
	return cache;
}
#endif

wxArrayString SpringUnitSync::GetAIList( const std::string& modname ) const
{


	wxArrayString ret;

	if ( usync().VersionSupports( USYNC_GetSkirmishAI ) )
	{
		int total = susynclib().GetSkirmishAICount( modname );
		for ( int i = 0; i < total; i++ )
		{
			wxArrayString infos = susynclib().GetAIInfo( i );
			int namepos = infos.Index( _T("shortName") );
			int versionpos = infos.Index( _T("version") );
			std::string ainame;
			if ( namepos != wxNOT_FOUND ) ainame += infos[namepos +1];
			if ( versionpos != wxNOT_FOUND ) ainame += _T(" ") + infos[versionpos +1];
			ret.Add( ainame );
		}
	}
	else
	{
		// list dynamic link libraries
		wxArrayString dlllist = susynclib().FindFilesVFS( wxDynamicLibrary::CanonicalizeName(_T("AI/Bot-libs/*"), wxDL_MODULE) );
		for( int i = 0; i < long(dlllist.GetCount()); i++ )
		{
			if ( ret.Index( dlllist[i].BeforeLast( '/') ) == wxNOT_FOUND ) ret.Add ( dlllist[i] ); // don't add duplicates
		}
		// list jar files (java AIs)
		wxArrayString jarlist = susynclib().FindFilesVFS( _T("AI/Bot-libs/*.jar") );
		for( int i = 0; i < long(jarlist.GetCount()); i++ )
		{
			if ( ret.Index( jarlist[i].BeforeLast( '/') ) == wxNOT_FOUND ) ret.Add ( jarlist[i] ); // don't add duplicates
		}

		// luaai
		try
		{
			const int LuaAICount = susynclib().GetLuaAICount( modname );
			for ( int i = 0; i < LuaAICount; i++ )
			{
				ret.Add( _T( "LuaAI:" ) +  susynclib().GetLuaAIName( i ) );
			}
		} CATCH_ANY
	}

	return ret;
}

void SpringUnitSync::UnSetCurrentMod()
{
	try
	{
		susynclib().UnSetCurrentMod();
	} catch( unitsync_assert ) {}
}

wxArrayString SpringUnitSync::GetAIInfos( int index ) const
{
	wxArrayString ret;
	try
	{
		ret = susynclib().GetAIInfo( index );
	}
	catch ( unitsync_assert ) {}
	return ret;
}

GameOptions SpringUnitSync::GetAIOptions( const std::string& modname, int index )
{
	wxLogDebugFunc( Tostd::string(index) );
	GameOptions ret;
	int count = susynclib().GetAIOptionCount(modname, index);
	for (int i = 0; i < count; ++i)
	{
		GetOptionEntry( i, ret );
	}
	return ret;
}

int SpringUnitSync::GetNumUnits( const std::string& modname ) const
{


	susynclib().AddAllArchives( susynclib().GetPrimaryModArchive( m_unsorted_mod_array.Index( modname ) ) );
	susynclib().ProcessUnitsNoChecksum();

	return susynclib().GetUnitCount();
}


wxArrayString SpringUnitSync::GetUnitsList( const std::string& modname )
{
	wxLogDebugFunc( modname );

	wxArrayString cache;
	try
	{
		cache = GetCacheFile( GetFileCachePath( modname, _T(""), true ) + _T(".units") );
	} catch(...)
	{
		susynclib().SetCurrentMod( modname );
		while ( susynclib().ProcessUnitsNoChecksum() ) {}
		unsigned int unitcount = susynclib().GetUnitCount();
		for ( unsigned int i = 0; i < unitcount; i++ )
		{
			cache.Add( susynclib().GetFullUnitName(i) << _T(" (") << susynclib().GetUnitName(i) << _T(")") );
		}

		SetCacheFile( GetFileCachePath( modname, _T(""), true ) + _T(".units"), cache );

	}

	return cache;
}


UnitsyncImage SpringUnitSync::GetMinimap( const std::string& mapname )
{
	return _GetMapImage( mapname, _T(".minimap.png"), &SpringUnitSyncLib::GetMinimap );
}

UnitsyncImage SpringUnitSync::GetMinimap( const std::string& mapname, int width, int height )
{
	const bool tiny = ( width <= 100 && height <= 100 );
	UnitsyncImage img;
	if ( tiny && m_tiny_minimap_cache.TryGet( mapname, img ) )
	{
		wxSize image_size = MakeFit(wxSize(img.GetWidth(), img.GetHeight()), wxSize(width, height));
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

			wxSize image_size = MakeFit(wxSize(mapinfo.width, mapinfo.height), wxSize(width, height));
			img.Rescale( image_size.GetWidth(), image_size.GetHeight() );
		}
		catch (...) {
			img = UnitsyncImage( 1, 1 );
		}
	}

	if ( tiny ) m_tiny_minimap_cache.Add( mapname, img );

	return img;
}

UnitsyncImage SpringUnitSync::GetMetalmap( const std::string& mapname )
{
	return _GetMapImage( mapname, _T(".metalmap.png"), &SpringUnitSyncLib::GetMetalmap );
}

UnitsyncImage SpringUnitSync::GetMetalmap( const std::string& mapname, int width, int height )
{
	return _GetScaledMapImage( mapname, &SpringUnitSync::GetMetalmap, width, height );
}

UnitsyncImage SpringUnitSync::GetHeightmap( const std::string& mapname )
{
	return _GetMapImage( mapname, _T(".heightmap.png"), &SpringUnitSyncLib::GetHeightmap );
}

UnitsyncImage SpringUnitSync::GetHeightmap( const std::string& mapname, int width, int height )
{
	return _GetScaledMapImage( mapname, &SpringUnitSync::GetHeightmap, width, height );
}

UnitsyncImage SpringUnitSync::_GetMapImage( const std::string& mapname, const std::string& imagename, UnitsyncImage (SpringUnitSyncLib::*loadMethod)(const std::string&) )
{
	UnitsyncImage img;
	if ( m_map_image_cache.TryGet( mapname + imagename, img ) )
		return img;

	std::string originalsizepath = GetFileCachePath( mapname, m_maps_unchained_hash[mapname], false ) + imagename;
	try
	{
		ASSERT_EXCEPTION( wxFileExists( originalsizepath ), _T("File cached image does not exist") );

		img = UnitsyncImage( originalsizepath, wxBITMAP_TYPE_PNG );
		ASSERT_EXCEPTION( img.Ok(), _T("Failed to load cache image") );
	}
	catch (...)
	{
		try
		{
			img = (susynclib().*loadMethod)( mapname );
			img.SaveFile( originalsizepath, wxBITMAP_TYPE_PNG );
		}
		catch (...)
		{
			img = UnitsyncImage( 1, 1 );
		}
	}
	m_map_image_cache.Add( mapname + imagename, img );
	return img;
}

UnitsyncImage SpringUnitSync::_GetScaledMapImage( const std::string& mapname, UnitsyncImage (SpringUnitSync::*loadMethod)(const std::string&), int width, int height )
{
	UnitsyncImage img = (this->*loadMethod) ( mapname );
	if (img.GetWidth() > 1 && img.GetHeight() > 1)
	{
		wxSize image_size = MakeFit(wxSize(img.GetWidth(), img.GetHeight()), wxSize(width, height));
		img.Rescale( image_size.GetWidth(), image_size.GetHeight() );
	}
	return img;
}

MapInfo SpringUnitSync::_GetMapInfoEx( const std::string& mapname )
{
	MapInfo info;
	if ( m_mapinfo_cache.TryGet( mapname, info ) )
		return info;

	wxArrayString cache;
	try {
		try
		{
			cache = GetCacheFile( GetFileCachePath( mapname, m_maps_unchained_hash[mapname], false ) + _T(".infoex") );

			ASSERT_EXCEPTION( cache.GetCount() >= 11, _T("not enough lines found in cache info ex") );
			info.author = cache[0];
			info.tidalStrength =  s2l( cache[1] );
			info.gravity = s2l( cache[2] );
			info.maxMetal = s2d( cache[3] );
			info.extractorRadius = s2d( cache[4] );
			info.minWind = s2l( cache[5] );
			info.maxWind = s2l( cache[6] );
			info.width = s2l( cache[7] );
			info.height = s2l( cache[8] );
			wxArrayString posinfo = std::stringTokenize( cache[9], _T(' '), wxTOKEN_RET_EMPTY );
			for ( unsigned int i = 0; i < posinfo.GetCount(); i++)
			{
				StartPos position;
				position.x = s2l( posinfo[i].BeforeFirst( _T('-') ) );
				position.y = s2l( posinfo[i].AfterFirst( _T('-') ) );
				info.positions.push_back( position );
			}

			unsigned int LineCount = cache.GetCount();
			for ( unsigned int i = 10; i < LineCount; i++ ) info.description << cache[i] << _T('\n');

		}
		catch (...)
		{
			info = susynclib().GetMapInfoEx( m_unsorted_map_array.Index(mapname), 1 );

			cache.Add ( info.author );
			cache.Add( Tostd::string( info.tidalStrength ) );
			cache.Add( Tostd::string( info.gravity ) );
			cache.Add( Tostd::string( info.maxMetal ) );
			cache.Add( Tostd::string( info.extractorRadius ) );
			cache.Add( Tostd::string( info.minWind ) );
			cache.Add( Tostd::string( info.maxWind )  );
			cache.Add( Tostd::string( info.width ) );
			cache.Add( Tostd::string( info.height ) );

			std::string postring;
			for ( unsigned int i = 0; i < info.positions.size(); i++)
			{
				postring << Tostd::string( info.positions[i].x ) << _T('-') << Tostd::string( info.positions[i].y ) << _T(' ');
			}
			cache.Add( postring );

			wxArrayString descrtoken = std::stringTokenize( info.description, _T('\n') );
			unsigned int desclinecount = descrtoken.GetCount();
			for ( unsigned int count = 0; count < desclinecount; count++ ) cache.Add( descrtoken[count] );

			SetCacheFile( GetFileCachePath( mapname, m_maps_unchained_hash[mapname], false ) + _T(".infoex"), cache );
		}
	}
	catch ( ... ) {
		info.width = 1;
		info.height = 1;
	}

	m_mapinfo_cache.Add( mapname, info );

	return info;
}

void SpringUnitSync::OnReload( wxCommandEvent& /*event*/ )
{
	ReloadUnitSyncLib();
}

void SpringUnitSync::AddReloadEvent(  )
{
	wxCommandEvent evt( wxUnitsyncReloadEvent, wxNewId() );
	AddPendingEvent( evt );
}

wxArrayString SpringUnitSync::FindFilesVFS( const std::string& pattern ) const
{
	return susynclib().FindFilesVFS( pattern );
}

bool SpringUnitSync::ReloadUnitSyncLib()
{
	return LoadUnitSyncLib( sett().GetCurrentUsedUnitSync() );
}


void SpringUnitSync::SetSpringDataPath( const std::string& path )
{
	susynclib().SetSpringConfigString( _T("SpringData"), path );
}


std::string SpringUnitSync::GetFileCachePath( const std::string& name, const std::string& hash, bool IsMod )
{
	//  LOCK_UNITSYNC;

	std::string ret = m_cache_path;
	if ( !name.IsEmpty() ) ret << name;
	else return wxEmptyString;
	if ( !hash.IsEmpty() ) ret << _T("-") << hash;
	else
	{
		if ( IsMod ) ret <<  _T("-") << m_mods_list[name];
		else
		{
			ret << _T("-") << m_maps_list[name];
		}
	}
	return ret;
}


wxArrayString SpringUnitSync::GetCacheFile( const std::string& path ) const
{
	wxArrayString ret;
	wxTextFile file( path );
	file.Open();
	ASSERT_EXCEPTION( file.IsOpened() , wxFormat( _T("cache file( %s ) not found") ) % path );
	unsigned int linecount = file.GetLineCount();
	for ( unsigned int count = 0; count < linecount; count ++ )
	{
		ret.Add( file[count] );
	}
	return ret;
}


void SpringUnitSync::SetCacheFile( const std::string& path, const wxArrayString& data )
{
	wxTextFile file( path );
	unsigned int arraycount = data.GetCount();
	for ( unsigned int count = 0; count < arraycount; count++ )
	{
		file.AddLine( data[count] );
	}
	file.Write();
	file.Close();
}

wxArrayString  SpringUnitSync::GetPlaybackList( bool ReplayType ) const
{
	wxArrayString ret;
	if ( !IsLoaded() ) return ret;

	if ( ReplayType )
		return susynclib().FindFilesVFS( _T("demos/*.sdf") );
	else
		return susynclib().FindFilesVFS( _T("Saves/*.ssf") );
}

bool SpringUnitSync::FileExists( const std::string& name ) const
{
	int handle = susynclib().OpenFileVFS(name);
	if ( handle == 0 ) return false;
	susynclib().CloseFileVFS(handle);
	return true;
}


std::string SpringUnitSync::GetArchivePath( const std::string& name ) const
{
	wxLogDebugFunc( name );

	return susynclib().GetArchivePath( name );
}

wxArrayString SpringUnitSync::GetScreenshotFilenames() const
{
	wxArrayString ret;
	if ( !IsLoaded() ) return ret;

	ret = susynclib().FindFilesVFS( _T("screenshots/*.*") );
	for ( int i = 0; i < long(ret.Count() - 1); ++i ) {
		if ( ret[i] == ret[i+1] )
			ret.RemoveAt( i+1 );
	}
	ret.Sort();
	return ret;
}

std::string SpringUnitSync::GetDefaultNick()
{
	std::string name = susynclib().GetSpringConfigString( _T("name"), _T("Player") );
	if ( name.IsEmpty() ) {
		susynclib().SetSpringConfigString( _T("name"), _T("Player") );
		return _T("Player");
	}
	return name;
}

void SpringUnitSync::SetDefaultNick( const std::string& nick )
{
	susynclib().SetSpringConfigString( _T("name"), nick );
}

////////////////////////////////////////////////////////////////////////////////
////////////////////////////// Unitsync prefetch/background thread code

namespace
{
typedef UnitsyncImage (SpringUnitSync::*LoadMethodPtr)(const std::string&);
typedef UnitsyncImage (SpringUnitSync::*ScaledLoadMethodPtr)(const std::string&, int, int);

class CacheMapWorkItem : public WorkItem
{
public:
	SpringUnitSync* m_usync;
	std::string m_mapname;
	LoadMethodPtr m_loadMethod;

	void Run()
	{
		(m_usync->*m_loadMethod)( m_mapname );
	}

	CacheMapWorkItem( SpringUnitSync* usync, const std::string& mapname, LoadMethodPtr loadMethod )
		: m_usync(usync), m_mapname(mapname.c_str()), m_loadMethod(loadMethod) {}
};

class CacheMinimapWorkItem : public WorkItem
{
public:
	std::string m_mapname;

	void Run()
	{
		// Fetch rescaled minimap using this specialized class instead of
		// CacheMapWorkItem with a pointer to SpringUnitSync::GetMinimap,
		// to ensure SpringUnitSync::_GetMapInfoEx will be called too, and
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
			m_mapname = wxEmptyString;
		}
		PostEvent();
	}

protected:
	SpringUnitSync* m_usync;
	std::string m_mapname;
	int m_evtHandlerId;
	int m_evtId;

	void PostEvent()
	{
		wxCommandEvent evt( UnitSyncAsyncOperationCompletedEvt, m_evtId );
		evt.SetString( m_mapname );
		m_usync->PostEvent( m_evtHandlerId, evt );
	}

	virtual void RunCore() = 0;

	GetMapImageAsyncResult( SpringUnitSync* usync, const std::string& mapname, int evtHandlerId, int evtId )
		: m_usync(usync), m_mapname(mapname.c_str()), m_evtHandlerId(evtHandlerId), m_evtId(evtId) {}
};

class GetMapImageAsyncWorkItem : public GetMapImageAsyncResult
{
public:
	void RunCore()
	{
		(m_usync->*m_loadMethod)( m_mapname );
	}

	LoadMethodPtr m_loadMethod;

	GetMapImageAsyncWorkItem( SpringUnitSync* usync, const std::string& mapname, int evtHandlerId, LoadMethodPtr loadMethod )
		: GetMapImageAsyncResult( usync, mapname, evtHandlerId, 1 ), m_loadMethod(loadMethod) {}
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

	GetScaledMapImageAsyncWorkItem( SpringUnitSync* usync, const std::string& mapname, int w, int h, int evtHandlerId, ScaledLoadMethodPtr loadMethod )
		: GetMapImageAsyncResult( usync, mapname, evtHandlerId, 2 ), m_width(w), m_height(h), m_loadMethod(loadMethod) {}
};

class GetMapExAsyncWorkItem : public GetMapImageAsyncResult
{
public:
	void RunCore()
	{
		m_usync->GetMapEx( m_mapname );
	}

	GetMapExAsyncWorkItem( SpringUnitSync* usync, const std::string& mapname, int evtHandlerId )
		: GetMapImageAsyncResult( usync, mapname, evtHandlerId, 3 ) {}
};
}


void SpringUnitSync::PrefetchMap( const std::string& mapname )
{
	wxLogDebugFunc( mapname );

	// Use a simple hash based on 3 characters from the mapname
	// (without '.smf') as negative priority for the WorkItems.
	// This ensures WorkItems for the same map are put together,
	// which improves caching performance.

	// Measured improvement: 60% more cache hits while populating replay tab.
	// 50% hits without, 80% hits with this code.  (cache size 20 images)

	const int length = std::max(0, int(mapname.length()) - 4);
	const int hash = ( wxChar(mapname[length * 1/4]) << 16 )
			| ( wxChar(mapname[length * 2/4]) << 8  )
			| wxChar(mapname[length * 3/4]);
	const int priority = -hash;

	if (! m_cache_thread )
	{
		wxLogError( _T("cache thread not initialised") );
		return;
	}
	{
		CacheMinimapWorkItem* work;

		work = new CacheMinimapWorkItem( mapname );
		m_cache_thread->DoWork( work, priority );
	}
	{
		CacheMapWorkItem* work;

		work = new CacheMapWorkItem( this, mapname, &SpringUnitSync::GetMetalmap );
		m_cache_thread->DoWork( work, priority );

		work = new CacheMapWorkItem( this, mapname, &SpringUnitSync::GetHeightmap );
		m_cache_thread->DoWork( work, priority );
	}
}

int SpringUnitSync::RegisterEvtHandler( wxEvtHandler* evtHandler )
{
	return m_evt_handlers.Add( evtHandler );
}

void SpringUnitSync::UnregisterEvtHandler( int evtHandlerId )
{
	m_evt_handlers.Remove( evtHandlerId );
}

void SpringUnitSync::PostEvent( int evtHandlerId, wxEvent& evt )
{
	m_evt_handlers.PostEvent( evtHandlerId, evt );
}

void SpringUnitSync::_GetMapImageAsync( const std::string& mapname, UnitsyncImage (SpringUnitSync::*loadMethod)(const std::string&), int evtHandlerId )
{
	if (! m_cache_thread )
	{
		wxLogError( _T("cache thread not initialised") );
		return;
	}
	GetMapImageAsyncWorkItem* work;

	work = new GetMapImageAsyncWorkItem( this, mapname, evtHandlerId, loadMethod );
	m_cache_thread->DoWork( work, 100 );
}

void SpringUnitSync::GetMinimapAsync( const std::string& mapname, int evtHandlerId )
{
	_GetMapImageAsync( mapname, &SpringUnitSync::GetMinimap, evtHandlerId );
}

void SpringUnitSync::GetMinimapAsync( const std::string& mapname, int width, int height, int evtHandlerId )
{
	if (! m_cache_thread )
	{
		LslError( _T("cache thread not initialised") );
		return;
	}
	GetScaledMapImageAsyncWorkItem* work;
	work = new GetScaledMapImageAsyncWorkItem( this, mapname, width, height, evtHandlerId, &SpringUnitSync::GetMinimap );
	m_cache_thread->DoWork( work, 100 );
}

void SpringUnitSync::GetMetalmapAsync( const std::string& mapname, int evtHandlerId )
{
	wxLogDebugFunc( mapname );
	_GetMapImageAsync( mapname, &SpringUnitSync::GetMetalmap, evtHandlerId );
}

void SpringUnitSync::GetMetalmapAsync( const std::string& mapname, int /*width*/, int /*height*/, int evtHandlerId )
{
	GetMetalmapAsync( mapname, evtHandlerId );
}

void SpringUnitSync::GetHeightmapAsync( const std::string& mapname, int evtHandlerId )
{
	wxLogDebugFunc( mapname );
	_GetMapImageAsync( mapname, &SpringUnitSync::GetHeightmap, evtHandlerId );
}

void SpringUnitSync::GetHeightmapAsync( const std::string& mapname, int /*width*/, int /*height*/, int evtHandlerId )
{
	GetHeightmapAsync( mapname, evtHandlerId );
}

void SpringUnitSync::GetMapExAsync( const std::string& mapname, int evtHandlerId )
{
	wxLogDebugFunc( mapname );
	if (! m_cache_thread )
	{
		wxLogError( _T("cache thread not initialised") );
		return;
	}

	GetMapExAsyncWorkItem* work;

	work = new GetMapExAsyncWorkItem( this, mapname, evtHandlerId );
	m_cache_thread->DoWork( work, 200 /* higher prio then GetMinimapAsync */ );
}

std::string SpringUnitSync::GetTextfileAsString( const std::string& modname, const std::string& file_path )
{
	susynclib().SetCurrentMod( modname );

	int ini = susynclib().OpenFileVFS ( file_path );
	if ( !ini )
		return wxEmptyString;

	int FileSize = susynclib().FileSizeVFS(ini);
	if (FileSize == 0) {
		susynclib().CloseFileVFS(ini);
		return wxEmptyString;
	}

	uninitialized_array<char> FileContent(FileSize);
	susynclib().ReadFileVFS(ini, FileContent, FileSize);
	return std::string( FileContent, wxConvAuto(), size_t( FileSize ) );
}

////////////////////////////////////////////////////////////////////////////////
////////////////////////////// EvtHandlerCollection code

int EvtHandlerCollection::Add( wxEvtHandler* evtHandler )
{
	boost::mutex::scoped_lock lock(m_lock);
	++m_last_id;
	m_items[m_last_id] = evtHandler;
	return m_last_id;
}

void EvtHandlerCollection::Remove( int evtHandlerId )
{
	boost::mutex::scoped_lock lock(m_lock);
	EvtHandlerMap::iterator it = m_items.find( evtHandlerId );
	if ( it != m_items.end() ) m_items.erase( it );
}

void EvtHandlerCollection::PostEvent( int evtHandlerId, wxEvent& evt )
{
	boost::mutex::scoped_lock lock(m_lock);
	EvtHandlerMap::iterator it = m_items.find( evtHandlerId );
	if ( it != m_items.end() ) wxPostEvent( it->second, evt );
}

std::string SpringUnitSync::GetNameForShortname( const std::string& shortname, const std::string& version) const
{
	ShortnameVersionToNameMap::const_iterator it
			=  m_shortname_to_name_map.find( std::make_pair(shortname,version) );
	if ( it != m_shortname_to_name_map.end() )
		return it->second;
	return wxEmptyString;
}

} // namespace LSL
