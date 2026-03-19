cmake_minimum_required(VERSION 3.16)

project(packet_sniffer LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_library(sniffer_core STATIC
    src/capture.cpp
)

target_include_directories(sniffer_core
    PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_compile_options(sniffer_core PRIVATE
    -Wall -Wextra -Wpedantic
)

target_link_libraries(sniffer_core
    PUBLIC pcap
)

add_executable(packet_sniffer
    src/main.cpp
    src/cli.cpp
)

target_compile_options(packet_sniffer PRIVATE
    -Wall -Wextra -Wpedantic
)

target_link_libraries(packet_sniffer
    PRIVATE sniffer_core
)