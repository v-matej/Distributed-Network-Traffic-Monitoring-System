#pragma once

#include "agent/AgentCaptureManager.hpp"
#include "agent/AgentTypes.hpp"
#include "sniffer/capture_service.hpp"
#include "sniffer/types.hpp"

#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace agent {

class AgentService {
public:
    AgentService(
        AgentConfig config,
        std::shared_ptr<sniffer::ICaptureService> capture_service
    );

    [[nodiscard]] HealthInfo get_health() const;
    std::vector<sniffer::InterfaceInfo> get_interfaces(std::string& error_message) const;

    CaptureSessionInfo start_capture_session(const sniffer::CaptureConfig& config);
    std::optional<CaptureSessionInfo> get_capture_session(const std::string& capture_id) const;
    std::vector<CaptureSessionInfo> list_capture_sessions() const;
    bool stop_capture_session(const std::string& capture_id, std::string& error_message);

private:
    AgentConfig config_;
    std::shared_ptr<sniffer::ICaptureService> capture_service_;
    std::shared_ptr<AgentCaptureManager> capture_manager_;
};

}  // namespace agent
