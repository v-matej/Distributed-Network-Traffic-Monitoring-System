#include "agent/AgentJsonMapper.hpp"

#include <nlohmann/json.hpp>

namespace agent {

using json = nlohmann::json;

namespace {
json capture_config_to_json(const sniffer::CaptureConfig& config) {
    json j;
    j["interface_name"] = config.interface_name;
    j["output_file"] = config.output_file;
    j["filter_expression"] = config.filter_expression;
    j["packet_count"] = config.packet_count;
    j["duration_seconds"] = config.duration_seconds;
    j["live_output"] = config.live_output;
    return j;
}

json capture_result_to_json(const sniffer::CaptureResult& result) {
    json j;
    j["success"] = result.success;
    j["interface_name"] = result.interface_name;
    j["output_file"] = result.output_file;
    j["filter_expression"] = result.filter_expression;
    j["packets_captured"] = result.packets_captured;
    j["bytes_captured"] = result.bytes_captured;
    j["stop_reason"] = sniffer::to_string(result.stop_reason);
    j["start_time"] = result.start_time;
    j["end_time"] = result.end_time;
    j["error_message"] = result.error_message;
    return j;
}
}  // namespace

std::string to_json(const HealthInfo& health) {
    json j;
    j["status"] = health.status;
    j["agent_name"] = health.agent_name;
    j["version"] = health.version;
    j["hostname"] = health.hostname;
    return j.dump(4);
}

std::string to_json(const std::vector<sniffer::InterfaceInfo>& interfaces) {
    json j;
    j["interfaces"] = json::array();

    for (const auto& iface : interfaces) {
        j["interfaces"].push_back({
            {"name", iface.name},
            {"description", iface.description}
        });
    }

    return j.dump(4);
}

std::string to_json(const CaptureSessionInfo& session) {
    json j;
    j["capture_id"] = session.capture_id;
    j["status"] = to_string(session.status);
    j["stop_requested"] = session.stop_requested;
    j["created_at"] = session.created_at;
    j["started_at"] = session.started_at;
    j["finished_at"] = session.finished_at;
    j["config"] = capture_config_to_json(session.config);
    j["result"] = capture_result_to_json(session.result);
    return j.dump(4);
}

std::string to_json(const std::vector<CaptureSessionInfo>& sessions) {
    json j;
    j["captures"] = json::array();

    for (const auto& session : sessions) {
        j["captures"].push_back({
            {"capture_id", session.capture_id},
            {"status", to_string(session.status)},
            {"stop_requested", session.stop_requested},
            {"created_at", session.created_at},
            {"started_at", session.started_at},
            {"finished_at", session.finished_at},
            {"config", capture_config_to_json(session.config)},
            {"result", capture_result_to_json(session.result)}
        });
    }

    return j.dump(4);
}

bool parse_capture_config_json(
    const std::string& request_body,
    sniffer::CaptureConfig& config,
    std::string& error_message
) {
    try {
        const json j = json::parse(request_body);

        if (!j.contains("interface_name") || !j["interface_name"].is_string()) {
            error_message = "Missing or invalid 'interface_name'";
            return false;
        }

        config.interface_name = j["interface_name"].get<std::string>();
        if (config.interface_name.empty()) {
            error_message = "'interface_name' must not be empty";
            return false;
        }

        if (j.contains("output_file")) {
            if (!j["output_file"].is_string()) {
                error_message = "Invalid 'output_file'";
                return false;
            }
            config.output_file = j["output_file"].get<std::string>();
        }

        if (j.contains("filter_expression")) {
            if (!j["filter_expression"].is_string()) {
                error_message = "Invalid 'filter_expression'";
                return false;
            }
            config.filter_expression = j["filter_expression"].get<std::string>();
        }

        if (j.contains("packet_count")) {
            if (!j["packet_count"].is_number_integer()) {
                error_message = "Invalid 'packet_count'";
                return false;
            }
            config.packet_count = j["packet_count"].get<int>();
            if (config.packet_count < 0) {
                error_message = "'packet_count' must be >= 0";
                return false;
            }
        }

        if (j.contains("duration_seconds")) {
            if (!j["duration_seconds"].is_number_integer()) {
                error_message = "Invalid 'duration_seconds'";
                return false;
            }
            config.duration_seconds = j["duration_seconds"].get<int>();
            if (config.duration_seconds < 0) {
                error_message = "'duration_seconds' must be >= 0";
                return false;
            }
        }

        if (j.contains("live_output")) {
            if (!j["live_output"].is_boolean()) {
                error_message = "Invalid 'live_output'";
                return false;
            }
            config.live_output = j["live_output"].get<bool>();
        }

        return true;
    } catch (const std::exception& ex) {
        error_message = std::string("Invalid JSON: ") + ex.what();
        return false;
    }
}

}  // namespace agent
