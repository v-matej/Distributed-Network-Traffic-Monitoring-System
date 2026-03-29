#pragma once

#include "sniffer/types.hpp"

#include <ctime>
#include <string>

namespace agent {

struct AgentConfig {
    std::string agent_name = "agent";
    std::string version = "0.1.0";
};

struct HealthInfo {
    std::string status;
    std::string agent_name;
    std::string version;
    std::string hostname;
};

enum class CaptureSessionStatus {
    Pending,
    Running,
    Stopping,
    Completed,
    Stopped,
    Failed
};

struct CaptureSessionInfo {
    std::string capture_id;
    CaptureSessionStatus status = CaptureSessionStatus::Pending;
    bool stop_requested = false;
    sniffer::CaptureConfig config;
    sniffer::CaptureResult result;
    std::time_t created_at = 0;
    std::time_t started_at = 0;
    std::time_t finished_at = 0;
};

inline std::string to_string(CaptureSessionStatus status) {
    switch (status) {
        case CaptureSessionStatus::Pending:
            return "pending";
        case CaptureSessionStatus::Running:
            return "running";
        case CaptureSessionStatus::Stopping:
            return "stopping";
        case CaptureSessionStatus::Completed:
            return "completed";
        case CaptureSessionStatus::Stopped:
            return "stopped";
        case CaptureSessionStatus::Failed:
            return "failed";
    }

    return "unknown";
}

}  // namespace agent
