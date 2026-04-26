#pragma once

#include "controller/ControllerTypes.hpp"

#include <string>
#include <vector>

namespace controller {

std::string make_error_json(const std::string& message);

std::string to_json(const KnownAgent& agent);
std::string to_json(const std::vector<KnownAgent>& agents);
std::string to_json(const KnownAgentWithHealth& agent_with_health);
std::string to_json(const KnownAgent& agent, const std::vector<RemoteInterfaceInfo>& interfaces);
std::string to_json(const KnownAgent& agent, const RemoteCaptureSessionInfo& capture);
std::string to_json(const KnownAgent& agent, const std::vector<RemoteCaptureSessionInfo>& captures);

bool parse_add_agent_request_json(
    const std::string& request_body,
    AddAgentRequest& request,
    std::string& error_message
);

bool parse_remote_capture_request_json(
    const std::string& request_body,
    RemoteCaptureRequest& request,
    std::string& error_message
);

std::string to_agent_capture_request_json(const RemoteCaptureRequest& request);

bool parse_remote_health_json(
    const std::string& response_body,
    RemoteHealthInfo& health,
    std::string& error_message
);

bool parse_remote_interfaces_json(
    const std::string& response_body,
    std::vector<RemoteInterfaceInfo>& interfaces,
    std::string& error_message
);

bool parse_remote_capture_json(
    const std::string& response_body,
    RemoteCaptureSessionInfo& capture,
    std::string& error_message
);

bool parse_remote_captures_json(
    const std::string& response_body,
    std::vector<RemoteCaptureSessionInfo>& captures,
    std::string& error_message
);

bool parse_error_message_json(
    const std::string& response_body,
    std::string& message
);

}  // namespace controller
