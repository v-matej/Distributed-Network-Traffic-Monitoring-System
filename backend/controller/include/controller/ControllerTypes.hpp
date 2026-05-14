#pragma once

#include <cstdint>
#include <ctime>
#include <string>
#include <vector>

namespace controller {

struct ControllerConfig {
    std::string controller_name = "controller";
    std::string version = "0.1.0";
};

struct AgentEndpoint {
    std::string host;
    int port = 8080;
};

struct KnownAgent {
    std::string agent_id;
    std::string display_name;
    std::string host;
    int port = 8080;
    std::time_t created_at = 0;
};

struct AddAgentRequest {
    std::string display_name;
    std::string host;
    int port = 8080;
};

struct RemoteHealthInfo {
    std::string status;
    std::string agent_name;
    std::string version;
    std::string hostname;
};

struct RemoteInterfaceInfo {
    std::string name;
    std::string description;
};

struct RemoteCaptureRequest {
    std::string interface_name;
    std::string filter_expression;
    int packet_count = 0;
    int duration_seconds = 0;
};

struct RemoteCaptureConfig {
    std::string interface_name;
    std::string output_file;
    std::string filter_expression;
    int packet_count = 0;
    int duration_seconds = 0;
    bool live_output = false;
};

struct RemoteCaptureResult {
    bool success = false;
    std::string interface_name;
    std::string output_file;
    std::string filter_expression;
    std::uint64_t packets_captured = 0;
    std::uint64_t bytes_captured = 0;
    std::string stop_reason;
    std::time_t start_time = 0;
    std::time_t end_time = 0;
    std::string error_message;
};

struct RemoteCaptureSessionInfo {
    std::string capture_id;
    std::string status;
    bool stop_requested = false;
    RemoteCaptureConfig config;
    RemoteCaptureResult result;
    std::time_t created_at = 0;
    std::time_t started_at = 0;
    std::time_t finished_at = 0;
};

struct KnownAgentWithHealth {
    KnownAgent agent;
    RemoteHealthInfo health;
};

}  // namespace controller
