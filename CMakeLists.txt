cmake_minimum_required(VERSION 3.20)
project(vita_wormhole_prototype LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_executable(vita_wormhole_prototype
    src/main.cpp
    src/game/main_game.cpp
    src/game/game_state.cpp
    src/render/graphics.cpp
    src/render/texture.cpp
    src/platform/input.cpp
    src/platform/platform.cpp
    src/data/ftl_dat.cpp
    src/data/bxml.cpp
)

target_include_directories(vita_wormhole_prototype PRIVATE include)
