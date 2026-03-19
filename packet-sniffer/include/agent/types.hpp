#pragma once

#include <string>

namespace agent {

struct AgentConfig {
    std::string agent_name = "agent";
    std::string version = "0.1.0";
};

struct HealthInfo {
    std::string status;
    std::string agent_name;
    std::string version;
    std::string hostname;
};

}  // namespace agent