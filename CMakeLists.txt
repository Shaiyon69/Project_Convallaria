cmake_minimum_required(VERSION 3.20)
project(convallaria CXX C)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Release)
endif()

if(EMSCRIPTEN)
  set(PLATFORM Web CACHE STRING "" FORCE)
endif()

include(FetchContent)
set(BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(BUILD_GAMES OFF CACHE BOOL "" FORCE)
FetchContent_Declare(raylib
  URL https://github.com/raysan5/raylib/archive/refs/tags/5.5.tar.gz)
FetchContent_MakeAvailable(raylib)

add_executable(convallaria src/main.cpp)
target_link_libraries(convallaria PRIVATE raylib)
# Assets come from assets/ beside the exe when present (shipped builds), else from the repo.
if(EMSCRIPTEN)
  # Browser build (also the phone version): assets packed into index.data, saves kept in IndexedDB.
  target_compile_definitions(convallaria PRIVATE ASSET_DIR="/assets")
  set_target_properties(convallaria PROPERTIES OUTPUT_NAME index SUFFIX ".html")
  target_link_options(convallaria PRIVATE -sUSE_GLFW=3 -sASYNCIFY -sALLOW_MEMORY_GROWTH=1 -sSTACK_SIZE=8MB -sEXPORTED_RUNTIME_METHODS=HEAPF32 -lidbfs.js
    "--preload-file=${CMAKE_CURRENT_SOURCE_DIR}/assets@/assets" "--shell-file=${CMAKE_CURRENT_SOURCE_DIR}/web/shell.html")
else()
  target_compile_definitions(convallaria PRIVATE ASSET_DIR="${CMAKE_CURRENT_SOURCE_DIR}/assets")
endif()
if(MINGW)
  target_link_options(convallaria PRIVATE -static -mwindows)  # no console window for players
endif()
