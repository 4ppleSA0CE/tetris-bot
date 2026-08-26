cmake_minimum_required(VERSION 3.20)
project(tetris_bot CXX)

set(CMAKE_CXX_STANDARD 20)

set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE RelWithDebInfo CACHE STRING "" FORCE)
endif()

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
    core/pc.cpp
)

set(TB_BINDINGS_SOURCES
    bindings/bot_instance.cpp
)

set(CMAKE_INTERPROCEDURAL_OPTIMIZATION ON)

add_library(tb_options INTERFACE)
target_include_directories(tb_options INTERFACE ${CMAKE_CURRENT_SOURCE_DIR})
target_compile_options(tb_options INTERFACE -Wall -Wextra -Wconversion -Wsign-conversion)

add_executable(tetris_bot native/main.cpp ${TB_CORE_SOURCES})
target_link_libraries(tetris_bot PRIVATE tb_options)

add_executable(tb_tests tests/tests.cpp ${TB_CORE_SOURCES} ${TB_BINDINGS_SOURCES})
target_link_libraries(tb_tests PRIVATE tb_options)
target_compile_options(tb_tests PRIVATE -UNDEBUG)

enable_testing()
add_test(NAME tb_tests COMMAND tb_tests)
