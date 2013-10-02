PROJECT(libSpringLobby)

#set minimum cmake version
cmake_minimum_required(VERSION 2.8)
SET(CMAKE_COLOR_MAKEFILE ON)

# Add Definitions, Compiler-Switches, etc.: -Wall -O2 -g3 ...
# MSVC compiler (cl.exe) does not accept the same switches as gcc, although preprocessor definitions in the -D form will work for both
IF(NOT MSVC)
	IF( NOT MINGW )
		#wxMSW generates > 9000 warnings with -wextra...
# 		ADD_DEFINITIONS( -Wextra )
	ENDIF( NOT MINGW )
# 	ADD_DEFINITIONS( -Wall -Wno-strict-aliasing -pedantic )
ELSE(NOT MSVC)
	ADD_DEFINITIONS(-D_RC_MSVC)
ENDIF(NOT MSVC)

macro( install_if_standalone )
	if( NOT DEFINED springlobby_SOURCE_DIR ) #we're not included as submodule	
		install( ${ARGN} )
	endif()
endmacro()

#----------------------------------------------------------------------------------------------------
# General Settings
#----------------------------------------------------------------------------------------------------
SET( CMAKE_ALLOW_LOOSE_LOOP_CONSTRUCTS true )

#----------------------------------------------------------------------------------------------------
# Load needed Modules
#----------------------------------------------------------------------------------------------------
if(NOT DEFINED LIBSPRINGLOBBY_REV)
  EXECUTE_PROCESS(COMMAND ${libSpringLobby_SOURCE_DIR}/tools/get-revision.sh
    OUTPUT_VARIABLE LIBSPRINGLOBBY_REV
    ERROR_VARIABLE GIT_ERROR
    OUTPUT_STRIP_TRAILING_WHITESPACE
    )
endif(NOT DEFINED LIBSPRINGLOBBY_REV)


INCLUDE(cmake/package_config.cmake)
INCLUDE(cmake/sources_list.cmake)

FIND_PACKAGE( PkgConfig )

set(Boost_USE_MULTITHREADED      ON)
if( ${CMAKE_CROSSCOMPILING} )
	set(Boost_USE_STATIC_LIBS       ON)
	set(Boost_USE_STATIC_RUNTIME    ON)
endif()
FIND_PACKAGE(Boost 1.40.0 COMPONENTS system thread filesystem  REQUIRED)
INCLUDE_DIRECTORIES(${Boost_INCLUDE_DIRS})
LINK_DIRECTORIES(${Boost_LIBRARY_DIRS})


#----------------------------------------------------------------------------------------------------
# Options, that can be changed be the User in order to customise libSpringLobby
#----------------------------------------------------------------------------------------------------
OPTION(BUILD_SHARED_LIBS "Chooses whether to link dynamic or static libraries. Recommend keeping this activated unless you know what you're doing." ON)

OPTION(BUILD_TESTS "build example and test binaries" OFF)
	
SET( LIBSPRINGLOBBY_REV
	"${LIBSPRINGLOBBY_REV}" CACHE STRING
	"The revision string used in packages etc." )

# If we build for windows Systems, we also include the Resource-File containing the Manifest, Icon and other Resources.
IF(WIN32)
	CreateResourceCompileCommand(libspringlobby_RC_FILE ${libSpringLobby_SOURCE_DIR}/src/ libSpringLobby.rc lsl_icon.o )
	SET(libSpringLobbySrc ${libSpringLobbySrc} ${libspringlobby_RC_FILE})
ENDIF(WIN32)


#----------------------------------------------------------------------------------------------------
# Build target defintions
#----------------------------------------------------------------------------------------------------

# Here we define the executable lsl-server ( or on Windows libSpringLobby.exe )

#ignore warnings about macosx app bundle output dir
cmake_policy(SET CMP0006 OLD)


INCLUDE(cmake/compat_flags.cmake)

# add_custom_target(pack
# 					${CMAKE_MAKE_PROGRAM} package
# 					&& ${libSpringLobby_SOURCE_DIR}/tools/transform_release_zip.sh
# 						${CMAKE_BINARY_DIR}/${PACKAGE_NAME}-${PACKAGE_VERSION}-win32.zip
# 						${CMAKE_STRIP}
# 				)
# 
# # setup make dist
# add_custom_target(dist ${CMAKE_SOURCE_DIR}/tools/make_dist.sh "${libSpringLobby_BINARY_DIR}" "${PACKAGE_NAME}-${PACKAGE_VERSION}"
# 					 )
# 
# # setup make distcheck
# add_custom_target(distcheck + cd ${CMAKE_BINARY_DIR} &&
#                         rm -rf ${PACKAGE_NAME}-${PACKAGE_VERSION} &&
#                         tar -xzf ${PACKAGE_NAME}-${PACKAGE_VERSION}.tar.gz &&
#                         cd ${PACKAGE_NAME}-${PACKAGE_VERSION}/ &&
#                         cmake "-DCMAKE_CXX_FLAGS:STRING=-O0". && ${CMAKE_MAKE_PROGRAM}
# 					)
# add_custom_target( version
# 				${libSpringLobby_SOURCE_DIR}/tools/regen_config_header.sh ${libSpringLobby_SOURCE_DIR}/cmake/config.h ${libSpringLobby_BINARY_DIR}/libSpringLobby_config.h
# 				)
# add_dependencies( distcheck dist )
# add_dependencies( lsl-server version )

OPTION(LSLSERVER
	"Compile and install lsl-server" ON)
IF (WIN32)
	install_if_standalone(FILES AUTHORS COPYING NEWS README THANKS  DESTINATION .)
	install_if_standalone(DIRECTORY ${CMAKE_INSTALL_PREFIX}/locale DESTINATION .)
ELSE (WIN32)
	install_if_standalone(FILES AUTHORS COPYING NEWS README THANKS DESTINATION ${CMAKE_INSTALL_PREFIX}/share/doc/lsl-server )
ENDIF (WIN32)

add_subdirectory( src )
add_subdirectory( doc )
if (${BUILD_TESTS})
	enable_testing()
	add_subdirectory( test )
endif()
