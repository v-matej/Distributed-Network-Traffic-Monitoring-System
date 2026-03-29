#pragma once

#include <atomic>
#include <cstdint>
#include <ctime>
#include <functional>
#include <memory>
#include <mutex>
#include <string>

namespace sniffer {

enum class StopReason {
    None,
    UserSignal,
    TimeLimit,
    PacketLimit,
    ExternalStop,
    Error
};

class CaptureControl {
public:
    void request_stop() {
        stop_requested_.store(true);

        std::function<void()> callback;
        {
            std::lock_guard<std::mutex> lock(mutex_);
            callback = stop_callback_;
        }

        if (callback) {
            callback();
        }
    }

    [[nodiscard]] bool is_stop_requested() const {
        return stop_requested_.load();
    }

    void set_stop_callback(std::function<void()> callback) {
        bool invoke_immediately = false;
        {
            std::lock_guard<std::mutex> lock(mutex_);
            stop_callback_ = std::move(callback);
            invoke_immediately = stop_requested_.load() && static_cast<bool>(stop_callback_);
        }

        if (invoke_immediately) {
            std::function<void()> immediate_callback;
            {
                std::lock_guard<std::mutex> lock(mutex_);
                immediate_callback = stop_callback_;
            }
            if (immediate_callback) {
                immediate_callback();
            }
        }
    }

    void clear_stop_callback() {
        std::lock_guard<std::mutex> lock(mutex_);
        stop_callback_ = {};
    }

private:
    std::atomic<bool> stop_requested_ {false};
    mutable std::mutex mutex_;
    std::function<void()> stop_callback_;
};

struct InterfaceInfo {
    std::string name;
    std::string description;
};

struct CaptureConfig {
    std::string interface_name;
    std::string output_file = "capture.pcap";
    std::string filter_expression;
    int packet_count = 0;
    int duration_seconds = 0;
    bool live_output = false;
    bool enable_signal_stop = false;
    bool enable_console_output = true;
    std::shared_ptr<CaptureControl> control;
};

struct CaptureResult {
    bool success = false;
    std::string interface_name;
    std::string output_file;
    std::string filter_expression;
    std::uint64_t packets_captured = 0;
    std::uint64_t bytes_captured = 0;
    StopReason stop_reason = StopReason::None;
    std::time_t start_time = 0;
    std::time_t end_time = 0;
    std::string error_message;
};

inline std::string to_string(StopReason reason) {
    switch (reason) {
        case StopReason::None:
            return "none";
        case StopReason::UserSignal:
            return "user_signal";
        case StopReason::TimeLimit:
            return "time_limit";
        case StopReason::PacketLimit:
            return "packet_limit";
        case StopReason::ExternalStop:
            return "external_stop";
        case StopReason::Error:
            return "error";
    }

    return "unknown";
}

}  // namespace sniffer
