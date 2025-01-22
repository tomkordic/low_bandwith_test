cmake_minimum_required(VERSION 3.15)
project(PlaybackVerifier)

find_package(PkgConfig REQUIRED)

pkg_check_modules(FFMPEG REQUIRED libavcodec libavformat libavutil libswscale)

include_directories(${})
link_directories(${FFMPEG_LIBRARY_DIRS})

message(FFMPEG_INCLUDE_DIRS: ${FFMPEG_INCLUDE_DIRS})

# Find libcurl
find_package(CURL REQUIRED)

# Specify the C++ standard
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED True)
add_definitions(-D__STDC_CONSTANT_MACROS)

set(SRC 
    src/main.cpp
    src/decoder.cpp
    src/hls_parser.cpp
    src/queue.hpp
    src/hls_segment.hpp
    src/logger.hpp
)

# Add the executable
add_executable(PlaybackVerifier ${SRC})
target_link_libraries(PlaybackVerifier ${FFMPEG_LIBRARIES} CURL::libcurl)
target_include_directories(PlaybackVerifier PRIVATE ${FFMPEG_INCLUDE_DIRS})