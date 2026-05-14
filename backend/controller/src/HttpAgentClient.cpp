#include "controller/HttpAgentClient.hpp"

#include "controller/ControllerJsonMapper.hpp"

#include <yhirose/httplib.h>

#include <utility>

namespace controller {

namespace {

void configure_client(httplib::Client& client) {
    client.set_connection_timeout(2, 0);
    client.set_read_timeout(5, 0);
    client.set_write_timeout(5, 0);
}

bool set_error_from_response(
    const httplib::Result& response,
    const std::string& path,
    const std::string& base_url,
    std::string& error_message,
    int& response_status
) {
    if (!response) {
        response_status = 502;
        error_message = "Failed to connect to agent at " + base_url + path;
        return false;
    }

    response_status = response->status;
    if (!parse_error_message_json(response->body, error_message)) {
        error_message = "Agent returned HTTP " + std::to_string(response->status) + " for " + path;
    }
    return false;
}

}  // namespace

HttpAgentClient::HttpAgentClient(AgentEndpoint endpoint)
    : endpoint_(std::move(endpoint)) {}

bool HttpAgentClient::get_health(
    RemoteHealthInfo& health,
    std::string& error_message,
    int& response_status
) {
    httplib::Client client(endpoint_.host, endpoint_.port);
    configure_client(client);

    const auto response = client.Get("/health");
    if (!response || response->status != 200) {
        return set_error_from_response(response, "/health", base_url(), error_message, response_status);
    }

    if (!parse_remote_health_json(response->body, health, error_message)) {
        response_status = 502;
        return false;
    }

    response_status = 200;
    return true;
}

bool HttpAgentClient::list_interfaces(
    std::vector<RemoteInterfaceInfo>& interfaces,
    std::string& error_message,
    int& response_status
) {
    httplib::Client client(endpoint_.host, endpoint_.port);
    configure_client(client);

    const auto response = client.Get("/interfaces");
    if (!response || response->status != 200) {
        return set_error_from_response(response, "/interfaces", base_url(), error_message, response_status);
    }

    if (!parse_remote_interfaces_json(response->body, interfaces, error_message)) {
        response_status = 502;
        return false;
    }

    response_status = 200;
    return true;
}

bool HttpAgentClient::start_capture(
    const RemoteCaptureRequest& request,
    RemoteCaptureSessionInfo& session,
    std::string& error_message,
    int& response_status
) {
    httplib::Client client(endpoint_.host, endpoint_.port);
    configure_client(client);

    const auto request_body = to_agent_capture_request_json(request);
    const auto response = client.Post("/captures", request_body, "application/json");
    if (!response || response->status != 202) {
        return set_error_from_response(response, "/captures", base_url(), error_message, response_status);
    }

    if (!parse_remote_capture_json(response->body, session, error_message)) {
        response_status = 502;
        return false;
    }

    response_status = 202;
    return true;
}

bool HttpAgentClient::list_captures(
    std::vector<RemoteCaptureSessionInfo>& captures,
    std::string& error_message,
    int& response_status
) {
    httplib::Client client(endpoint_.host, endpoint_.port);
    configure_client(client);

    const auto response = client.Get("/captures");
    if (!response || response->status != 200) {
        return set_error_from_response(response, "/captures", base_url(), error_message, response_status);
    }

    if (!parse_remote_captures_json(response->body, captures, error_message)) {
        response_status = 502;
        return false;
    }

    response_status = 200;
    return true;
}

bool HttpAgentClient::get_capture(
    const std::string& capture_id,
    RemoteCaptureSessionInfo& session,
    std::string& error_message,
    int& response_status
) {
    httplib::Client client(endpoint_.host, endpoint_.port);
    configure_client(client);

    const auto path = "/captures/" + capture_id;
    const auto response = client.Get(path);
    if (!response || response->status != 200) {
        return set_error_from_response(response, path, base_url(), error_message, response_status);
    }

    if (!parse_remote_capture_json(response->body, session, error_message)) {
        response_status = 502;
        return false;
    }

    response_status = 200;
    return true;
}

bool HttpAgentClient::stop_capture(
    const std::string& capture_id,
    RemoteCaptureSessionInfo& session,
    std::string& error_message,
    int& response_status
) {
    httplib::Client client(endpoint_.host, endpoint_.port);
    configure_client(client);

    const auto path = "/captures/" + capture_id + "/stop";
    const auto response = client.Post(path, "", "application/json");
    if (!response || response->status != 202) {
        return set_error_from_response(response, path, base_url(), error_message, response_status);
    }

    if (!parse_remote_capture_json(response->body, session, error_message)) {
        response_status = 502;
        return false;
    }

    response_status = 202;
    return true;
}

bool HttpAgentClient::download_capture(
    const std::string& capture_id,
    std::string& content,
    std::string& content_type,
    std::string& error_message,
    int& response_status
) {
    httplib::Client client(endpoint_.host, endpoint_.port);
    configure_client(client);
    client.set_read_timeout(60, 0);

    const auto path = "/captures/" + capture_id + "/download";
    const auto response = client.Get(path);

    if (!response || response->status != 200) {
        return set_error_from_response(response, path, base_url(), error_message, response_status);
    }

    response_status = 200;
    content = response->body;
    content_type = response->get_header_value("Content-Type");

    if (content_type.empty()) {
        content_type = "application/vnd.tcpdump.pcap";
    }

    return true;
}

std::string HttpAgentClient::base_url() const {
    return "http://" + endpoint_.host + ":" + std::to_string(endpoint_.port);
}

}  // namespace controller
