#include "loader.h"

#include <boost/extension/shared_library.hpp>
#include <string>

#include "c_api.h"
#include "sharedlib.h"

namespace LSL {

#define BIND(type, name,var) \
	s->var = (type) GetLibFuncPtr(s->m_libhandle, name);

void UnitsyncFunctionLoader::LuaParser(UnitsyncLib *s)
{
    BIND(lpClosePtr, "lpClose", m_parser_close );
    BIND(lpOpenFilePtr, "lpOpenFile", m_parser_open_file );
    BIND(lpOpenSourcePtr, "lpOpenSource", m_parser_open_source );
    BIND(lpExecutePtr, "lpExecute", m_parser_execute );
    BIND(lpErrorLogPtr, "lpErrorLog", m_parser_error_log );

    BIND(lpAddTableIntPtr, "lpAddTableInt", m_parser_add_table_int );
    BIND(lpAddTableStrPtr, "lpAddTableStr", m_parser_add_table_string );
    BIND(lpEndTablePtr, "lpEndTable", m_parser_end_table );
    BIND(lpAddIntKeyIntValPtr, "lpAddIntKeyIntVal", m_parser_add_int_key_int_value );
    BIND(lpAddStrKeyIntValPtr, "lpAddStrKeyIntVal", m_parser_add_string_key_int_value );
    BIND(lpAddIntKeyBoolValPtr, "lpAddIntKeyBoolVal", m_parser_add_int_key_bool_value );
    BIND(lpAddStrKeyBoolValPtr, "lpAddStrKeyBoolVal", m_parser_add_string_key_bool_value );
    BIND(lpAddIntKeyFloatValPtr, "lpAddIntKeyFloatVal", m_parser_add_int_key_float_value );
    BIND(lpAddStrKeyFloatValPtr, "lpAddStrKeyFloatVal", m_parser_add_string_key_float_value );
    BIND(lpAddIntKeyStrValPtr, "lpAddIntKeyStrVal", m_parser_add_int_key_string_value );
    BIND(lpAddStrKeyStrValPtr, "lpAddStrKeyStrVal", m_parser_add_string_key_string_value );

    BIND(lpRootTablePtr, "lpRootTable", m_parser_root_table );
    BIND(lpRootTableExprPtr, "lpRootTableExpr", m_parser_root_table_expression );
    BIND(lpSubTableIntPtr, "lpSubTableInt", m_parser_sub_table_int );
    BIND(lpSubTableStrPtr, "lpSubTableStr", m_parser_sub_table_string );
    BIND(lpSubTableExprPtr, "lpSubTableExpr", m_parser_sub_table_expression );
    BIND(lpPopTablePtr, "lpPopTable", m_parser_pop_table );

    BIND(lpGetKeyExistsIntPtr, "lpGetKeyExistsInt", m_parser_key_int_exists );
    BIND(lpGetKeyExistsStrPtr, "lpGetKeyExistsStr", m_parser_key_string_exists );

    BIND(lpGetIntKeyTypePtr, "lpGetIntKeyType", m_parser_int_key_get_type );
    BIND(lpGetStrKeyTypePtr, "lpGetStrKeyType", m_parser_string_key_get_type );

    BIND(lpGetIntKeyListCountPtr, "lpGetIntKeyListCount", m_parser_int_key_get_list_count );
    BIND(lpGetIntKeyListEntryPtr, "lpGetIntKeyListEntry", m_parser_int_key_get_list_entry );
    BIND(lpGetStrKeyListCountPtr, "lpGetStrKeyListCount", m_parser_string_key_get_list_count );
    BIND(lpGetStrKeyListEntryPtr, "lpGetStrKeyListEntry", m_parser_string_key_get_list_entry );

    BIND(lpGetIntKeyIntValPtr, "lpGetIntKeyIntVal", m_parser_int_key_get_int_value );
    BIND(lpGetStrKeyIntValPtr, "lpGetStrKeyIntVal", m_parser_string_key_get_int_value );
    BIND(lpGetIntKeyBoolValPtr, "lpGetIntKeyBoolVal", m_parser_int_key_get_bool_value );
    BIND(lpGetStrKeyBoolValPtr, "lpGetStrKeyBoolVal", m_parser_string_key_get_bool_value );
    BIND(lpGetIntKeyFloatValPtr, "lpGetIntKeyFloatVal", m_parser_int_key_get_float_value );
    BIND(lpGetStrKeyFloatValPtr, "lpGetStrKeyFloatVal", m_parser_string_key_get_float_value );
    BIND(lpGetIntKeyStrValPtr, "lpGetIntKeyStrVal", m_parser_int_key_get_string_value );
    BIND(lpGetStrKeyStrValPtr, "lpGetStrKeyStrVal", m_parser_string_key_get_string_value );
}

void UnitsyncFunctionLoader::MMOptions(UnitsyncLib *s)
{
    BIND(GetMapOptionCountPtr, "GetMapOptionCount", m_get_map_option_count );
    BIND(GetCustomOptionCountPtr, "GetCustomOptionCount", m_get_custom_option_count );
    BIND(GetModOptionCountPtr, "GetModOptionCount", m_get_mod_option_count );
    BIND(GetSkirmishAIOptionCountPtr, "GetSkirmishAIOptionCount",m_get_skirmish_ai_option_count );
    BIND(GetOptionKeyPtr, "GetOptionKey", m_get_option_key );
    BIND(GetOptionNamePtr, "GetOptionName", m_get_option_name );
    BIND(GetOptionDescPtr, "GetOptionDesc", m_get_option_desc );
    BIND(GetOptionTypePtr, "GetOptionType", m_get_option_type );
    BIND(GetOptionSectionPtr, "GetOptionSection", m_get_option_section );
    BIND(GetOptionStylePtr, "GetOptionStyle", m_get_option_style );
    BIND(GetOptionBoolDefPtr, "GetOptionBoolDef", m_get_option_bool_def );
    BIND(GetOptionNumberDefPtr, "GetOptionNumberDef", m_get_option_number_def );
    BIND(GetOptionNumberMinPtr, "GetOptionNumberMin", m_get_option_number_min );
    BIND(GetOptionNumberMaxPtr, "GetOptionNumberMax", m_get_option_number_max );
    BIND(GetOptionNumberStepPtr, "GetOptionNumberStep", m_get_option_number_step );
    BIND(GetOptionStringDefPtr, "GetOptionStringDef", m_get_option_string_def );
    BIND(GetOptionStringMaxLenPtr, "GetOptionStringMaxLen", m_get_option_string_max_len );
    BIND(GetOptionListCountPtr, "GetOptionListCount", m_get_option_list_count );
    BIND(GetOptionListDefPtr, "GetOptionListDef", m_get_option_list_def );
    BIND(GetOptionListItemKeyPtr, "GetOptionListItemKey", m_get_option_list_item_key );
    BIND(GetOptionListItemNamePtr, "GetOptionListItemName", m_get_option_list_item_name );
    BIND(GetOptionListItemDescPtr, "GetOptionListItemDesc", m_get_option_list_item_desc );
}

void UnitsyncFunctionLoader::Map(UnitsyncLib *s)
{
    BIND(GetMapCountPtr, "GetMapCount",  m_get_map_count );
    BIND(GetMapChecksumPtr, "GetMapChecksum",  m_get_map_checksum );
    BIND(GetMapNamePtr, "GetMapName",  m_get_map_name );

    try {
        BIND(GetMapDescriptionPtr, "GetMapDescription", m_get_map_description );
        BIND(GetMapAuthorPtr, "GetMapAuthor", m_get_map_author );
        BIND(GetMapWidthPtr, "GetMapWidth", m_get_map_width );
        BIND(GetMapHeightPtr, "GetMapHeight", m_get_map_height );
        BIND(GetMapTidalStrengthPtr, "GetMapTidalStrength", m_get_map_tidalStrength );
        BIND(GetMapWindMinPtr, "GetMapWindMin", m_get_map_windMin );
        BIND(GetMapWindMaxPtr, "GetMapWindMax", m_get_map_windMax );
        BIND(GetMapGravityPtr, "GetMapGravity", m_get_map_gravity );
        BIND(GetMapResourceCountPtr, "GetMapResourceCount", m_get_map_resource_count );
        BIND(GetMapResourceNamePtr, "GetMapResourceName", m_get_map_resource_name );
        BIND(GetMapResourceMaxPtr, "GetMapResourceMax", m_get_map_resource_max );
        BIND(GetMapResourceExtractorRadiusPtr, "GetMapResourceExtractorRadius",m_get_map_resource_extractorRadius );
        BIND(GetMapPosCountPtr, "GetMapPosCount", m_get_map_pos_count );
        BIND(GetMapPosXPtr, "GetMapPosX", m_get_map_pos_x );
        BIND(GetMapPosZPtr, "GetMapPosZ", m_get_map_pos_z );
        LslDebug("Using new style map-info fetching (GetMap*() functions).");
    }
    catch ( ... )
    {
        s->m_get_map_name = NULL;
        LslDebug("Using old style map-info fetching (GetMapInfoEx()).");
    }
    BIND(GetMapInfoExPtr, "GetMapInfoEx", m_get_map_info_ex );
    BIND(GetMinimapPtr, "GetMinimap", m_get_minimap );
    BIND(GetInfoMapSizePtr, "GetInfoMapSize", m_get_infomap_size );
    BIND(GetInfoMapPtr, "GetInfoMap", m_get_infomap );

    BIND(GetMapArchiveCountPtr, "GetMapArchiveCount", m_get_map_archive_count );
    BIND(GetMapArchiveNamePtr, "GetMapArchiveName", m_get_map_archive_name );
    BIND(GetMapChecksumPtr, "GetMapChecksum", m_get_map_checksum );
    BIND(GetMapChecksumFromNamePtr, "GetMapChecksumFromName", m_get_map_checksum_from_name );
}

void UnitsyncFunctionLoader::Basic(UnitsyncLib *s)
{
    BIND(InitPtr, "Init", m_init );
    BIND(UnInitPtr, "UnInit", m_uninit );
    BIND(GetNextErrorPtr, "GetNextError", m_get_next_error );
    BIND(GetWritableDataDirectoryPtr, "GetWritableDataDirectory", m_get_writeable_data_dir );
    BIND(GetDataDirectoryPtr, "GetDataDirectory", m_get_data_dir_by_index );
    BIND(GetDataDirectoryCountPtr, "GetDataDirectoryCount", m_get_data_dir_count );
    BIND(GetSpringVersionPtr, "GetSpringVersion", m_get_spring_version );
    BIND(GetSpringVersionPatchsetPtr, "GetSpringVersionPatchset", m_get_spring_version_patchset);
    BIND(IsSpringReleaseVersionPtr, "IsSpringReleaseVersion", m_is_spring_release_version );

    BIND(AddAllArchivesPtr, "AddAllArchives", m_add_all_archives );
    BIND(RemoveAllArchivesPtr, "RemoveAllArchives", m_remove_all_archives );

    BIND(InitFindVFSPtr, "InitFindVFS", m_init_find_vfs );
    BIND(FindFilesVFSPtr, "FindFilesVFS", m_find_files_vfs );
    BIND(OpenFileVFSPtr, "OpenFileVFS", m_open_file_vfs );
    BIND(FileSizeVFSPtr, "FileSizeVFS", m_file_size_vfs );
    BIND(ReadFileVFSPtr, "ReadFileVFS", m_read_file_vfs );
    BIND(CloseFileVFSPtr, "CloseFileVFS", m_close_file_vfs );


    BIND(ProcessUnitsPtr, "ProcessUnits", m_process_units );
    BIND(AddArchivePtr, "AddArchive", m_add_archive );
    BIND(GetArchiveChecksumPtr, "GetArchiveChecksum", m_get_archive_checksum );
    BIND(GetArchivePathPtr, "GetArchivePath", m_get_archive_path );

    BIND(OpenArchivePtr, "OpenArchive", m_open_archive );
    BIND(CloseArchivePtr, "CloseArchive", m_close_archive );
    BIND(FindFilesArchivePtr, "FindFilesArchive", m_find_Files_archive );
    BIND(OpenArchiveFilePtr, "OpenArchiveFile", m_open_archive_file );
    BIND(ReadArchiveFilePtr, "ReadArchiveFile", m_read_archive_file );
    BIND(CloseArchiveFilePtr, "CloseArchiveFile", m_close_archive_file );
    BIND(SizeArchiveFilePtr, "SizeArchiveFile", m_size_archive_file );

    BIND(GetSkirmishAICountPtr, "GetSkirmishAICount", m_get_skirmish_ai_count );
    BIND(GetSkirmishAIInfoCountPtr, "GetSkirmishAIInfoCount", m_get_skirmish_ai_info_count );
    BIND(GetInfoKeyPtr, "GetInfoKey", m_get_skirmish_ai_info_key );
    BIND(GetInfoValuePtr, "GetInfoValue", m_get_skirmish_ai_info_value );
    BIND(GetInfoDescriptionPtr, "GetInfoDescription", m_get_skirmish_ai_info_description );
}

void UnitsyncFunctionLoader::Config(UnitsyncLib *s)
{
    BIND(SetSpringConfigFilePtr, "SetSpringConfigFile", m_set_spring_config_file_path );
    BIND(GetSpringConfigFilePtr, "GetSpringConfigFile", m_get_spring_config_file_path );

    BIND(SetSpringConfigFloatPtr, "SetSpringConfigFloat", m_set_spring_config_float );
    BIND(GetSpringConfigFloatPtr, "GetSpringConfigFloat", m_get_spring_config_float );
    BIND(GetSpringConfigIntPtr, "GetSpringConfigInt", m_get_spring_config_int );
    BIND(GetSpringConfigStringPtr, "GetSpringConfigString", m_get_spring_config_string );
    BIND(SetSpringConfigStringPtr, "SetSpringConfigString", m_set_spring_config_string );
    BIND(SetSpringConfigIntPtr, "SetSpringConfigInt", m_set_spring_config_int );
}

void UnitsyncFunctionLoader::Mod(UnitsyncLib *s)
{
    BIND(GetPrimaryModChecksumPtr, "GetPrimaryModChecksum", m_get_mod_checksum );
    BIND(GetPrimaryModIndexPtr, "GetPrimaryModIndex", m_get_mod_index );
    BIND(GetPrimaryModNamePtr, "GetPrimaryModName", m_get_mod_name );
    BIND(GetPrimaryModCountPtr, "GetPrimaryModCount", m_get_mod_count );
    BIND(GetPrimaryModArchivePtr, "GetPrimaryModArchive", m_get_mod_archive );

    BIND(GetSideCountPtr, "GetSideCount", m_get_side_count );
    BIND(GetSideNamePtr, "GetSideName", m_get_side_name );

    BIND(GetPrimaryModShortNamePtr, "GetPrimaryModShortName", m_get_primary_mod_short_name );
    BIND(GetPrimaryModVersionPtr, "GetPrimaryModVersion", m_get_primary_mod_version );
    BIND(GetPrimaryModMutatorPtr, "GetPrimaryModMutator", m_get_primary_mod_mutator );
    BIND(GetPrimaryModGamePtr, "GetPrimaryModGame", m_get_primary_mod_game );
    BIND(GetPrimaryModShortGamePtr, "GetPrimaryModShortGame", m_get_primary_mod_short_game );
    BIND(GetPrimaryModDescriptionPtr, "GetPrimaryModDescription", m_get_primary_mod_description );
    BIND(GetPrimaryModArchivePtr, "GetPrimaryModArchive", m_get_primary_mod_archive );
    BIND(GetPrimaryModArchiveCountPtr, "GetPrimaryModArchiveCount",m_get_primary_mod_archive_count );
    BIND(GetPrimaryModArchiveListPtr, "GetPrimaryModArchiveList", m_get_primary_mod_archive_list );
    BIND(GetPrimaryModChecksumFromNamePtr, "GetPrimaryModChecksumFromName",m_get_primary_mod_checksum_from_name );

    BIND(GetModValidMapCountPtr, "GetModValidMapCount", m_get_mod_valid_map_count );
    BIND(GetModValidMapPtr, "GetModValidMap", m_get_valid_map );

    BIND(GetUnitCountPtr, "GetUnitCount", m_get_unit_count );
    BIND(GetUnitNamePtr, "GetUnitName", m_get_unit_name );
    BIND(GetFullUnitNamePtr, "GetFullUnitName", m_get_unit_full_name );
    BIND(ProcessUnitsNoChecksumPtr, "ProcessUnitsNoChecksum", m_proc_units_nocheck );
}



} // namespace LSL
