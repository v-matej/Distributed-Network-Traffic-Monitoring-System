cmake_minimum_required(VERSION 3.16)

project(packet_sniffer LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_library(sniffer_core STATIC
    src/sniffer/capture.cpp
)

target_include_directories(sniffer_core
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
        ${CMAKE_CURRENT_SOURCE_DIR}/external
)

target_compile_options(sniffer_core PRIVATE
    -Wall -Wextra -Wpedantic
)

target_link_libraries(sniffer_core
    PUBLIC pcap
)

add_library(agent_core STATIC
    src/agent/AgentCaptureManager.cpp
    src/agent/AgentService.cpp
    src/agent/AgentJsonMapper.cpp
)

target_include_directories(agent_core
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
        ${CMAKE_CURRENT_SOURCE_DIR}/external
)

target_compile_options(agent_core PRIVATE
    -Wall -Wextra -Wpedantic
)

target_link_libraries(agent_core
    PUBLIC sniffer_core
)

add_library(agent_http STATIC
    src/agent/AgentHttpServer.cpp
)

target_include_directories(agent_http
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
        ${CMAKE_CURRENT_SOURCE_DIR}/external
)

target_compile_options(agent_http PRIVATE
    -Wall -Wextra -Wpedantic
)

target_link_libraries(agent_http
    PUBLIC agent_core
)

add_executable(packet_sniffer
    src/cli/main.cpp
    src/cli/cli.cpp
)

target_compile_options(packet_sniffer PRIVATE
    -Wall -Wextra -Wpedantic
)

target_link_libraries(packet_sniffer
    PRIVATE sniffer_core
)

add_executable(agent_server
    src/agent_app/main.cpp
)

target_compile_options(agent_server PRIVATE
    -Wall -Wextra -Wpedantic
)

target_link_libraries(agent_server
    PRIVATE agent_http agent_core sniffer_core
)
