cmake_minimum_required(VERSION 3.20)
project(vita_wormhole_prototype LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

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

option(BUILD_TESTS "Build host-side regression tests" ON)
if(BUILD_TESTS AND NOT DEFINED VITA)
    enable_testing()
    add_executable(test_data tests/test_data.cpp)
    target_link_libraries(test_data PRIVATE wormhole_core)
    add_test(NAME data COMMAND test_data)

    add_executable(test_input tests/test_input.cpp)
    target_link_libraries(test_input PRIVATE wormhole_core)
    add_test(NAME input COMMAND test_input)
endif()
