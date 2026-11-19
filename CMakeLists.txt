cmake_minimum_required(VERSION 3.21)

project(TheWorld_MapManager LANGUAGES C CXX)

if(NOT WIN32 OR NOT MSVC)
	message(FATAL_ERROR "TheWorld_MapManager CMake build requires Windows and MSVC.")
endif()

if(NOT DEFINED THEWORLD_EXTERNAL_ROOT)
	set(THEWORLD_EXTERNAL_ROOT "${PROJECT_SOURCE_DIR}/..")
endif()
if(NOT DEFINED THEWORLD_LIB_DIR)
	set(THEWORLD_LIB_DIR "${THEWORLD_EXTERNAL_ROOT}/TheWorld_GDN_Viewer/x64")
endif()
set(THEWORLD_PROJ_LIB_DIR "${THEWORLD_EXTERNAL_ROOT}/PROJ/Bin/lib" CACHE PATH
	"Directory containing the prebuilt PROJ import libraries")

set(_include_dirs
	"${PROJECT_SOURCE_DIR}"
	"${THEWORLD_EXTERNAL_ROOT}/TheWorld_Utils"
	"${THEWORLD_EXTERNAL_ROOT}/TheWorld_GDN_Viewer/TheWorld_GDN_Viewer"
	"${THEWORLD_EXTERNAL_ROOT}/GSL/include"
	"${THEWORLD_EXTERNAL_ROOT}/jsoncpp/include"
	"${THEWORLD_EXTERNAL_ROOT}/PROJ/src"
	"${PROJECT_SOURCE_DIR}/plog/include"
	"${THEWORLD_EXTERNAL_ROOT}/eigen"
)
foreach(_include_dir IN LISTS _include_dirs)
	if(NOT IS_DIRECTORY "${_include_dir}")
		message(FATAL_ERROR "Required include directory not found: ${_include_dir}")
	endif()
endforeach()

if(TARGET proj)
	set(_proj_link proj)
else()
	set(_proj_debug "${THEWORLD_PROJ_LIB_DIR}/Debug/proj_d.lib")
	set(_proj_release "${THEWORLD_PROJ_LIB_DIR}/Release/proj.lib")
	foreach(_library IN ITEMS "${_proj_debug}" "${_proj_release}")
		if(NOT EXISTS "${_library}")
			message(FATAL_ERROR "Required prebuilt PROJ library not found: ${_library}")
		endif()
	endforeach()
	set(_proj_link "$<$<CONFIG:Debug>:${_proj_debug}>" "$<$<CONFIG:Release>:${_proj_release}>")
endif()

add_library(TheWorld_MapManager SHARED
	"${THEWORLD_EXTERNAL_ROOT}/TheWorld_GDN_Viewer/TheWorld_GDN_Viewer/Viewer_Utils.cpp"
	base64.cpp
	dbfopen.c
	DBFactory.cpp
	DBInterface.cpp
	DBJson.cpp
	DBSQLLite.cpp
	DisusedCode.cpp
	dllmain.cpp
	GroundJsonFile.cpp
	MapManager.cpp
	MapManagerException.cpp
	pch.cpp
	safileio.c
	sbnsearch.c
	shpopen.c
	shptree.c
	sqlite3.c
	MapManager_Utils.cpp
	WorldDefiner.cpp
)

target_include_directories(TheWorld_MapManager PRIVATE ${_include_dirs})
target_compile_features(TheWorld_MapManager PRIVATE c_std_11 cxx_std_20)
target_compile_definitions(TheWorld_MapManager PRIVATE
	THEWORLDMAPMANAGER_EXPORTS
	_WINDOWS
	_USRDLL
	UNICODE
	_UNICODE
	_CRT_SECURE_NO_WARNINGS
	"$<$<CONFIG:Debug>:_DEBUG>"
	"$<$<CONFIG:Release>:NDEBUG>"
)
target_compile_options(TheWorld_MapManager PRIVATE /W3 /sdl /permissive- /Zc:__cplusplus)

if(TARGET TheWorld_Utils)
	set(_utils_link TheWorld_Utils)
else()
	set(_utils_link
		"$<$<CONFIG:Debug>:${THEWORLD_LIB_DIR}/TheWorld_Utils_d.lib>"
		"$<$<CONFIG:Release>:${THEWORLD_LIB_DIR}/TheWorld_Utils.lib>")
endif()
if(TARGET lib_json)
	set(_json_link lib_json)
else()
	set(_json_link
		"$<$<CONFIG:Debug>:${THEWORLD_LIB_DIR}/lib_json_d.lib>"
		"$<$<CONFIG:Release>:${THEWORLD_LIB_DIR}/lib_json.lib>")
endif()

target_link_libraries(TheWorld_MapManager PRIVATE
	Rpcrt4
	${_utils_link}
	${_json_link}
	${_proj_link}
)

set_target_properties(TheWorld_MapManager PROPERTIES
	CXX_EXTENSIONS OFF
	MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>DLL"
	PREFIX ""
	OUTPUT_NAME_DEBUG "TheWorld_MapManager_d"
	OUTPUT_NAME_RELEASE "TheWorld_MapManager"
	RUNTIME_OUTPUT_DIRECTORY "${THEWORLD_LIB_DIR}/$<0:>"
	ARCHIVE_OUTPUT_DIRECTORY "${THEWORLD_LIB_DIR}/$<0:>"
)
