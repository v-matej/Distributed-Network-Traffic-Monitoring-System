#include "agent/AgentCaptureManager.hpp"

#include <atomic>
#include <chrono>
#include <ctime>
#include <thread>
#include <memory>
#include <mutex>
#include <optional>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

namespace agent {

namespace {
struct CaptureSessionRecord {
    mutable std::mutex mutex;
    CaptureSessionInfo info;
    std::shared_ptr<sniffer::CaptureControl> control;
    std::jthread worker;
};

std::mutex g_sessions_mutex;
std::unordered_map<std::string, std::shared_ptr<CaptureSessionRecord>> g_sessions;
std::atomic<std::uint64_t> g_capture_counter {0};

std::string generate_capture_id() {
    const auto now = std::chrono::system_clock::now().time_since_epoch();
    const auto millis = std::chrono::duration_cast<std::chrono::milliseconds>(now).count();
    const auto counter = ++g_capture_counter;
    return "cap-" + std::to_string(millis) + "-" + std::to_string(counter);
}

CaptureSessionInfo copy_session_info(const std::shared_ptr<CaptureSessionRecord>& record) {
    std::lock_guard<std::mutex> lock(record->mutex);
    return record->info;
}

bool is_terminal_status(CaptureSessionStatus status) {
    return status == CaptureSessionStatus::Completed ||
           status == CaptureSessionStatus::Stopped ||
           status == CaptureSessionStatus::Failed;
}
}  // namespace

AgentCaptureManager::AgentCaptureManager(std::shared_ptr<sniffer::ICaptureService> capture_service)
    : capture_service_(std::move(capture_service)) {
    if (!capture_service_) {
        throw std::invalid_argument("capture_service must not be null");
    }
}

CaptureSessionInfo AgentCaptureManager::start_capture(const sniffer::CaptureConfig& config) {
    auto record = std::make_shared<CaptureSessionRecord>();
    record->control = std::make_shared<sniffer::CaptureControl>();
    record->info.capture_id = generate_capture_id();
    record->info.status = CaptureSessionStatus::Pending;
    record->info.config = config;
    record->info.config.control = nullptr;
    record->info.created_at = std::time(nullptr);

    {
        std::lock_guard<std::mutex> lock(g_sessions_mutex);
        g_sessions[record->info.capture_id] = record;
    }

    sniffer::CaptureConfig async_config = config;
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

        sniffer::CaptureResult result = capture_service->run_capture(async_config);

        {
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
        }
    });

    return copy_session_info(record);
}

std::optional<CaptureSessionInfo> AgentCaptureManager::get_capture_session(const std::string& capture_id) const {
    std::shared_ptr<CaptureSessionRecord> record;
    {
        std::lock_guard<std::mutex> lock(g_sessions_mutex);
        const auto it = g_sessions.find(capture_id);
        if (it == g_sessions.end()) {
            return std::nullopt;
        }
        record = it->second;
    }

    return copy_session_info(record);
}

std::vector<CaptureSessionInfo> AgentCaptureManager::list_capture_sessions() const {
    std::vector<std::shared_ptr<CaptureSessionRecord>> records;
    {
        std::lock_guard<std::mutex> lock(g_sessions_mutex);
        records.reserve(g_sessions.size());
        for (const auto& [_, record] : g_sessions) {
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
        std::lock_guard<std::mutex> lock(g_sessions_mutex);
        const auto it = g_sessions.find(capture_id);
        if (it == g_sessions.end()) {
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

}  // namespace agent
