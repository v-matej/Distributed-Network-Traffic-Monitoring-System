#pragma once

#include "controller/AgentRegistry.hpp"
#include "controller/ControllerTypes.hpp"

#include <cstddef>
#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace controller {

class ControllerService {
public:
    explicit ControllerService(std::shared_ptr<AgentRegistry> agent_registry);

    bool add_agent(const AddAgentRequest& request, KnownAgent& added_agent, std::string& error_message);
    bool remove_agent(const std::string& agent_id, KnownAgent& removed_agent, std::string& error_message);
    bool clear_agents(std::size_t& cleared_count, std::string& error_message);
    std::vector<KnownAgent> list_agents() const;
    std::optional<KnownAgent> get_agent(const std::string& agent_id) const;

    bool get_agent_health(
        const std::string& agent_id,
        KnownAgentWithHealth& result,
        std::string& error_message,
        int& response_status
    ) const;

    bool get_agent_interfaces(
        const std::string& agent_id,
        KnownAgent& agent,
        std::vector<RemoteInterfaceInfo>& interfaces,
        std::string& error_message,
        int& response_status
    ) const;

    bool start_agent_capture(
        const std::string& agent_id,
        const RemoteCaptureRequest& request,
        KnownAgent& agent,
        RemoteCaptureSessionInfo& session,
        std::string& error_message,
        int& response_status
    ) const;

    bool list_agent_captures(
        const std::string& agent_id,
        KnownAgent& agent,
        std::vector<RemoteCaptureSessionInfo>& captures,
        std::string& error_message,
        int& response_status
    ) const;

    bool get_agent_capture(
        const std::string& agent_id,
        const std::string& capture_id,
        KnownAgent& agent,
        RemoteCaptureSessionInfo& session,
        std::string& error_message,
        int& response_status
    ) const;

    bool stop_agent_capture(
        const std::string& agent_id,
        const std::string& capture_id,
        KnownAgent& agent,
        RemoteCaptureSessionInfo& session,
        std::string& error_message,
        int& response_status
    ) const;

    bool download_agent_capture(
        const std::string& agent_id,
        const std::string& capture_id,
        KnownAgent& agent,
        std::string& content,
        std::string& content_type,
        std::string& error_message,
        int& response_status
    ) const;

private:
    AgentEndpoint endpoint_from_agent(const KnownAgent& agent) const;

    std::shared_ptr<AgentRegistry> agent_registry_;
};

}  // namespace controller
