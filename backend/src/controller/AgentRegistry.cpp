#include "controller/AgentRegistry.hpp"

#include <algorithm>
#include <cctype>
#include <ctime>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <nlohmann/json.hpp>
#include <sstream>
#include <system_error>

namespace controller {

using json = nlohmann::json;

namespace {

json agent_to_json_value(const KnownAgent& agent) {
    json j;
    j["agent_id"] = agent.agent_id;
    j["display_name"] = agent.display_name;
    j["host"] = agent.host;
    j["port"] = agent.port;
    j["created_at"] = agent.created_at;
    return j;
}

bool parse_agent_json(const json& j, KnownAgent& agent, std::string& error_message) {
    if (!j.is_object()) {
        error_message = "Invalid agent entry in storage";
        return false;
    }
    if (!j.contains("agent_id") || !j["agent_id"].is_string()) {
        error_message = "Missing or invalid 'agent_id' in storage";
        return false;
    }
    if (!j.contains("display_name") || !j["display_name"].is_string()) {
        error_message = "Missing or invalid 'display_name' in storage";
        return false;
    }
    if (!j.contains("host") || !j["host"].is_string()) {
        error_message = "Missing or invalid 'host' in storage";
        return false;
    }
    if (!j.contains("port") || !j["port"].is_number_integer()) {
        error_message = "Missing or invalid 'port' in storage";
        return false;
    }
    if (!j.contains("created_at") || !j["created_at"].is_number_integer()) {
        error_message = "Missing or invalid 'created_at' in storage";
        return false;
    }

    agent.agent_id = j["agent_id"].get<std::string>();
    agent.display_name = j["display_name"].get<std::string>();
    agent.host = j["host"].get<std::string>();
    agent.port = j["port"].get<int>();
    agent.created_at = static_cast<std::time_t>(j["created_at"].get<long long>());
    return true;
}

}  // namespace

bool AgentRegistry::set_storage_path(const std::string& storage_path, std::string& error_message) {
    const auto trimmed_path = trim_copy(storage_path);
    if (trimmed_path.empty()) {
        error_message = "Storage path must not be empty";
        return false;
    }

    std::lock_guard<std::mutex> lock(mutex_);
    storage_path_ = trimmed_path;
    return true;
}

bool AgentRegistry::load_from_storage(std::string& error_message) {
    std::lock_guard<std::mutex> lock(mutex_);

    if (storage_path_.empty()) {
        error_message = "Storage path is not configured";
        return false;
    }

    const std::filesystem::path storage_file(storage_path_);
    if (!std::filesystem::exists(storage_file)) {
        agents_.clear();
        next_agent_id_ = 1;
        return true;
    }

    std::ifstream input(storage_file);
    if (!input) {
        error_message = "Failed to open storage file: " + storage_file.string();
        return false;
    }

    json document;
    try {
        input >> document;
    } catch (const std::exception& ex) {
        error_message = std::string("Invalid storage JSON: ") + ex.what();
        return false;
    }

    if (!document.is_object()) {
        error_message = "Invalid storage root object";
        return false;
    }
    if (!document.contains("next_agent_id") || !document["next_agent_id"].is_number_unsigned()) {
        error_message = "Missing or invalid 'next_agent_id' in storage";
        return false;
    }
    if (!document.contains("agents") || !document["agents"].is_array()) {
        error_message = "Missing or invalid 'agents' in storage";
        return false;
    }

    std::map<std::string, KnownAgent> loaded_agents;
    for (const auto& item : document["agents"]) {
        KnownAgent agent;
        if (!parse_agent_json(item, agent, error_message)) {
            return false;
        }

        if (loaded_agents.find(agent.agent_id) != loaded_agents.end()) {
            error_message = "Duplicate agent id in storage: " + agent.agent_id;
            return false;
        }

        loaded_agents[agent.agent_id] = agent;
    }

    agents_ = std::move(loaded_agents);
    next_agent_id_ = document["next_agent_id"].get<unsigned long long>();
    if (next_agent_id_ == 0) {
        next_agent_id_ = 1;
    }

    return true;
}

bool AgentRegistry::add_agent(const AddAgentRequest& request, KnownAgent& added_agent, std::string& error_message) {
    const auto host = trim_copy(request.host);
    const auto display_name = trim_copy(request.display_name);

    if (host.empty()) {
        error_message = "'host' must not be empty";
        return false;
    }

    if (request.port <= 0 || request.port > 65535) {
        error_message = "'port' must be between 1 and 65535";
        return false;
    }

    std::lock_guard<std::mutex> lock(mutex_);

    for (const auto& [existing_id, existing_agent] : agents_) {
        (void)existing_id;
        if (existing_agent.host == host && existing_agent.port == request.port) {
            error_message = "Agent with the same host and port is already registered";
            return false;
        }
    }

    const auto previous_agents = agents_;
    const auto previous_next_agent_id = next_agent_id_;

    KnownAgent agent;
    agent.agent_id = generate_agent_id();
    agent.display_name = display_name;
    agent.host = host;
    agent.port = request.port;
    agent.created_at = std::time(nullptr);

    agents_[agent.agent_id] = agent;

    if (!save_to_storage_locked(error_message)) {
        agents_ = previous_agents;
        next_agent_id_ = previous_next_agent_id;
        return false;
    }

    added_agent = agent;
    return true;
}

bool AgentRegistry::remove_agent(const std::string& agent_id, KnownAgent& removed_agent, std::string& error_message) {
    std::lock_guard<std::mutex> lock(mutex_);

    const auto it = agents_.find(agent_id);
    if (it == agents_.end()) {
        error_message = "Agent not found";
        return false;
    }

    const auto previous_agents = agents_;
    const auto previous_next_agent_id = next_agent_id_;

    removed_agent = it->second;
    agents_.erase(it);

    if (!save_to_storage_locked(error_message)) {
        agents_ = previous_agents;
        next_agent_id_ = previous_next_agent_id;
        return false;
    }

    return true;
}

bool AgentRegistry::clear_agents(std::size_t& cleared_count, std::string& error_message) {
    std::lock_guard<std::mutex> lock(mutex_);

    cleared_count = agents_.size();
    const auto previous_agents = agents_;
    const auto previous_next_agent_id = next_agent_id_;

    agents_.clear();
    next_agent_id_ = 1;

    if (storage_path_.empty()) {
        return true;
    }

    const std::filesystem::path storage_file(storage_path_);
    std::error_code ec;
    if (std::filesystem::exists(storage_file, ec)) {
        std::filesystem::remove(storage_file, ec);
        if (ec) {
            agents_ = previous_agents;
            next_agent_id_ = previous_next_agent_id;
            error_message = "Failed to clear storage file: " + storage_file.string();
            return false;
        }
    }

    return true;
}

std::optional<KnownAgent> AgentRegistry::get_agent(const std::string& agent_id) const {
    std::lock_guard<std::mutex> lock(mutex_);
    const auto it = agents_.find(agent_id);
    if (it == agents_.end()) {
        return std::nullopt;
    }

    return it->second;
}

std::vector<KnownAgent> AgentRegistry::list_agents() const {
    std::lock_guard<std::mutex> lock(mutex_);

    std::vector<KnownAgent> agents;
    agents.reserve(agents_.size());

    for (const auto& [agent_id, agent] : agents_) {
        (void)agent_id;
        agents.push_back(agent);
    }

    return agents;
}

bool AgentRegistry::save_to_storage_locked(std::string& error_message) const {
    if (storage_path_.empty()) {
        return true;
    }

    json document;
    document["next_agent_id"] = next_agent_id_;
    document["agents"] = json::array();

    for (const auto& [agent_id, agent] : agents_) {
        (void)agent_id;
        document["agents"].push_back(agent_to_json_value(agent));
    }

    const std::filesystem::path storage_file(storage_path_);
    const auto parent_directory = storage_file.parent_path();
    std::error_code ec;

    if (!parent_directory.empty()) {
        std::filesystem::create_directories(parent_directory, ec);
        if (ec) {
            error_message = "Failed to create storage directory: " + parent_directory.string();
            return false;
        }
    }

    const auto temp_file = storage_file.string() + ".tmp";
    std::ofstream output(temp_file, std::ios::trunc);
    if (!output) {
        error_message = "Failed to write temporary storage file: " + temp_file;
        return false;
    }

    output << document.dump(4);
    output.close();

    if (!output) {
        error_message = "Failed to flush temporary storage file: " + temp_file;
        return false;
    }

    std::filesystem::remove(storage_file, ec);
    ec.clear();
    std::filesystem::rename(temp_file, storage_file, ec);
    if (ec) {
        std::filesystem::remove(temp_file);
        error_message = "Failed to finalize storage file: " + storage_file.string();
        return false;
    }

    return true;
}

std::string AgentRegistry::generate_agent_id() {
    std::ostringstream id_builder;
    id_builder << "agent-" << std::setw(4) << std::setfill('0') << next_agent_id_++;
    return id_builder.str();
}

std::string AgentRegistry::trim_copy(const std::string& value) {
    auto begin = value.begin();
    auto end = value.end();

    while (begin != end && std::isspace(static_cast<unsigned char>(*begin)) != 0) {
        ++begin;
    }

    while (begin != end && std::isspace(static_cast<unsigned char>(*(end - 1))) != 0) {
        --end;
    }

    return std::string(begin, end);
}

}  // namespace controller
