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

bool ControllerService::remove_agent(const std::string& agent_id, KnownAgent& removed_agent, std::string& error_message) {
    if (!agent_registry_) {
        error_message = "Agent registry is not initialized";
        return false;
    }

    return agent_registry_->remove_agent(agent_id, removed_agent, error_message);
}

bool ControllerService::clear_agents(std::size_t& cleared_count, std::string& error_message) {
    if (!agent_registry_) {
        error_message = "Agent registry is not initialized";
        return false;
    }

    return agent_registry_->clear_agents(cleared_count, error_message);
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
    std::string& error_message,
    int& response_status
) const {
    const auto agent = get_agent(agent_id);
    if (!agent.has_value()) {
        response_status = 404;
        error_message = "Agent not found";
        return false;
    }

    HttpAgentClient agent_client(endpoint_from_agent(*agent));
    RemoteHealthInfo health;
    if (!agent_client.get_health(health, error_message, response_status)) {
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
    std::string& error_message,
    int& response_status
) const {
    const auto known_agent = get_agent(agent_id);
    if (!known_agent.has_value()) {
        response_status = 404;
        error_message = "Agent not found";
        return false;
    }

    HttpAgentClient agent_client(endpoint_from_agent(*known_agent));
    if (!agent_client.list_interfaces(interfaces, error_message, response_status)) {
        return false;
    }

    agent = *known_agent;
    return true;
}

bool ControllerService::start_agent_capture(
    const std::string& agent_id,
    const RemoteCaptureRequest& request,
    KnownAgent& agent,
    RemoteCaptureSessionInfo& session,
    std::string& error_message,
    int& response_status
) const {
    const auto known_agent = get_agent(agent_id);
    if (!known_agent.has_value()) {
        response_status = 404;
        error_message = "Agent not found";
        return false;
    }

    HttpAgentClient agent_client(endpoint_from_agent(*known_agent));
    if (!agent_client.start_capture(request, session, error_message, response_status)) {
        return false;
    }

    agent = *known_agent;
    return true;
}

bool ControllerService::list_agent_captures(
    const std::string& agent_id,
    KnownAgent& agent,
    std::vector<RemoteCaptureSessionInfo>& captures,
    std::string& error_message,
    int& response_status
) const {
    const auto known_agent = get_agent(agent_id);
    if (!known_agent.has_value()) {
        response_status = 404;
        error_message = "Agent not found";
        return false;
    }

    HttpAgentClient agent_client(endpoint_from_agent(*known_agent));
    if (!agent_client.list_captures(captures, error_message, response_status)) {
        return false;
    }

    agent = *known_agent;
    return true;
}

bool ControllerService::get_agent_capture(
    const std::string& agent_id,
    const std::string& capture_id,
    KnownAgent& agent,
    RemoteCaptureSessionInfo& session,
    std::string& error_message,
    int& response_status
) const {
    const auto known_agent = get_agent(agent_id);
    if (!known_agent.has_value()) {
        response_status = 404;
        error_message = "Agent not found";
        return false;
    }

    HttpAgentClient agent_client(endpoint_from_agent(*known_agent));
    if (!agent_client.get_capture(capture_id, session, error_message, response_status)) {
        return false;
    }

    agent = *known_agent;
    return true;
}

bool ControllerService::stop_agent_capture(
    const std::string& agent_id,
    const std::string& capture_id,
    KnownAgent& agent,
    RemoteCaptureSessionInfo& session,
    std::string& error_message,
    int& response_status
) const {
    const auto known_agent = get_agent(agent_id);
    if (!known_agent.has_value()) {
        response_status = 404;
        error_message = "Agent not found";
        return false;
    }

    HttpAgentClient agent_client(endpoint_from_agent(*known_agent));
    if (!agent_client.stop_capture(capture_id, session, error_message, response_status)) {
        return false;
    }

    agent = *known_agent;
    return true;
}

bool ControllerService::download_agent_capture(
    const std::string& agent_id,
    const std::string& capture_id,
    KnownAgent& agent,
    std::string& content,
    std::string& content_type,
    std::string& error_message,
    int& response_status
) const {
    const auto known_agent = get_agent(agent_id);
    if (!known_agent.has_value()) {
        response_status = 404;
        error_message = "Agent not found";
        return false;
    }

    HttpAgentClient agent_client(endpoint_from_agent(*known_agent));
    if (!agent_client.download_capture(
            capture_id,
            content,
            content_type,
            error_message,
            response_status
        )) {
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
