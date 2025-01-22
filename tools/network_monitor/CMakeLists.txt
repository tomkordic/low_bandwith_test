# Minimum version of CMake required
cmake_minimum_required(VERSION 3.10)

# Project name and version
project(NetworkMonitor VERSION 1.0)

# Set the C++ standard
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED True)
add_definitions(-D__STDC_CONSTANT_MACROS)

set(SRC 
    src/monitor.cpp
    src/packet_processor.cpp
    src/logger.hpp
)

# Add the executable target
add_executable(network_monitor ${SRC})

# Link the libpcap library
find_package(PkgConfig REQUIRED)
pkg_check_modules(PCAP REQUIRED libpcap)

target_include_directories(network_monitor PRIVATE ${PCAP_INCLUDE_DIRS})
target_link_libraries(network_monitor PRIVATE ${PCAP_LIBRARIES})

# Ensure libpcap is linked correctly
target_compile_options(network_monitor PRIVATE ${PCAP_CFLAGS_OTHER})
