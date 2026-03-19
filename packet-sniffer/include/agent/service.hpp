#pragma once

#include "agent/types.hpp"
#include "sniffer/capture_service.hpp"
#include "sniffer/types.hpp"

#include <memory>
#include <string>
#include <vector>

namespace agent {

class AgentService {
public:
    AgentService(
        AgentConfig config,
        std::shared_ptr<sniffer::ICaptureService> capture_service
    );

    HealthInfo get_health() const;

    std::vector<sniffer::InterfaceInfo> get_interfaces(std::string& error_message) const;

    sniffer::CaptureResult start_capture(const sniffer::CaptureConfig& config) const;

private:
    AgentConfig config_;
    std::shared_ptr<sniffer::ICaptureService> capture_service_;
};

}  // namespace agent