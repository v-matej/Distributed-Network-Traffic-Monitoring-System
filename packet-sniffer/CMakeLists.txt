cmake_minimum_required(VERSION 3.16)

project(packet_sniffer LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_executable(packet_sniffer
    src/main.cpp
    src/cli.cpp
    src/sniffer.cpp
)

target_compile_options(packet_sniffer PRIVATE
    -Wall -Wextra -Wpedantic
)

target_link_libraries(packet_sniffer PRIVATE pcap)