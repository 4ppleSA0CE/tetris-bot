cmake_minimum_required(VERSION 3.20)
project(tetris_bot CXX)

set(CMAKE_CXX_STANDARD 20)

# Link-time optimization. Measured, not speculative: generateMoves is the hot path at
# ~40x the cost of a full board evaluation, and pieceCells() lives in its own translation
# unit, so every call from movegen/search is a real `bl` to an external symbol. Enabling
# LTO measured 19.7 -> 13.9 us/call (53,600 -> 71,800 calls/sec, 1.35x), and a single-TU
# build reached only 73,300 -- so this captures essentially all of the available cross-TU
# inlining rather than being a first step toward more. That 1.35x buys search depth inside
# the fixed 5 ms budget, which is what the PRD 11 targets are bounded by.
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE RelWithDebInfo CACHE STRING "" FORCE)
endif()

# Every core/*.cpp file, listed explicitly rather than globbed. Tasks append to
# this list as they add sources. The wasm milestone adds an if(EMSCRIPTEN)
# branch that reuses TB_CORE_SOURCES verbatim, so nothing here gets restructured.
set(TB_CORE_SOURCES
    core/piece.cpp
    core/board.cpp
    core/srs.cpp
    core/rng.cpp
    core/attack.cpp
    core/movegen.cpp
    core/eval.cpp
    core/search.cpp
    core/game.cpp
)

# Sources under bindings/ that the NATIVE test binary links. The wasm build in
# build.sh lists its own sources and does not read this variable. The tetris_bot
# CLI deliberately does not link these - it drives tb::Game, not BotInstance.
set(TB_BINDINGS_SOURCES
    bindings/bot_instance.cpp
)

# Shared include dir and warning flags. Every target links this, so a new target
# (notably the if(EMSCRIPTEN) one the wasm milestone adds) cannot silently miss
# them by forgetting to repeat two lines.
set(CMAKE_INTERPROCEDURAL_OPTIMIZATION ON)

add_library(tb_options INTERFACE)
target_include_directories(tb_options INTERFACE ${CMAKE_CURRENT_SOURCE_DIR})
target_compile_options(tb_options INTERFACE -Wall -Wextra -Wconversion -Wsign-conversion)

add_executable(tetris_bot native/main.cpp ${TB_CORE_SOURCES})
target_link_libraries(tetris_bot PRIVATE tb_options)

add_executable(tb_tests tests/tests.cpp ${TB_CORE_SOURCES} ${TB_BINDINGS_SOURCES})
target_link_libraries(tb_tests PRIVATE tb_options)
# RelWithDebInfo puts -DNDEBUG in CMAKE_CXX_FLAGS_RELWITHDEBINFO, which compiles
# every assert() to nothing. target_compile_options lands AFTER those flags on
# the command line, so -UNDEBUG wins and the assertions stay live.
target_compile_options(tb_tests PRIVATE -UNDEBUG)

enable_testing()
add_test(NAME tb_tests COMMAND tb_tests)
