cmake_minimum_required(VERSION 3.29)
project(D2RArchipelago VERSION 0.1.0 LANGUAGES CXX RC)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")

set(D2RLPLUGIN_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
add_subdirectory(sdk)

# Code shared by the plugin, the unit tests and the ap-probe test tool.
add_library(ap_core STATIC
	src/ap_client.cpp
	src/codex.cpp
	src/command_panel.cpp
	src/config.cpp
	src/loot.cpp
	src/personalize.cpp
	src/randomizer.cpp
	src/save_state.cpp
	src/session_log.cpp
	src/skill_table.cpp
	src/sounds.cpp
	src/sprite.cpp
	src/table.cpp
	src/websocket.cpp
)
target_include_directories(ap_core PUBLIC src third_party)
target_link_libraries(ap_core PUBLIC D2RLPlugin::D2RLPlugin winhttp)
target_compile_definitions(ap_core PUBLIC WIN32_LEAN_AND_MEAN NOMINMAX)
target_compile_options(ap_core PUBLIC /W4 /utf-8 /EHsc)

add_library(d2rl_archipelago SHARED src/plugin.cpp src/notify.cpp src/plugin.rc)
target_link_libraries(d2rl_archipelago PRIVATE ap_core winmm)
set_target_properties(d2rl_archipelago PROPERTIES OUTPUT_NAME "d2rl-archipelago")
d2rlplugin_embed_config(TARGET d2rl_archipelago FILE "${CMAKE_CURRENT_SOURCE_DIR}/archipelago.toml")

add_executable(ap_probe tools/ap_probe.cpp)
target_link_libraries(ap_probe PRIVATE ap_core)

# The setup tool reads the player's own game files with CascLib (MIT).
include(FetchContent)
set(CASC_BUILD_SHARED_LIB OFF CACHE BOOL "" FORCE)
set(CASC_BUILD_STATIC_LIB ON CACHE BOOL "" FORCE)
FetchContent_Declare(casclib
	GIT_REPOSITORY https://github.com/ladislav-zezula/CascLib.git
	GIT_TAG 38a34665624b8775bb875274b36191b21c38d97b
)
FetchContent_MakeAvailable(casclib)

add_executable(d2r_ap_setup tools/setup.cpp)
target_link_libraries(d2r_ap_setup PRIVATE ap_core casc_static mfplat mfreadwrite mfuuid ole32 shell32 version)
target_include_directories(d2r_ap_setup PRIVATE "${casclib_SOURCE_DIR}/src")
target_compile_definitions(d2r_ap_setup PRIVATE CASCLIB_NO_AUTO_LINK_LIBRARY)
set_target_properties(d2r_ap_setup PROPERTIES OUTPUT_NAME "d2r-ap-setup")

enable_testing()
add_executable(ap_tests tests/tests.cpp)
target_link_libraries(ap_tests PRIVATE ap_core)
add_test(NAME ap_tests COMMAND ap_tests)
