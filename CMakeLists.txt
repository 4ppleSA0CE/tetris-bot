cmake_minimum_required(VERSION 3.20)
project(tetris_bot CXX)

set(CMAKE_CXX_STANDARD 20)
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
)

# Shared include dir and warning flags. Every target links this, so a new target
# (notably the if(EMSCRIPTEN) one the wasm milestone adds) cannot silently miss
# them by forgetting to repeat two lines.
add_library(tb_options INTERFACE)
target_include_directories(tb_options INTERFACE ${CMAKE_CURRENT_SOURCE_DIR})
target_compile_options(tb_options INTERFACE -Wall -Wextra)

add_executable(tetris_bot native/main.cpp ${TB_CORE_SOURCES})
target_link_libraries(tetris_bot PRIVATE tb_options)

add_executable(tb_tests tests/tests.cpp ${TB_CORE_SOURCES})
target_link_libraries(tb_tests PRIVATE tb_options)
# RelWithDebInfo puts -DNDEBUG in CMAKE_CXX_FLAGS_RELWITHDEBINFO, which compiles
# every assert() to nothing. target_compile_options lands AFTER those flags on
# the command line, so -UNDEBUG wins and the assertions stay live.
target_compile_options(tb_tests PRIVATE -UNDEBUG)

enable_testing()
add_test(NAME tb_tests COMMAND tb_tests)
