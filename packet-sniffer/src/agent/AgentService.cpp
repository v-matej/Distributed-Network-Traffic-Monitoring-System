#include "agent/AgentService.hpp"

#include <unistd.h>

#include <algorithm>
#include <array>
#include <optional>
#include <stdexcept>
#include <utility>

namespace agent {

namespace {
std::string get_hostname() {
    std::array<char, 256> buffer {};

    if (gethostname(buffer.data(), buffer.size()) != 0) {
        return "unknown";
    }

    buffer.back() = '\0';
    return std::string(buffer.data());
}
}  // namespace

AgentService::AgentService(
    AgentConfig config,
    std::shared_ptr<sniffer::ICaptureService> capture_service
)
    : config_(std::move(config)),
      capture_service_(std::move(capture_service)),
      capture_manager_(std::make_shared<AgentCaptureManager>(
          capture_service_,
          config_.capture_storage_directory
      )) {
    if (!capture_service_) {
        throw std::invalid_argument("capture_service must not be null");
    }
}

AgentService::~AgentService() {
    shutdown();
}

HealthInfo AgentService::get_health() const {
    HealthInfo info;
    info.status = "ok";
    info.agent_name = config_.agent_name;
    info.version = config_.version;
    info.hostname = get_hostname();
    return info;
}

std::vector<sniffer::InterfaceInfo> AgentService::get_interfaces(std::string& error_message) const {
    return capture_service_->list_interfaces(error_message);
}

bool AgentService::start_capture_session(
    const sniffer::CaptureConfig& config,
    CaptureSessionInfo& session_info,
    std::string& error_message
) {
    if (!validate_capture_request(config, error_message)) {
        return false;
    }

    return capture_manager_->start_capture(config, session_info, error_message);
}

std::optional<CaptureSessionInfo> AgentService::get_capture_session(const std::string& capture_id) const {
    return capture_manager_->get_capture_session(capture_id);
}

std::vector<CaptureSessionInfo> AgentService::list_capture_sessions() const {
    return capture_manager_->list_capture_sessions();
}

bool AgentService::stop_capture_session(const std::string& capture_id, std::string& error_message) {
    return capture_manager_->request_stop(capture_id, error_message);
}

void AgentService::shutdown() {
    if (capture_manager_) {
        capture_manager_->shutdown();
    }
}

bool AgentService::validate_capture_request(
    const sniffer::CaptureConfig& config,
    std::string& error_message
) const {
    if (config.interface_name.empty()) {
        error_message = "'interface_name' must not be empty.";
        return false;
    }

    if (config.packet_count < 0) {
        error_message = "'packet_count' must be >= 0.";
        return false;
    }

    if (config.duration_seconds < 0) {
        error_message = "'duration_seconds' must be >= 0.";
        return false;
    }

    std::string interfaces_error;
    const auto interfaces = capture_service_->list_interfaces(interfaces_error);
    if (!interfaces_error.empty()) {
        error_message = "Failed to validate interface: " + interfaces_error;
        return false;
    }

    const auto interface_it = std::find_if(
        interfaces.begin(),
        interfaces.end(),
        [&config](const sniffer::InterfaceInfo& iface) {
            return iface.name == config.interface_name;
        }
    );

    if (interface_it == interfaces.end()) {
        error_message = "Requested interface does not exist: " + config.interface_name;
        return false;
    }

    return true;
}

}  // namespace agent
