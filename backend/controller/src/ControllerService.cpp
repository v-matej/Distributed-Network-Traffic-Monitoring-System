#include "controller/ControllerService.hpp"

#include "controller/HttpAgentClient.hpp"
#include "controller/PcapAnalyzer.hpp"

#include <nlohmann/json.hpp>

#include <algorithm>
#include <chrono>
#include <cctype>
#include <cstdint>
#include <cstdlib>
#include <exception>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <map>
#include <string>
#include <system_error>
#include <utility>
#include <vector>

namespace controller {

using json = nlohmann::json;

namespace {

std::string sanitize_path_component(const std::string& value) {
    std::string result;
    result.reserve(value.size());

    for (const char character : value) {
        const auto unsigned_character = static_cast<unsigned char>(character);

        if (std::isalnum(unsigned_character) || character == '-' || character == '_') {
            result.push_back(character);
        } else {
            result.push_back('_');
        }
    }

    return result.empty() ? "unknown" : result;
}

std::filesystem::path make_agent_storage_directory(
    const std::string& capture_storage_root,
    const std::string& agent_id
) {
    return std::filesystem::path(capture_storage_root)
        / sanitize_path_component(agent_id);
}

std::filesystem::path make_capture_storage_path(
    const std::string& capture_storage_root,
    const std::string& agent_id,
    const std::string& capture_id
) {
    return make_agent_storage_directory(capture_storage_root, agent_id)
        / (sanitize_path_component(capture_id) + ".pcap");
}

std::filesystem::path make_capture_metadata_path(
    const std::string& capture_storage_root,
    const std::string& agent_id,
    const std::string& capture_id
) {
    return make_agent_storage_directory(capture_storage_root, agent_id)
        / (sanitize_path_component(capture_id) + ".json");
}

std::time_t file_time_to_time_t(const std::filesystem::file_time_type& file_time) {
    const auto system_time =
        std::chrono::time_point_cast<std::chrono::system_clock::duration>(
            file_time - std::filesystem::file_time_type::clock::now()
            + std::chrono::system_clock::now()
        );

    return std::chrono::system_clock::to_time_t(system_time);
}

std::time_t parse_capture_time_from_capture_id(const std::string& capture_id) {
    const std::string prefix = "cap-";

    if (capture_id.rfind(prefix, 0) != 0) {
        return 0;
    }

    std::size_t begin = prefix.size();
    std::size_t end = begin;

    while (end < capture_id.size()
           && std::isdigit(static_cast<unsigned char>(capture_id[end])) != 0) {
        ++end;
    }

    if (end == begin) {
        return 0;
    }

    const auto timestamp_text = capture_id.substr(begin, end - begin);

    char* end_pointer = nullptr;
    const auto raw_value = std::strtoull(timestamp_text.c_str(), &end_pointer, 10);

    if (end_pointer == timestamp_text.c_str() || raw_value == 0) {
        return 0;
    }

    std::uint64_t seconds = raw_value;

    if (timestamp_text.size() >= 13) {
        seconds = raw_value / 1000;
    }

    constexpr std::uint64_t minimum_reasonable_time = 1577836800ULL;
    constexpr std::uint64_t maximum_reasonable_time = 4102444800ULL;

    if (seconds < minimum_reasonable_time || seconds > maximum_reasonable_time) {
        return 0;
    }

    return static_cast<std::time_t>(seconds);
}

ControllerStoredCaptureInfo make_stored_capture_info(
    const std::string& agent_id,
    const std::string& capture_id,
    const std::filesystem::path& local_path
) {
    ControllerStoredCaptureInfo info;
    info.agent_id = agent_id;
    info.capture_id = capture_id;
    info.local_path = local_path.string();

    std::error_code error;
    info.exists = std::filesystem::exists(local_path, error)
        && std::filesystem::is_regular_file(local_path, error);

    if (info.exists) {
        info.file_size_bytes =
            static_cast<std::uint64_t>(std::filesystem::file_size(local_path, error));

        if (!error) {
            info.fetched_at = file_time_to_time_t(std::filesystem::last_write_time(local_path));
        }
    }

    return info;
}

json capture_config_to_json_value(const RemoteCaptureConfig& config) {
    json j;
    j["interface_name"] = config.interface_name;
    j["output_file"] = config.output_file;
    j["filter_expression"] = config.filter_expression;
    j["packet_count"] = config.packet_count;
    j["duration_seconds"] = config.duration_seconds;
    j["live_output"] = config.live_output;
    return j;
}

json capture_result_to_json_value(const RemoteCaptureResult& result) {
    json j;
    j["success"] = result.success;
    j["interface_name"] = result.interface_name;
    j["output_file"] = result.output_file;
    j["filter_expression"] = result.filter_expression;
    j["packets_captured"] = result.packets_captured;
    j["bytes_captured"] = result.bytes_captured;
    j["stop_reason"] = result.stop_reason;
    j["start_time"] = result.start_time;
    j["end_time"] = result.end_time;
    j["error_message"] = result.error_message;
    return j;
}

json capture_to_json_value(const RemoteCaptureSessionInfo& capture) {
    json j;
    j["capture_id"] = capture.capture_id;
    j["status"] = capture.status;
    j["stop_requested"] = capture.stop_requested;
    j["created_at"] = capture.created_at;
    j["started_at"] = capture.started_at;
    j["finished_at"] = capture.finished_at;
    j["config"] = capture_config_to_json_value(capture.config);
    j["result"] = capture_result_to_json_value(capture.result);
    return j;
}

bool read_string_field(
    const json& j,
    const char* field_name,
    std::string& value,
    std::string& error_message
) {
    if (!j.contains(field_name) || !j[field_name].is_string()) {
        error_message = std::string("Missing or invalid '") + field_name + "'";
        return false;
    }

    value = j[field_name].get<std::string>();
    return true;
}

bool read_bool_field(
    const json& j,
    const char* field_name,
    bool& value,
    std::string& error_message
) {
    if (!j.contains(field_name) || !j[field_name].is_boolean()) {
        error_message = std::string("Missing or invalid '") + field_name + "'";
        return false;
    }

    value = j[field_name].get<bool>();
    return true;
}

bool read_int_field(
    const json& j,
    const char* field_name,
    int& value,
    std::string& error_message
) {
    if (!j.contains(field_name) || !j[field_name].is_number_integer()) {
        error_message = std::string("Missing or invalid '") + field_name + "'";
        return false;
    }

    value = j[field_name].get<int>();
    return true;
}

bool read_time_field(
    const json& j,
    const char* field_name,
    std::time_t& value,
    std::string& error_message
) {
    if (!j.contains(field_name) || !j[field_name].is_number_integer()) {
        error_message = std::string("Missing or invalid '") + field_name + "'";
        return false;
    }

    value = static_cast<std::time_t>(j[field_name].get<long long>());
    return true;
}

bool read_uint64_field(
    const json& j,
    const char* field_name,
    std::uint64_t& value,
    std::string& error_message
) {
    if (!j.contains(field_name) ||
        !(j[field_name].is_number_unsigned() || j[field_name].is_number_integer())) {
        error_message = std::string("Missing or invalid '") + field_name + "'";
        return false;
    }

    value = j[field_name].get<std::uint64_t>();
    return true;
}

bool parse_capture_config_json(
    const json& j,
    RemoteCaptureConfig& config,
    std::string& error_message
) {
    if (!j.is_object()) {
        error_message = "Invalid capture config metadata";
        return false;
    }

    return read_string_field(j, "interface_name", config.interface_name, error_message)
        && read_string_field(j, "output_file", config.output_file, error_message)
        && read_string_field(j, "filter_expression", config.filter_expression, error_message)
        && read_int_field(j, "packet_count", config.packet_count, error_message)
        && read_int_field(j, "duration_seconds", config.duration_seconds, error_message)
        && read_bool_field(j, "live_output", config.live_output, error_message);
}

bool parse_capture_result_json(
    const json& j,
    RemoteCaptureResult& result,
    std::string& error_message
) {
    if (!j.is_object()) {
        error_message = "Invalid capture result metadata";
        return false;
    }

    return read_bool_field(j, "success", result.success, error_message)
        && read_string_field(j, "interface_name", result.interface_name, error_message)
        && read_string_field(j, "output_file", result.output_file, error_message)
        && read_string_field(j, "filter_expression", result.filter_expression, error_message)
        && read_uint64_field(j, "packets_captured", result.packets_captured, error_message)
        && read_uint64_field(j, "bytes_captured", result.bytes_captured, error_message)
        && read_string_field(j, "stop_reason", result.stop_reason, error_message)
        && read_time_field(j, "start_time", result.start_time, error_message)
        && read_time_field(j, "end_time", result.end_time, error_message)
        && read_string_field(j, "error_message", result.error_message, error_message);
}

bool parse_capture_json(
    const json& j,
    RemoteCaptureSessionInfo& capture,
    std::string& error_message
) {
    if (!j.is_object()) {
        error_message = "Invalid capture metadata";
        return false;
    }

    if (!read_string_field(j, "capture_id", capture.capture_id, error_message)
        || !read_string_field(j, "status", capture.status, error_message)
        || !read_bool_field(j, "stop_requested", capture.stop_requested, error_message)
        || !read_time_field(j, "created_at", capture.created_at, error_message)
        || !read_time_field(j, "started_at", capture.started_at, error_message)
        || !read_time_field(j, "finished_at", capture.finished_at, error_message)) {
        return false;
    }

    if (!j.contains("config") ||
        !parse_capture_config_json(j["config"], capture.config, error_message)) {
        return false;
    }

    if (!j.contains("result") ||
        !parse_capture_result_json(j["result"], capture.result, error_message)) {
        return false;
    }

    return true;
}

RemoteCaptureSessionInfo make_synthetic_capture_from_stored_file(
    const ControllerStoredCaptureInfo& stored_capture
) {
    const auto capture_time_from_id =
        parse_capture_time_from_capture_id(stored_capture.capture_id);

    const auto best_time =
        capture_time_from_id > 0 ? capture_time_from_id : stored_capture.fetched_at;

    RemoteCaptureSessionInfo capture;
    capture.capture_id = stored_capture.capture_id;
    capture.status = "completed";
    capture.stop_requested = false;

    capture.created_at = best_time;
    capture.started_at = best_time;
    capture.finished_at = best_time;

    capture.config.interface_name = "";
    capture.config.output_file = stored_capture.local_path;
    capture.config.filter_expression = "";
    capture.config.packet_count = 0;
    capture.config.duration_seconds = 0;
    capture.config.live_output = false;

    capture.result.success = true;
    capture.result.interface_name = "";
    capture.result.output_file = stored_capture.local_path;
    capture.result.filter_expression = "";
    capture.result.packets_captured = 0;
    capture.result.bytes_captured = stored_capture.file_size_bytes;
    capture.result.stop_reason = "controller_storage_fallback";
    capture.result.start_time = best_time;
    capture.result.end_time = best_time;
    capture.result.error_message = "";

    return capture;
}

bool save_capture_metadata(
    const std::filesystem::path& metadata_path,
    const std::string& agent_id,
    const RemoteCaptureSessionInfo& capture,
    std::string& error_message
) {
    std::error_code filesystem_error;
    std::filesystem::create_directories(metadata_path.parent_path(), filesystem_error);

    if (filesystem_error) {
        error_message =
            "Failed to create capture metadata directory: "
            + filesystem_error.message();
        return false;
    }

    json document;
    document["schema_version"] = 1;
    document["agent_id"] = agent_id;
    document["capture_id"] = capture.capture_id;
    document["capture"] = capture_to_json_value(capture);

    const auto temporary_path = metadata_path.string() + ".tmp";

    std::ofstream output(temporary_path, std::ios::trunc);
    if (!output) {
        error_message = "Failed to open temporary capture metadata file for writing";
        return false;
    }

    output << document.dump(4);
    output.close();

    if (!output) {
        error_message = "Failed to write capture metadata";
        return false;
    }

    std::filesystem::remove(metadata_path, filesystem_error);
    filesystem_error.clear();

    std::filesystem::rename(temporary_path, metadata_path, filesystem_error);
    if (filesystem_error) {
        std::filesystem::remove(temporary_path);
        error_message = "Failed to finalize capture metadata file: "
            + filesystem_error.message();
        return false;
    }

    return true;
}

bool load_capture_metadata(
    const std::filesystem::path& metadata_path,
    const std::string& expected_agent_id,
    const std::string& expected_capture_id,
    RemoteCaptureSessionInfo& capture,
    std::string& error_message
) {
    std::ifstream input(metadata_path);
    if (!input) {
        error_message = "Failed to open capture metadata file";
        return false;
    }

    json document;
    try {
        input >> document;
    } catch (const std::exception& ex) {
        error_message = std::string("Invalid capture metadata JSON: ") + ex.what();
        return false;
    }

    if (!document.is_object()) {
        error_message = "Invalid capture metadata root";
        return false;
    }

    std::string agent_id;
    std::string capture_id;

    if (!read_string_field(document, "agent_id", agent_id, error_message)
        || !read_string_field(document, "capture_id", capture_id, error_message)) {
        return false;
    }

    if (agent_id != expected_agent_id) {
        error_message = "Capture metadata belongs to another agent";
        return false;
    }

    if (capture_id != expected_capture_id) {
        error_message = "Capture metadata belongs to another capture";
        return false;
    }

    if (!document.contains("capture")) {
        error_message = "Missing capture object in metadata";
        return false;
    }

    if (!parse_capture_json(document["capture"], capture, error_message)) {
        return false;
    }

    return true;
}

bool load_stored_capture_session(
    const std::string& capture_storage_root,
    const std::string& agent_id,
    const std::string& capture_id,
    RemoteCaptureSessionInfo& capture
) {
    const auto pcap_path =
        make_capture_storage_path(capture_storage_root, agent_id, capture_id);

    const auto stored_capture =
        make_stored_capture_info(agent_id, capture_id, pcap_path);

    if (!stored_capture.exists) {
        return false;
    }

    const auto metadata_path =
        make_capture_metadata_path(capture_storage_root, agent_id, capture_id);

    std::error_code filesystem_error;
    if (std::filesystem::exists(metadata_path, filesystem_error)
        && std::filesystem::is_regular_file(metadata_path, filesystem_error)) {
        std::string metadata_error;
        if (load_capture_metadata(
                metadata_path,
                agent_id,
                capture_id,
                capture,
                metadata_error
            )) {
            return true;
        }
    }

    capture = make_synthetic_capture_from_stored_file(stored_capture);
    return true;
}

std::vector<RemoteCaptureSessionInfo> list_stored_capture_sessions_for_agent(
    const std::string& capture_storage_root,
    const std::string& agent_id
) {
    std::vector<RemoteCaptureSessionInfo> captures;

    const auto agent_directory =
        make_agent_storage_directory(capture_storage_root, agent_id);

    std::error_code filesystem_error;
    if (!std::filesystem::exists(agent_directory, filesystem_error)
        || !std::filesystem::is_directory(agent_directory, filesystem_error)) {
        return captures;
    }

    for (const auto& entry : std::filesystem::directory_iterator(agent_directory, filesystem_error)) {
        if (filesystem_error) {
            break;
        }

        if (!entry.is_regular_file() || entry.path().extension() != ".pcap") {
            continue;
        }

        const auto capture_id = entry.path().stem().string();

        RemoteCaptureSessionInfo capture;
        if (load_stored_capture_session(capture_storage_root, agent_id, capture_id, capture)) {
            captures.push_back(std::move(capture));
        }
    }

    std::sort(
        captures.begin(),
        captures.end(),
        [](const RemoteCaptureSessionInfo& left, const RemoteCaptureSessionInfo& right) {
            return left.created_at > right.created_at;
        }
    );

    return captures;
}

std::vector<RemoteCaptureSessionInfo> merge_live_and_stored_captures(
    const std::vector<RemoteCaptureSessionInfo>& live_captures,
    const std::vector<RemoteCaptureSessionInfo>& stored_captures
) {
    std::map<std::string, RemoteCaptureSessionInfo> merged;

    for (const auto& stored_capture : stored_captures) {
        merged[stored_capture.capture_id] = stored_capture;
    }

    for (const auto& live_capture : live_captures) {
        merged[live_capture.capture_id] = live_capture;
    }

    std::vector<RemoteCaptureSessionInfo> captures;
    captures.reserve(merged.size());

    for (const auto& [capture_id, capture] : merged) {
        (void)capture_id;
        captures.push_back(capture);
    }

    std::sort(
        captures.begin(),
        captures.end(),
        [](const RemoteCaptureSessionInfo& left, const RemoteCaptureSessionInfo& right) {
            return left.created_at > right.created_at;
        }
    );

    return captures;
}

bool remove_path_if_exists(
    const std::filesystem::path& path,
    std::string& error_message
) {
    std::error_code filesystem_error;

    if (!std::filesystem::exists(path, filesystem_error)) {
        return true;
    }

    std::filesystem::remove_all(path, filesystem_error);

    if (filesystem_error) {
        error_message =
            "Failed to remove path from controller storage: "
            + path.string()
            + " ("
            + filesystem_error.message()
            + ")";
        return false;
    }

    return true;
}

}  // namespace

ControllerService::ControllerService(std::shared_ptr<AgentRegistry> agent_registry)
    : agent_registry_(std::move(agent_registry)) {}

void ControllerService::set_capture_storage_root(std::string capture_storage_root) {
    capture_storage_root_ = std::move(capture_storage_root);

    if (capture_storage_root_.empty()) {
        capture_storage_root_ = "data/captures";
    }
}

bool ControllerService::add_agent(
    const AddAgentRequest& request,
    KnownAgent& added_agent,
    std::string& error_message
) {
    if (!agent_registry_) {
        error_message = "Agent registry is not initialized";
        return false;
    }

    return agent_registry_->add_agent(request, added_agent, error_message);
}

bool ControllerService::remove_agent(
    const std::string& agent_id,
    KnownAgent& removed_agent,
    std::string& error_message
) {
    if (!agent_registry_) {
        error_message = "Agent registry is not initialized";
        return false;
    }

    if (!agent_registry_->remove_agent(agent_id, removed_agent, error_message)) {
        return false;
    }

    const auto agent_capture_directory =
        make_agent_storage_directory(capture_storage_root_, agent_id);

    if (!remove_path_if_exists(agent_capture_directory, error_message)) {
        return false;
    }

    return true;
}

bool ControllerService::clear_agents(
    std::size_t& cleared_count,
    std::string& error_message
) {
    if (!agent_registry_) {
        error_message = "Agent registry is not initialized";
        return false;
    }

    if (!agent_registry_->clear_agents(cleared_count, error_message)) {
        return false;
    }

    const std::filesystem::path capture_root(capture_storage_root_);
    if (!remove_path_if_exists(capture_root, error_message)) {
        return false;
    }

    return true;
}

std::vector<KnownAgent> ControllerService::list_agents() const {
    if (!agent_registry_) {
        return {};
    }

    return agent_registry_->list_agents();
}

std::optional<KnownAgent> ControllerService::get_agent(
    const std::string& agent_id
) const {
    if (!agent_registry_) {
        return std::nullopt;
    }

    return agent_registry_->get_agent(agent_id);
}

bool ControllerService::get_agent_health(
    const std::string& agent_id,
    KnownAgentWithHealth& result,
    std::string& error_message,
    int& response_status
) const {
    const auto agent = get_agent(agent_id);
    if (!agent.has_value()) {
        response_status = 404;
        error_message = "Agent not found";
        return false;
    }

    HttpAgentClient agent_client(endpoint_from_agent(*agent));
    RemoteHealthInfo health;
    if (!agent_client.get_health(health, error_message, response_status)) {
        return false;
    }

    result.agent = *agent;
    result.health = std::move(health);
    return true;
}

bool ControllerService::get_agent_interfaces(
    const std::string& agent_id,
    KnownAgent& agent,
    std::vector<RemoteInterfaceInfo>& interfaces,
    std::string& error_message,
    int& response_status
) const {
    const auto known_agent = get_agent(agent_id);
    if (!known_agent.has_value()) {
        response_status = 404;
        error_message = "Agent not found";
        return false;
    }

    HttpAgentClient agent_client(endpoint_from_agent(*known_agent));
    if (!agent_client.list_interfaces(interfaces, error_message, response_status)) {
        return false;
    }

    agent = *known_agent;
    return true;
}

bool ControllerService::start_agent_capture(
    const std::string& agent_id,
    const RemoteCaptureRequest& request,
    KnownAgent& agent,
    RemoteCaptureSessionInfo& session,
    std::string& error_message,
    int& response_status
) const {
    const auto known_agent = get_agent(agent_id);
    if (!known_agent.has_value()) {
        response_status = 404;
        error_message = "Agent not found";
        return false;
    }

    HttpAgentClient agent_client(endpoint_from_agent(*known_agent));
    if (!agent_client.start_capture(request, session, error_message, response_status)) {
        return false;
    }

    agent = *known_agent;
    return true;
}

bool ControllerService::list_agent_captures(
    const std::string& agent_id,
    KnownAgent& agent,
    std::vector<RemoteCaptureSessionInfo>& captures,
    std::string& error_message,
    int& response_status
) const {
    const auto known_agent = get_agent(agent_id);
    if (!known_agent.has_value()) {
        response_status = 404;
        error_message = "Agent not found";
        return false;
    }

    std::vector<RemoteCaptureSessionInfo> live_captures;
    const auto stored_captures =
        list_stored_capture_sessions_for_agent(capture_storage_root_, agent_id);

    HttpAgentClient agent_client(endpoint_from_agent(*known_agent));

    std::string live_error_message;
    int live_response_status = 0;
    const bool live_ok =
        agent_client.list_captures(live_captures, live_error_message, live_response_status);

    if (!live_ok && stored_captures.empty()) {
        error_message = live_error_message;
        response_status = live_response_status;
        return false;
    }

    captures = merge_live_and_stored_captures(live_captures, stored_captures);
    agent = *known_agent;
    response_status = 200;
    return true;
}

bool ControllerService::get_agent_capture(
    const std::string& agent_id,
    const std::string& capture_id,
    KnownAgent& agent,
    RemoteCaptureSessionInfo& session,
    std::string& error_message,
    int& response_status
) const {
    const auto known_agent = get_agent(agent_id);
    if (!known_agent.has_value()) {
        response_status = 404;
        error_message = "Agent not found";
        return false;
    }

    HttpAgentClient agent_client(endpoint_from_agent(*known_agent));

    std::string live_error_message;
    int live_response_status = 0;

    if (agent_client.get_capture(capture_id, session, live_error_message, live_response_status)) {
        agent = *known_agent;
        response_status = 200;
        return true;
    }

    if (load_stored_capture_session(capture_storage_root_, agent_id, capture_id, session)) {
        agent = *known_agent;
        response_status = 200;
        return true;
    }

    error_message = live_error_message.empty()
        ? "Capture not found on agent or controller storage"
        : live_error_message;

    response_status = live_response_status == 0 ? 404 : live_response_status;
    return false;
}

bool ControllerService::stop_agent_capture(
    const std::string& agent_id,
    const std::string& capture_id,
    KnownAgent& agent,
    RemoteCaptureSessionInfo& session,
    std::string& error_message,
    int& response_status
) const {
    const auto known_agent = get_agent(agent_id);
    if (!known_agent.has_value()) {
        response_status = 404;
        error_message = "Agent not found";
        return false;
    }

    HttpAgentClient agent_client(endpoint_from_agent(*known_agent));
    if (!agent_client.stop_capture(capture_id, session, error_message, response_status)) {
        return false;
    }

    agent = *known_agent;
    return true;
}

bool ControllerService::download_agent_capture(
    const std::string& agent_id,
    const std::string& capture_id,
    KnownAgent& agent,
    std::string& content,
    std::string& content_type,
    std::string& error_message,
    int& response_status
) const {
    const auto known_agent = get_agent(agent_id);
    if (!known_agent.has_value()) {
        response_status = 404;
        error_message = "Agent not found";
        return false;
    }

    HttpAgentClient agent_client(endpoint_from_agent(*known_agent));
    if (!agent_client.download_capture(
            capture_id,
            content,
            content_type,
            error_message,
            response_status
        )) {
        return false;
    }

    agent = *known_agent;
    return true;
}

bool ControllerService::fetch_agent_capture_to_controller(
    const std::string& agent_id,
    const std::string& capture_id,
    ControllerStoredCaptureInfo& stored_capture,
    std::string& error_message,
    int& response_status
) const {
    KnownAgent agent;
    std::string content;
    std::string content_type;

    if (!download_agent_capture(
            agent_id,
            capture_id,
            agent,
            content,
            content_type,
            error_message,
            response_status
        )) {
        return false;
    }

    const auto local_path =
        make_capture_storage_path(capture_storage_root_, agent_id, capture_id);

    std::error_code filesystem_error;
    std::filesystem::create_directories(local_path.parent_path(), filesystem_error);

    if (filesystem_error) {
        response_status = 500;
        error_message =
            "Failed to create controller capture storage directory: "
            + filesystem_error.message();
        return false;
    }

    std::ofstream output(local_path, std::ios::binary | std::ios::trunc);
    if (!output) {
        response_status = 500;
        error_message = "Failed to open controller capture file for writing";
        return false;
    }

    output.write(content.data(), static_cast<std::streamsize>(content.size()));

    if (!output) {
        response_status = 500;
        error_message = "Failed to write capture content to controller storage";
        return false;
    }

    output.close();

    stored_capture = make_stored_capture_info(agent_id, capture_id, local_path);

    RemoteCaptureSessionInfo capture_metadata;
    std::string metadata_error_message;
    int metadata_response_status = 0;

    HttpAgentClient agent_client(endpoint_from_agent(agent));
    if (!agent_client.get_capture(
            capture_id,
            capture_metadata,
            metadata_error_message,
            metadata_response_status
        )) {
        capture_metadata = make_synthetic_capture_from_stored_file(stored_capture);
    }

    const auto metadata_path =
        make_capture_metadata_path(capture_storage_root_, agent_id, capture_id);

    if (!save_capture_metadata(metadata_path, agent_id, capture_metadata, error_message)) {
        response_status = 500;
        return false;
    }

    response_status = 201;
    return true;
}

std::vector<ControllerStoredCaptureInfo> ControllerService::list_controller_captures() const {
    std::vector<ControllerStoredCaptureInfo> captures;

    const std::filesystem::path root(capture_storage_root_);
    std::error_code error;

    if (!std::filesystem::exists(root, error) || !std::filesystem::is_directory(root, error)) {
        return captures;
    }

    for (const auto& agent_entry : std::filesystem::directory_iterator(root, error)) {
        if (error) {
            break;
        }

        if (!agent_entry.is_directory()) {
            continue;
        }

        const auto agent_id = agent_entry.path().filename().string();

        for (const auto& capture_entry : std::filesystem::directory_iterator(agent_entry.path(), error)) {
            if (error) {
                break;
            }

            if (!capture_entry.is_regular_file() || capture_entry.path().extension() != ".pcap") {
                continue;
            }

            const auto capture_id = capture_entry.path().stem().string();
            captures.push_back(
                make_stored_capture_info(agent_id, capture_id, capture_entry.path())
            );
        }
    }

    std::sort(
        captures.begin(),
        captures.end(),
        [](const ControllerStoredCaptureInfo& left, const ControllerStoredCaptureInfo& right) {
            return left.fetched_at > right.fetched_at;
        }
    );

    return captures;
}

bool ControllerService::get_controller_capture(
    const std::string& agent_id,
    const std::string& capture_id,
    ControllerStoredCaptureInfo& stored_capture,
    std::string& error_message
) const {
    const auto local_path =
        make_capture_storage_path(capture_storage_root_, agent_id, capture_id);

    stored_capture = make_stored_capture_info(agent_id, capture_id, local_path);

    if (!stored_capture.exists) {
        error_message = "Capture is not stored on controller";
        return false;
    }

    return true;
}

bool ControllerService::read_controller_capture_content(
    const std::string& agent_id,
    const std::string& capture_id,
    std::string& content,
    ControllerStoredCaptureInfo& stored_capture,
    std::string& error_message
) const {
    if (!get_controller_capture(agent_id, capture_id, stored_capture, error_message)) {
        return false;
    }

    std::ifstream input(stored_capture.local_path, std::ios::binary);
    if (!input) {
        error_message = "Failed to open stored capture on controller";
        return false;
    }

    content.assign(
        std::istreambuf_iterator<char>(input),
        std::istreambuf_iterator<char>()
    );

    return true;
}

bool ControllerService::analyze_controller_capture(
    const std::string& agent_id,
    const std::string& capture_id,
    PcapAnalysisResult& analysis,
    std::string& error_message
) const {
    ControllerStoredCaptureInfo stored_capture;

    if (!get_controller_capture(agent_id, capture_id, stored_capture, error_message)) {
        return false;
    }

    return PcapAnalyzer::analyze(stored_capture, analysis, error_message);
}

AgentEndpoint ControllerService::endpoint_from_agent(const KnownAgent& agent) const {
    AgentEndpoint endpoint;
    endpoint.host = agent.host;
    endpoint.port = agent.port;
    return endpoint;
}

}  // namespace controller
