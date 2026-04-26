#include "controller/HttpAgentClient.hpp"

#include "controller/ControllerJsonMapper.hpp"

#include <yhirose/httplib.h>

#include <utility>

namespace controller {

HttpAgentClient::HttpAgentClient(AgentEndpoint endpoint)
    : endpoint_(std::move(endpoint)) {}

bool HttpAgentClient::get_health(RemoteHealthInfo& health, std::string& error_message) {
    httplib::Client client(endpoint_.host, endpoint_.port);
    client.set_connection_timeout(2, 0);
    client.set_read_timeout(5, 0);
    client.set_write_timeout(5, 0);

    const auto response = client.Get("/health");
    if (!response) {
        error_message = "Failed to connect to agent at " + base_url();
        return false;
    }

    if (response->status != 200) {
        if (!parse_error_message_json(response->body, error_message)) {
            error_message = "Agent returned HTTP " + std::to_string(response->status) + " for /health";
        }
        return false;
    }

    return parse_remote_health_json(response->body, health, error_message);
}

bool HttpAgentClient::list_interfaces(std::vector<RemoteInterfaceInfo>& interfaces, std::string& error_message) {
    httplib::Client client(endpoint_.host, endpoint_.port);
    client.set_connection_timeout(2, 0);
    client.set_read_timeout(5, 0);
    client.set_write_timeout(5, 0);

    const auto response = client.Get("/interfaces");
    if (!response) {
        error_message = "Failed to connect to agent at " + base_url();
        return false;
    }

    if (response->status != 200) {
        if (!parse_error_message_json(response->body, error_message)) {
            error_message = "Agent returned HTTP " + std::to_string(response->status) + " for /interfaces";
        }
        return false;
    }

    return parse_remote_interfaces_json(response->body, interfaces, error_message);
}

std::string HttpAgentClient::base_url() const {
    return "http://" + endpoint_.host + ":" + std::to_string(endpoint_.port);
}

}  // namespace controller
