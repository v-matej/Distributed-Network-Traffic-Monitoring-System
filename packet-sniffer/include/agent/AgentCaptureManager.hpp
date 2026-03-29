#pragma once

#include "agent/AgentTypes.hpp"
#include "sniffer/capture_service.hpp"

#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace agent {

class AgentCaptureManager {
public:
    explicit AgentCaptureManager(std::shared_ptr<sniffer::ICaptureService> capture_service);

    CaptureSessionInfo start_capture(const sniffer::CaptureConfig& config);
    std::optional<CaptureSessionInfo> get_capture_session(const std::string& capture_id) const;
    std::vector<CaptureSessionInfo> list_capture_sessions() const;
    bool request_stop(const std::string& capture_id, std::string& error_message);

private:
    std::shared_ptr<sniffer::ICaptureService> capture_service_;
};

}  // namespace agent
