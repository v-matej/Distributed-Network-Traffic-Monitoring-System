#include "controller/ControllerService.hpp"

#include "controller/HttpAgentClient.hpp"

#include <utility>

namespace controller {

ControllerService::ControllerService(std::shared_ptr<AgentRegistry> agent_registry)
    : agent_registry_(std::move(agent_registry)) {}

bool ControllerService::add_agent(const AddAgentRequest& request, KnownAgent& added_agent, std::string& error_message) {
    if (!agent_registry_) {
        error_message = "Agent registry is not initialized";
        return false;
    }

    return agent_registry_->add_agent(request, added_agent, error_message);
}

std::vector<KnownAgent> ControllerService::list_agents() const {
    if (!agent_registry_) {
        return {};
    }

    return agent_registry_->list_agents();
}

std::optional<KnownAgent> ControllerService::get_agent(const std::string& agent_id) const {
    if (!agent_registry_) {
        return std::nullopt;
    }

    return agent_registry_->get_agent(agent_id);
}

bool ControllerService::get_agent_health(
    const std::string& agent_id,
    KnownAgentWithHealth& result,
    std::string& error_message
) const {
    const auto agent = get_agent(agent_id);
    if (!agent.has_value()) {
        error_message = "Agent not found";
        return false;
    }

    HttpAgentClient agent_client(endpoint_from_agent(*agent));
    RemoteHealthInfo health;
    if (!agent_client.get_health(health, error_message)) {
        return false;
    }

    result.agent = *agent;
    result.health = std::move(health);
    return true;
}

bool ControllerService::get_agent_interfaces(
    const std::string& agent_id,
    KnownAgent& agent,
    std::vector<RemoteInterfaceInfo>& interfaces,
    std::string& error_message
) const {
    const auto known_agent = get_agent(agent_id);
    if (!known_agent.has_value()) {
        error_message = "Agent not found";
        return false;
    }

    HttpAgentClient agent_client(endpoint_from_agent(*known_agent));
    if (!agent_client.list_interfaces(interfaces, error_message)) {
        return false;
    }

    agent = *known_agent;
    return true;
}

AgentEndpoint ControllerService::endpoint_from_agent(const KnownAgent& agent) const {
    AgentEndpoint endpoint;
    endpoint.host = agent.host;
    endpoint.port = agent.port;
    return endpoint;
}

}  // namespace controller
