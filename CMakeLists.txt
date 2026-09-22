cmake_minimum_required(VERSION 3.20)

if(NOT DEFINED CMAKE_TOOLCHAIN_FILE AND DEFINED ENV{VITASDK})
    set(CMAKE_TOOLCHAIN_FILE "$ENV{VITASDK}/share/vita.toolchain.cmake" CACHE PATH "VitaSDK toolchain")
endif()

project(vita_wormhole_prototype LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(VITA)
    include("$ENV{VITASDK}/share/vita.cmake" REQUIRED)
    set(VITA_APP_NAME "FTL for PS Vita")
    set(VITA_TITLEID "FTLV00001")
    set(VITA_VERSION "01.00")
endif()

add_library(wormhole_core
    src/game/game_state.cpp
    src/game/main_game.cpp
    src/render/graphics.cpp
    src/render/texture.cpp
    src/platform/input.cpp
    src/platform/platform.cpp
    src/data/ftl_dat.cpp
    src/data/bxml.cpp
)
target_include_directories(wormhole_core PUBLIC include)

add_executable(vita_wormhole_prototype src/main.cpp)
target_link_libraries(vita_wormhole_prototype PRIVATE wormhole_core)

if(VITA)
    target_link_libraries(vita_wormhole_prototype PRIVATE
        vitaGL
        SceCtrl_stub
        SceDisplay_stub
        SceGxm_stub
        SceTouch_stub
    )
    vita_create_self(eboot.bin vita_wormhole_prototype UNSAFE)
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
endif()
