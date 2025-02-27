# Minimum version of CMake required
cmake_minimum_required(VERSION 3.10)

# Project name and version
project(NetworkMayhemInterface VERSION 1.0)

# Set the C++ standard
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED True)
add_definitions(-D__STDC_CONSTANT_MACROS)

set(SRC 
    src/main.cpp
    src/interface.cpp
    src/binary_parser.cpp
    src/exceptions.cpp
)

# Add the executable target
add_executable(network_mayhem ${SRC})
