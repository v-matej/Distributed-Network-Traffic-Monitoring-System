#include "controller/AgentRegistry.hpp"

#include <algorithm>
#include <cctype>
#include <ctime>
#include <iomanip>
#include <sstream>

namespace controller {

bool AgentRegistry::add_agent(const AddAgentRequest& request, KnownAgent& added_agent, std::string& error_message) {
    const auto host = trim_copy(request.host);
    const auto display_name = trim_copy(request.display_name);

    if (host.empty()) {
        error_message = "'host' must not be empty";
        return false;
    }

    if (request.port <= 0 || request.port > 65535) {
        error_message = "'port' must be between 1 and 65535";
        return false;
    }

    std::lock_guard<std::mutex> lock(mutex_);

    for (const auto& [existing_id, existing_agent] : agents_) {
        (void)existing_id;
        if (existing_agent.host == host && existing_agent.port == request.port) {
            error_message = "Agent with the same host and port is already registered";
            return false;
        }
    }

    KnownAgent agent;
    agent.agent_id = generate_agent_id();
    agent.display_name = display_name;
    agent.host = host;
    agent.port = request.port;
    agent.created_at = std::time(nullptr);

    agents_[agent.agent_id] = agent;
    added_agent = agent;
    return true;
}

std::optional<KnownAgent> AgentRegistry::get_agent(const std::string& agent_id) const {
    std::lock_guard<std::mutex> lock(mutex_);
    const auto it = agents_.find(agent_id);
    if (it == agents_.end()) {
        return std::nullopt;
    }

    return it->second;
}

std::vector<KnownAgent> AgentRegistry::list_agents() const {
    std::lock_guard<std::mutex> lock(mutex_);

    std::vector<KnownAgent> agents;
    agents.reserve(agents_.size());

    for (const auto& [agent_id, agent] : agents_) {
        (void)agent_id;
        agents.push_back(agent);
    }

    return agents;
}

std::string AgentRegistry::generate_agent_id() {
    std::ostringstream id_builder;
    id_builder << "agent-" << std::setw(4) << std::setfill('0') << next_agent_id_++;
    return id_builder.str();
}

std::string AgentRegistry::trim_copy(const std::string& value) {
    auto begin = value.begin();
    auto end = value.end();

    while (begin != end && std::isspace(static_cast<unsigned char>(*begin)) != 0) {
        ++begin;
    }

    while (begin != end && std::isspace(static_cast<unsigned char>(*(end - 1))) != 0) {
        --end;
    }

    return std::string(begin, end);
}

}  // namespace controller
