cmake_minimum_required(VERSION 3.16)

project(packet_sniffer LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(PROJECT_INCLUDE_DIRS
    ${CMAKE_CURRENT_SOURCE_DIR}/agent/include
    ${CMAKE_CURRENT_SOURCE_DIR}/controller/include
    ${CMAKE_CURRENT_SOURCE_DIR}/sniffer/include
    ${CMAKE_CURRENT_SOURCE_DIR}/external
)

add_library(sniffer_core STATIC
    sniffer/src/capture.cpp
)

target_include_directories(sniffer_core
    PUBLIC
        ${PROJECT_INCLUDE_DIRS}
)

target_compile_options(sniffer_core PRIVATE
    -Wall -Wextra -Wpedantic
)

target_link_libraries(sniffer_core
    PUBLIC pcap
)

add_library(agent_core STATIC
    agent/src/AgentCaptureManager.cpp
    agent/src/AgentService.cpp
    agent/src/AgentJsonMapper.cpp
)

target_include_directories(agent_core
    PUBLIC
        ${PROJECT_INCLUDE_DIRS}
)

target_compile_options(agent_core PRIVATE
    -Wall -Wextra -Wpedantic
)

target_link_libraries(agent_core
    PUBLIC sniffer_core
)

add_library(agent_http STATIC
    agent/src/AgentHttpServer.cpp
)

target_include_directories(agent_http
    PUBLIC
        ${PROJECT_INCLUDE_DIRS}
)

target_compile_options(agent_http PRIVATE
    -Wall -Wextra -Wpedantic
)

target_link_libraries(agent_http
    PUBLIC agent_core
)

add_library(controller_core STATIC
    controller/src/AgentRegistry.cpp
    controller/src/HttpAgentClient.cpp
    controller/src/ControllerService.cpp
    controller/src/ControllerJsonMapper.cpp
)

target_include_directories(controller_core
    PUBLIC
        ${PROJECT_INCLUDE_DIRS}
)

target_compile_options(controller_core PRIVATE
    -Wall -Wextra -Wpedantic
)

add_library(controller_http STATIC
    controller/src/ControllerHttpServer.cpp
)

target_include_directories(controller_http
    PUBLIC
        ${PROJECT_INCLUDE_DIRS}
)

target_compile_options(controller_http PRIVATE
    -Wall -Wextra -Wpedantic
)

target_link_libraries(controller_http
    PUBLIC controller_core
)

add_executable(packet_sniffer
    apps/packet_sniffer/main.cpp
    apps/packet_sniffer/cli.cpp
)

target_compile_options(packet_sniffer PRIVATE
    -Wall -Wextra -Wpedantic
)

target_link_libraries(packet_sniffer
    PRIVATE sniffer_core
)

add_executable(agent_server
    apps/agent_server/main.cpp
)

target_compile_options(agent_server PRIVATE
    -Wall -Wextra -Wpedantic
)

target_link_libraries(agent_server
    PRIVATE agent_http agent_core sniffer_core
)

add_executable(controller_server
    apps/controller_server/main.cpp
)

target_compile_options(controller_server PRIVATE
    -Wall -Wextra -Wpedantic
)

target_link_libraries(controller_server
    PRIVATE controller_http controller_core
)
