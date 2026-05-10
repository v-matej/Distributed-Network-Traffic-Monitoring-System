#pragma once

#include "agent/AgentTypes.hpp"
#include "sniffer/types.hpp"

#include <string>
#include <vector>

namespace agent {

std::string to_json(const HealthInfo& health);
std::string to_json(const std::vector<sniffer::InterfaceInfo>& interfaces);
std::string to_json(const CaptureSessionInfo& session);
std::string to_json(const std::vector<CaptureSessionInfo>& sessions);

bool parse_capture_config_json(
    const std::string& request_body,
    sniffer::CaptureConfig& config,
    std::string& error_message
);

}  // namespace agent
