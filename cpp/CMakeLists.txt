cmake_minimum_required(VERSION 3.20)
project(convallaria CXX C)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Release)
endif()

include(FetchContent)
set(BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(BUILD_GAMES OFF CACHE BOOL "" FORCE)
FetchContent_Declare(raylib
  URL https://github.com/raysan5/raylib/archive/refs/tags/5.5.tar.gz)
FetchContent_MakeAvailable(raylib)

add_executable(convallaria src/main.cpp)
target_link_libraries(convallaria PRIVATE raylib)
# Assets are read straight from the repo root (the old Godot asset folders).
target_compile_definitions(convallaria PRIVATE ASSET_DIR="${CMAKE_CURRENT_SOURCE_DIR}/..")
if(MINGW)
  target_link_options(convallaria PRIVATE -static)
endif()
