#include "controller/ControllerJsonMapper.hpp"

#include <nlohmann/json.hpp>

#include <exception>

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

json health_to_json_value(const RemoteHealthInfo& health) {
    json j;
    j["status"] = health.status;
    j["agent_name"] = health.agent_name;
    j["version"] = health.version;
    j["hostname"] = health.hostname;
    return j;
}

json interfaces_to_json_value(const std::vector<RemoteInterfaceInfo>& interfaces) {
    json interfaces_json = json::array();

    for (const auto& iface : interfaces) {
        interfaces_json.push_back({
            {"name", iface.name},
            {"description", iface.description}
        });
    }

    return interfaces_json;
}
}  // namespace

std::string make_error_json(const std::string& message) {
    json j;
    j["status"] = "error";
    j["message"] = message;
    return j.dump(4);
}

std::string to_json(const KnownAgent& agent) {
    return agent_to_json_value(agent).dump(4);
}

std::string to_json(const std::vector<KnownAgent>& agents) {
    json j;
    j["agents"] = json::array();

    for (const auto& agent : agents) {
        j["agents"].push_back(agent_to_json_value(agent));
    }

    return j.dump(4);
}

std::string to_json(const KnownAgentWithHealth& agent_with_health) {
    json j;
    j["agent"] = agent_to_json_value(agent_with_health.agent);
    j["health"] = health_to_json_value(agent_with_health.health);
    return j.dump(4);
}

std::string to_json(const KnownAgent& agent, const std::vector<RemoteInterfaceInfo>& interfaces) {
    json j;
    j["agent"] = agent_to_json_value(agent);
    j["interfaces"] = interfaces_to_json_value(interfaces);
    return j.dump(4);
}

bool parse_add_agent_request_json(
    const std::string& request_body,
    AddAgentRequest& request,
    std::string& error_message
) {
    try {
        const json j = json::parse(request_body);

        if (!j.contains("host") || !j["host"].is_string()) {
            error_message = "Missing or invalid 'host'";
            return false;
        }

        request.host = j["host"].get<std::string>();

        if (j.contains("display_name")) {
            if (!j["display_name"].is_string()) {
                error_message = "Invalid 'display_name'";
                return false;
            }
            request.display_name = j["display_name"].get<std::string>();
        }

        if (j.contains("port")) {
            if (!j["port"].is_number_integer()) {
                error_message = "Invalid 'port'";
                return false;
            }
            request.port = j["port"].get<int>();
        }

        return true;
    } catch (const std::exception& ex) {
        error_message = std::string("Invalid JSON: ") + ex.what();
        return false;
    }
}

bool parse_remote_health_json(
    const std::string& response_body,
    RemoteHealthInfo& health,
    std::string& error_message
) {
    try {
        const json j = json::parse(response_body);

        if (!j.contains("status") || !j["status"].is_string()) {
            error_message = "Missing or invalid 'status' in health response";
            return false;
        }
        if (!j.contains("agent_name") || !j["agent_name"].is_string()) {
            error_message = "Missing or invalid 'agent_name' in health response";
            return false;
        }
        if (!j.contains("version") || !j["version"].is_string()) {
            error_message = "Missing or invalid 'version' in health response";
            return false;
        }
        if (!j.contains("hostname") || !j["hostname"].is_string()) {
            error_message = "Missing or invalid 'hostname' in health response";
            return false;
        }

        health.status = j["status"].get<std::string>();
        health.agent_name = j["agent_name"].get<std::string>();
        health.version = j["version"].get<std::string>();
        health.hostname = j["hostname"].get<std::string>();
        return true;
    } catch (const std::exception& ex) {
        error_message = std::string("Invalid health JSON: ") + ex.what();
        return false;
    }
}

bool parse_remote_interfaces_json(
    const std::string& response_body,
    std::vector<RemoteInterfaceInfo>& interfaces,
    std::string& error_message
) {
    try {
        const json j = json::parse(response_body);

        if (!j.contains("interfaces") || !j["interfaces"].is_array()) {
            error_message = "Missing or invalid 'interfaces' in response";
            return false;
        }

        interfaces.clear();
        for (const auto& item : j["interfaces"]) {
            if (!item.is_object()) {
                error_message = "Invalid interface entry in response";
                return false;
            }
            if (!item.contains("name") || !item["name"].is_string()) {
                error_message = "Missing or invalid 'name' in interface entry";
                return false;
            }

            RemoteInterfaceInfo iface;
            iface.name = item["name"].get<std::string>();

            if (item.contains("description") && item["description"].is_string()) {
                iface.description = item["description"].get<std::string>();
            }

            interfaces.push_back(iface);
        }

        return true;
    } catch (const std::exception& ex) {
        error_message = std::string("Invalid interfaces JSON: ") + ex.what();
        return false;
    }
}

bool parse_error_message_json(const std::string& response_body, std::string& message) {
    try {
        const json j = json::parse(response_body);
        if (j.contains("message") && j["message"].is_string()) {
            message = j["message"].get<std::string>();
            return true;
        }
    } catch (const std::exception&) {
    }

    return false;
}

}  // namespace controller
