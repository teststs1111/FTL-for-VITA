cmake_minimum_required(VERSION 3.20)

if(NOT DEFINED CMAKE_TOOLCHAIN_FILE AND DEFINED ENV{VITASDK})
    set(CMAKE_TOOLCHAIN_FILE "$ENV{VITASDK}/share/vita.toolchain.cmake" CACHE PATH "VitaSDK toolchain")
endif()

project(vita_wormhole_prototype LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(PNG REQUIRED)

if(VITA)
    include("$ENV{VITASDK}/share/vita.cmake" REQUIRED)
    set(VITA_APP_NAME "FTL: Faster Than Light")
    set(VITA_TITLEID "FTLV00001")
    set(VITA_VERSION "01.00")
endif()

add_library(wormhole_core
    src/game/game_state.cpp
    src/audio/audio.cpp
    src/game/main_game.cpp
    src/game/ship_runtime.cpp
    src/game/combat_runtime.cpp
    src/i18n/localization.cpp
    src/render/graphics.cpp
    src/render/texture.cpp
    src/platform/input.cpp
    src/platform/platform.cpp
    src/data/ftl_dat.cpp
    src/data/bxml.cpp
    src/data/asset_store.cpp
    src/data/ship_blueprint.cpp
    src/data/blueprint_database.cpp
    src/data/ship_content.cpp
    src/data/event_database.cpp
    src/data/sector_database.cpp
    src/data/sector_graph.cpp
    src/data/last_stand_state.cpp
    src/render/png_loader.cpp
    src/render/texture_cache.cpp
    src/render/text_renderer.cpp
)
target_include_directories(wormhole_core PUBLIC include)
target_link_libraries(wormhole_core PUBLIC PNG::PNG)

add_executable(vita_wormhole_prototype src/main.cpp)
target_link_libraries(vita_wormhole_prototype PRIVATE wormhole_core)

if(VITA)
    target_link_libraries(wormhole_core PUBLIC ScePvf_stub vorbisfile vorbis ogg)
    target_link_libraries(vita_wormhole_prototype PRIVATE
        vitaGL
        vitashark
        SceShaccCgExt
        taihen_stub
        mathneon
        m
        SceAppMgr_stub
        SceCommonDialog_stub
        SceCtrl_stub
        SceDisplay_stub
        SceGxm_stub
        SceKernelDmacMgr_stub
        SceShaccCg_stub
        SceTouch_stub
        ScePvf_stub
    )
    # vita-elf-create appends SCE module metadata to PT_LOAD segment 0.
    # Keep a full page-aligned gap before the next load segment so metadata can
    # be inserted without overlapping segment 1 on newer VitaSDK/binutils.
    target_link_options(vita_wormhole_prototype PRIVATE
        -Wl,-z,max-page-size=0x10000
        -Wl,-z,common-page-size=0x1000
    )
    # vita-elf-create appends SCE module metadata to PT_LOAD segment 0.
    # Strip debug sections first so the metadata has guaranteed room in the segment.
    vita_create_self(eboot.bin vita_wormhole_prototype UNSAFE STRIPPED)
    vita_create_vpk(FTL-for-VITA.vpk ${VITA_TITLEID} eboot.bin
        VERSION ${VITA_VERSION}
        NAME ${VITA_APP_NAME}
    )
endif()

option(BUILD_TESTS "Build host-side regression tests" ON)
if(BUILD_TESTS AND NOT VITA)
    enable_testing()
    add_executable(test_data tests/test_data.cpp)
    target_link_libraries(test_data PRIVATE wormhole_core)
    add_test(NAME data COMMAND test_data)
    add_executable(test_input tests/test_input.cpp)
    target_link_libraries(test_input PRIVATE wormhole_core)
    add_test(NAME input COMMAND test_input)
    add_executable(test_localization tests/test_localization.cpp)
    target_link_libraries(test_localization PRIVATE wormhole_core)
    add_test(NAME localization COMMAND test_localization)
    add_executable(test_real_ftl_dat tests/test_real_ftl_dat.cpp)
    target_link_libraries(test_real_ftl_dat PRIVATE wormhole_core)
    add_test(NAME real_ftl_dat COMMAND test_real_ftl_dat)
    add_executable(test_sector_graph tests/test_sector_graph.cpp)
    target_link_libraries(test_sector_graph PRIVATE wormhole_core)
    add_test(NAME sector_graph COMMAND test_sector_graph)
    add_executable(test_rebel_fleet_environment tests/test_rebel_fleet_environment.cpp)
    target_link_libraries(test_rebel_fleet_environment PRIVATE wormhole_core)
    add_test(NAME rebel_fleet_environment COMMAND test_rebel_fleet_environment)
    add_executable(test_last_stand_state tests/test_last_stand_state.cpp)
    target_link_libraries(test_last_stand_state PRIVATE wormhole_core)
    add_test(NAME last_stand_state COMMAND test_last_stand_state)
    add_executable(test_reactor_power_sources tests/test_reactor_power_sources.cpp)
    target_link_libraries(test_reactor_power_sources PRIVATE wormhole_core)
    add_test(NAME reactor_power_sources COMMAND test_reactor_power_sources)
    add_executable(test_plasma_storm_power tests/test_plasma_storm_power.cpp)
    target_link_libraries(test_plasma_storm_power PRIVATE wormhole_core)
    add_test(NAME plasma_storm_power COMMAND test_plasma_storm_power)
    add_executable(test_layout_room_lookup tests/test_layout_room_lookup.cpp)
    target_link_libraries(test_layout_room_lookup PRIVATE wormhole_core)
    add_test(NAME layout_room_lookup COMMAND test_layout_room_lookup)
    add_executable(test_flak_scatter tests/test_flak_scatter.cpp)
    target_link_libraries(test_flak_scatter PRIVATE wormhole_core)
    add_test(NAME flak_scatter COMMAND test_flak_scatter)
    add_executable(test_flak_fake_projectiles tests/test_flak_fake_projectiles.cpp)
    target_link_libraries(test_flak_fake_projectiles PRIVATE wormhole_core)
    add_test(NAME flak_fake_projectiles COMMAND test_flak_fake_projectiles)
    add_executable(test_audio tests/test_audio.cpp)
    target_link_libraries(test_audio PRIVATE wormhole_core)
    add_test(NAME audio COMMAND test_audio)
endif()
