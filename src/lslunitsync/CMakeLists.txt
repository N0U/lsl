SET(libUnitsyncSrc
	"${CMAKE_CURRENT_SOURCE_DIR}/c_api.cpp"
	"${CMAKE_CURRENT_SOURCE_DIR}/sharedlib.cpp"
	"${CMAKE_CURRENT_SOURCE_DIR}/image.cpp"
	"${CMAKE_CURRENT_SOURCE_DIR}/loader.cpp"
	"${CMAKE_CURRENT_SOURCE_DIR}/mmoptionmodel.cpp"
	"${CMAKE_CURRENT_SOURCE_DIR}/optionswrapper.cpp"
	"${CMAKE_CURRENT_SOURCE_DIR}/unitsync.cpp"
	)
FILE( GLOB RECURSE libUnitsyncHeader "${CMAKE_CURRENT_SOURCE_DIR}/*.h" )

#grep -lR cc\"$ "${CMAKE_CURRENT_SOURCE_DIR}/* | sed -e "s/\.h/\.cc/g"
SET( templatesources
	""
)
LIST( APPEND libUnitsyncHeader ${templatesources} )
set_source_files_properties(  ${libUnitsyncHeader} PROPERTIES HEADER_FILE_ONLY 1 )
	
#cimage deps
FIND_PACKAGE(PNG REQUIRED)
FIND_PACKAGE(X11 REQUIRED)
ADD_DEFINITIONS( -Dcimg_use_png )
ADD_LIBRARY(lsl-unitsync STATIC ${libUnitsyncHeader} ${libUnitsyncSrc} )
if( WIN32 )
	TARGET_LINK_LIBRARIES(lsl-unitsync ${CMAKE_DL_LIBS})
endif()
TARGET_LINK_LIBRARIES(lsl-unitsync lsl-utils ${Boost_LIBRARIES} ${PNG_LIBRARY} ${X11_LIBRARIES})
