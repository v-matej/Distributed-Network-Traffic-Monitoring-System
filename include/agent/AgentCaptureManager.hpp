#pragma once

#include "agent/AgentTypes.hpp"
#include "sniffer/capture_service.hpp"

#include <atomic>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

namespace agent {

class AgentCaptureManager {
public:
    explicit AgentCaptureManager(
        std::shared_ptr<sniffer::ICaptureService> capture_service,
        std::string capture_storage_directory
    );

    ~AgentCaptureManager();

    bool start_capture(
        const sniffer::CaptureConfig& requested_config,
        CaptureSessionInfo& session_info,
        std::string& error_message
    );

    std::optional<CaptureSessionInfo> get_capture_session(const std::string& capture_id) const;
    std::vector<CaptureSessionInfo> list_capture_sessions() const;
    bool request_stop(const std::string& capture_id, std::string& error_message);
    void shutdown();

private:
    struct CaptureSessionRecord;

    std::string generate_capture_id();
    std::string build_output_file_path(const std::string& capture_id) const;
    CaptureSessionInfo copy_session_info(const std::shared_ptr<CaptureSessionRecord>& record) const;

    std::shared_ptr<sniffer::ICaptureService> capture_service_;
    std::string capture_storage_directory_;

    mutable std::mutex sessions_mutex_;
    std::unordered_map<std::string, std::shared_ptr<CaptureSessionRecord>> sessions_;
    std::atomic<std::uint64_t> capture_counter_ {0};
};

}  // namespace agent
