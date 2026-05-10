#include "agent/AgentCaptureManager.hpp"

#include <chrono>
#include <ctime>
#include <filesystem>
#include <memory>
#include <mutex>
#include <optional>
#include <stdexcept>
#include <string>
#include <system_error>
#include <thread>
#include <utility>
#include <vector>

namespace agent {

namespace {
bool is_terminal_status(CaptureSessionStatus status) {
    return status == CaptureSessionStatus::Completed ||
           status == CaptureSessionStatus::Stopped ||
           status == CaptureSessionStatus::Failed;
}
}  // namespace

struct AgentCaptureManager::CaptureSessionRecord {
    mutable std::mutex mutex;
    CaptureSessionInfo info;
    std::shared_ptr<sniffer::CaptureControl> control;
    std::jthread worker;
};

AgentCaptureManager::AgentCaptureManager(
    std::shared_ptr<sniffer::ICaptureService> capture_service,
    std::string capture_storage_directory
)
    : capture_service_(std::move(capture_service)),
      capture_storage_directory_(std::move(capture_storage_directory)) {
    if (!capture_service_) {
        throw std::invalid_argument("capture_service must not be null");
    }

    if (capture_storage_directory_.empty()) {
        throw std::invalid_argument("capture_storage_directory must not be empty");
    }
}

AgentCaptureManager::~AgentCaptureManager() {
    shutdown();
}

std::string AgentCaptureManager::generate_capture_id() {
    const auto now = std::chrono::system_clock::now().time_since_epoch();
    const auto millis = std::chrono::duration_cast<std::chrono::milliseconds>(now).count();
    const auto counter = ++capture_counter_;
    return "cap-" + std::to_string(millis) + "-" + std::to_string(counter);
}

std::string AgentCaptureManager::build_output_file_path(const std::string& capture_id) const {
    const std::filesystem::path directory(capture_storage_directory_);
    const std::filesystem::path filename = capture_id + ".pcap";
    return (directory / filename).string();
}

CaptureSessionInfo AgentCaptureManager::copy_session_info(
    const std::shared_ptr<CaptureSessionRecord>& record
) const {
    std::lock_guard<std::mutex> lock(record->mutex);
    return record->info;
}

bool AgentCaptureManager::start_capture(
    const sniffer::CaptureConfig& requested_config,
    CaptureSessionInfo& session_info,
    std::string& error_message
) {
    std::error_code filesystem_error;
    std::filesystem::create_directories(capture_storage_directory_, filesystem_error);
    if (filesystem_error) {
        error_message = "Failed to create capture storage directory: " + filesystem_error.message();
        return false;
    }

    auto record = std::make_shared<CaptureSessionRecord>();
    record->control = std::make_shared<sniffer::CaptureControl>();
    record->info.capture_id = generate_capture_id();
    record->info.status = CaptureSessionStatus::Pending;
    record->info.config = requested_config;
    record->info.config.control = nullptr;
    record->info.config.output_file = build_output_file_path(record->info.capture_id);
    record->info.created_at = std::time(nullptr);

    {
        std::lock_guard<std::mutex> lock(sessions_mutex_);
        sessions_[record->info.capture_id] = record;
    }

    sniffer::CaptureConfig async_config = record->info.config;
    async_config.control = record->control;
    async_config.enable_signal_stop = false;
    async_config.enable_console_output = false;
    async_config.live_output = false;

    record->worker = std::jthread([record, capture_service = capture_service_, async_config]() mutable {
        {
            std::lock_guard<std::mutex> lock(record->mutex);
            record->info.status = CaptureSessionStatus::Running;
            record->info.started_at = std::time(nullptr);
        }

        try {
            sniffer::CaptureResult result = capture_service->run_capture(async_config);

            std::lock_guard<std::mutex> lock(record->mutex);
            record->info.result = std::move(result);
            record->info.finished_at = std::time(nullptr);

            if (!record->info.result.success) {
                record->info.status = CaptureSessionStatus::Failed;
            } else if (record->info.result.stop_reason == sniffer::StopReason::ExternalStop) {
                record->info.status = CaptureSessionStatus::Stopped;
            } else {
                record->info.status = CaptureSessionStatus::Completed;
            }
        } catch (const std::exception& ex) {
            std::lock_guard<std::mutex> lock(record->mutex);
            record->info.result.success = false;
            record->info.result.stop_reason = sniffer::StopReason::Error;
            record->info.result.error_message = ex.what();
            record->info.finished_at = std::time(nullptr);
            record->info.status = CaptureSessionStatus::Failed;
        } catch (...) {
            std::lock_guard<std::mutex> lock(record->mutex);
            record->info.result.success = false;
            record->info.result.stop_reason = sniffer::StopReason::Error;
            record->info.result.error_message = "Unknown capture worker error.";
            record->info.finished_at = std::time(nullptr);
            record->info.status = CaptureSessionStatus::Failed;
        }
    });

    session_info = copy_session_info(record);
    return true;
}

std::optional<CaptureSessionInfo> AgentCaptureManager::get_capture_session(const std::string& capture_id) const {
    std::shared_ptr<CaptureSessionRecord> record;
    {
        std::lock_guard<std::mutex> lock(sessions_mutex_);
        const auto it = sessions_.find(capture_id);
        if (it == sessions_.end()) {
            return std::nullopt;
        }
        record = it->second;
    }

    return copy_session_info(record);
}

std::vector<CaptureSessionInfo> AgentCaptureManager::list_capture_sessions() const {
    std::vector<std::shared_ptr<CaptureSessionRecord>> records;
    {
        std::lock_guard<std::mutex> lock(sessions_mutex_);
        records.reserve(sessions_.size());
        for (const auto& [_, record] : sessions_) {
            records.push_back(record);
        }
    }

    std::vector<CaptureSessionInfo> sessions;
    sessions.reserve(records.size());
    for (const auto& record : records) {
        sessions.push_back(copy_session_info(record));
    }
    return sessions;
}

bool AgentCaptureManager::request_stop(const std::string& capture_id, std::string& error_message) {
    std::shared_ptr<CaptureSessionRecord> record;
    {
        std::lock_guard<std::mutex> lock(sessions_mutex_);
        const auto it = sessions_.find(capture_id);
        if (it == sessions_.end()) {
            error_message = "Capture session not found.";
            return false;
        }
        record = it->second;
    }

    {
        std::lock_guard<std::mutex> lock(record->mutex);
        if (is_terminal_status(record->info.status)) {
            error_message = "Capture session is already finished.";
            return false;
        }

        record->info.stop_requested = true;
        if (record->info.status == CaptureSessionStatus::Pending ||
            record->info.status == CaptureSessionStatus::Running) {
            record->info.status = CaptureSessionStatus::Stopping;
        }
    }

    record->control->request_stop();
    return true;
}

void AgentCaptureManager::shutdown() {
    std::vector<std::shared_ptr<CaptureSessionRecord>> records;
    {
        std::lock_guard<std::mutex> lock(sessions_mutex_);
        records.reserve(sessions_.size());
        for (const auto& [_, record] : sessions_) {
            records.push_back(record);
        }
    }

    for (const auto& record : records) {
        bool should_request_stop = false;
        {
            std::lock_guard<std::mutex> lock(record->mutex);
            if (!is_terminal_status(record->info.status)) {
                record->info.stop_requested = true;
                if (record->info.status == CaptureSessionStatus::Pending ||
                    record->info.status == CaptureSessionStatus::Running) {
                    record->info.status = CaptureSessionStatus::Stopping;
                }
                should_request_stop = true;
            }
        }

        if (should_request_stop && record->control) {
            record->control->request_stop();
        }
    }

    {
        std::lock_guard<std::mutex> lock(sessions_mutex_);
        sessions_.clear();
    }
}

}  // namespace agent
