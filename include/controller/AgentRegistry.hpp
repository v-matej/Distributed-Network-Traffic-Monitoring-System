#pragma once

#include "controller/ControllerTypes.hpp"

#include <map>
#include <mutex>
#include <optional>
#include <string>
#include <vector>

namespace controller {

class AgentRegistry {
public:
    AgentRegistry() = default;

    bool add_agent(const AddAgentRequest& request, KnownAgent& added_agent, std::string& error_message);
    bool remove_agent(const std::string& agent_id, KnownAgent& removed_agent, std::string& error_message);
    std::optional<KnownAgent> get_agent(const std::string& agent_id) const;
    std::vector<KnownAgent> list_agents() const;

private:
    std::string generate_agent_id();
    static std::string trim_copy(const std::string& value);

    mutable std::mutex mutex_;
    std::map<std::string, KnownAgent> agents_;
    unsigned long long next_agent_id_ = 1;
};

}  // namespace controller
