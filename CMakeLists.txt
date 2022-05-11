cmake_minimum_required(VERSION 3.1 FATAL_ERROR)
project(emerald)

ADD_DEFINITIONS(-DUNICODE -D_UNICODE)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

set(SPDLOG_WCHAR_SUPPORT ON)
set(SPDLOG_WCHAR_FILENAMES ON)
add_subdirectory(deps/spdlog)

add_subdirectory(deps/zydis)
add_subdirectory(winx)
add_subdirectory(emerald)
