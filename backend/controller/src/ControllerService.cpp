#include "controller/ControllerService.hpp"

#include "controller/HttpAgentClient.hpp"

#include "controller/PcapAnalyzer.hpp"

#include <chrono>
#include <cctype>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <utility>

namespace controller {

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

std::filesystem::path make_capture_storage_path(
    const std::string& capture_storage_root,
    const std::string& agent_id,
    const std::string& capture_id
) {
    return std::filesystem::path(capture_storage_root)
        / sanitize_path_component(agent_id)
        / (sanitize_path_component(capture_id) + ".pcap");
}

std::time_t file_time_to_time_t(const std::filesystem::file_time_type& file_time) {
    const auto system_time =
        std::chrono::time_point_cast<std::chrono::system_clock::duration>(
            file_time - std::filesystem::file_time_type::clock::now()
            + std::chrono::system_clock::now()
        );

    return std::chrono::system_clock::to_time_t(system_time);
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

    return agent_registry_->remove_agent(agent_id, removed_agent, error_message);
}

bool ControllerService::clear_agents(
    std::size_t& cleared_count,
    std::string& error_message
) {
    if (!agent_registry_) {
        error_message = "Agent registry is not initialized";
        return false;
    }

    return agent_registry_->clear_agents(cleared_count, error_message);
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

    HttpAgentClient agent_client(endpoint_from_agent(*known_agent));
    if (!agent_client.list_captures(captures, error_message, response_status)) {
        return false;
    }

    agent = *known_agent;
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
    if (!agent_client.get_capture(capture_id, session, error_message, response_status)) {
        return false;
    }

    agent = *known_agent;
    return true;
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

    for (const auto& agent_entry : std::filesystem::directory_iterator(root)) {
        if (!agent_entry.is_directory()) {
            continue;
        }

        const auto agent_id = agent_entry.path().filename().string();

        for (const auto& capture_entry : std::filesystem::directory_iterator(agent_entry.path())) {
            if (!capture_entry.is_regular_file() || capture_entry.path().extension() != ".pcap") {
                continue;
            }

            const auto capture_id = capture_entry.path().stem().string();
            captures.push_back(
                make_stored_capture_info(agent_id, capture_id, capture_entry.path())
            );
        }
    }

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