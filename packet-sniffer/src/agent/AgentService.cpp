#include "agent/AgentService.hpp"

#include <unistd.h>

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
      capture_manager_(std::make_shared<AgentCaptureManager>(capture_service_)) {
    if (!capture_service_) {
        throw std::invalid_argument("capture_service must not be null");
    }
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

CaptureSessionInfo AgentService::start_capture_session(const sniffer::CaptureConfig& config) {
    return capture_manager_->start_capture(config);
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

}  // namespace agent
