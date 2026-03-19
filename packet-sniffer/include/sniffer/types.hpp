#pragma once

#include <cstdint>
#include <ctime>
#include <string>

namespace sniffer {

enum class StopReason {
    None,
    UserSignal,
    TimeLimit,
    PacketLimit,
    Error
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
        case StopReason::Error:
            return "error";
    }

    return "unknown";
}

}  // namespace sniffer