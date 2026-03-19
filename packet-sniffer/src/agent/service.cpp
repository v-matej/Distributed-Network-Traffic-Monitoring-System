#include "agent/service.hpp"

#include <unistd.h>

#include <array>
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
      capture_service_(std::move(capture_service)) {
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

sniffer::CaptureResult AgentService::start_capture(const sniffer::CaptureConfig& config) const {
    return capture_service_->run_capture(config);
}

}  // namespace agent