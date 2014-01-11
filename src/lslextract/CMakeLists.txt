message(STATUS ${libUnitsyncSrc})

add_executable(lslextract
	lslextract.cpp
	../lslunitsync/c_api.cpp
	../lslunitsync/sharedlib.cpp
	../lslunitsync/image.cpp
	../lslunitsync/loader.cpp
	../lslunitsync/mmoptionmodel.cpp
	../lslunitsync/optionswrapper.cpp
	../lslunitsync/unitsync.cpp

	../lslutils/misc.cpp
	../lslutils/config.cpp
	../lslutils/crc.cpp
	../lslutils/thread.cpp
	../lslutils/net.cpp
	../lslutils/globalsmanager.cpp
	../lslutils/md5.c
)
FIND_PACKAGE(PNG REQUIRED)
FIND_PACKAGE(X11 REQUIRED)

remove_definitions(-DHAVE_WX -D__WXDEBUG__ -D__WXGTK__ -DHAVE_SPRINGLOBBY=1 -DENABLE_DEBUG_REPORT-DHAVE_CONFIG_H  -DHAVE_LIBNOTIFY)
TARGET_LINK_LIBRARIES(lslextract
	${Boost_LIBRARIES}
	${PNG_LIBRARY}
	${X11_LIBRARIES}
	${CMAKE_DL_LIBS}
)

