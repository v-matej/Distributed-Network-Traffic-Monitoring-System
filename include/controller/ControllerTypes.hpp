#pragma once

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

struct KnownAgentWithHealth {
    KnownAgent agent;
    RemoteHealthInfo health;
};

}  // namespace controller
