#include "controller/ControllerHttpServer.hpp"

#include "controller/ControllerJsonMapper.hpp"

#include <iostream>
#include <utility>

namespace controller {

namespace {

int map_info_proxy_status(int response_status) {
    if (response_status == 404) {
        return 404;
    }
    return 502;
}

int map_capture_proxy_status(int response_status) {
    switch (response_status) {
        case 400:
        case 404:
        case 409:
            return response_status;
        default:
            return 502;
    }
}

}  // namespace

ControllerHttpServer::ControllerHttpServer(
    std::shared_ptr<ControllerService> controller_service,
    ControllerHttpServerConfig config
)
    : controller_service_(std::move(controller_service)),
      config_(std::move(config)) {}

bool ControllerHttpServer::start(std::string& error_message) {
    if (!controller_service_) {
        error_message = "Controller service is not initialized.";
        return false;
    }

    server_.Get("/api/agents", [this](const httplib::Request&, httplib::Response& res) {
        const auto agents = controller_service_->list_agents();
        res.set_content(to_json(agents), "application/json");
        res.status = 200;
    });

    server_.Post("/api/agents", [this](const httplib::Request& req, httplib::Response& res) {
        if (req.body.empty()) {
            res.set_content(make_error_json("Request body is empty"), "application/json");
            res.status = 400;
            return;
        }

        AddAgentRequest request;
        std::string parse_error;
        if (!parse_add_agent_request_json(req.body, request, parse_error)) {
            res.set_content(make_error_json(parse_error), "application/json");
            res.status = 400;
            return;
        }

        KnownAgent added_agent;
        std::string add_error;
        if (!controller_service_->add_agent(request, added_agent, add_error)) {
            res.set_content(make_error_json(add_error), "application/json");
            res.status = 400;
            return;
        }

        res.set_content(to_json(added_agent), "application/json");
        res.status = 201;
    });

    server_.Delete(R"(/api/agents/([A-Za-z0-9_-]+))", [this](const httplib::Request& req, httplib::Response& res) {
        const auto agent_id = req.matches[1].str();

        KnownAgent removed_agent;
        std::string remove_error;
        if (!controller_service_->remove_agent(agent_id, removed_agent, remove_error)) {
            res.set_content(make_error_json(remove_error), "application/json");
            res.status = 404;
            return;
        }

        res.set_content(to_json(removed_agent), "application/json");
        res.status = 200;
    });

    server_.Get(R"(/api/agents/([A-Za-z0-9_-]+)/health)", [this](const httplib::Request& req, httplib::Response& res) {
        const auto agent_id = req.matches[1].str();

        KnownAgentWithHealth result;
        std::string route_error_message;
        int response_status = 0;
        if (!controller_service_->get_agent_health(agent_id, result, route_error_message, response_status)) {
            res.set_content(make_error_json(route_error_message), "application/json");
            res.status = map_info_proxy_status(response_status);
            return;
        }

        res.set_content(to_json(result), "application/json");
        res.status = 200;
    });

    server_.Get(R"(/api/agents/([A-Za-z0-9_-]+)/interfaces)", [this](const httplib::Request& req, httplib::Response& res) {
        const auto agent_id = req.matches[1].str();

        KnownAgent agent;
        std::vector<RemoteInterfaceInfo> interfaces;
        std::string route_error_message;
        int response_status = 0;
        if (!controller_service_->get_agent_interfaces(
                agent_id,
                agent,
                interfaces,
                route_error_message,
                response_status
            )) {
            res.set_content(make_error_json(route_error_message), "application/json");
            res.status = map_info_proxy_status(response_status);
            return;
        }

        res.set_content(to_json(agent, interfaces), "application/json");
        res.status = 200;
    });

    server_.Post(R"(/api/agents/([A-Za-z0-9_-]+)/captures)", [this](const httplib::Request& req, httplib::Response& res) {
        if (req.body.empty()) {
            res.set_content(make_error_json("Request body is empty"), "application/json");
            res.status = 400;
            return;
        }

        RemoteCaptureRequest request;
        std::string parse_error;
        if (!parse_remote_capture_request_json(req.body, request, parse_error)) {
            res.set_content(make_error_json(parse_error), "application/json");
            res.status = 400;
            return;
        }

        const auto agent_id = req.matches[1].str();
        KnownAgent agent;
        RemoteCaptureSessionInfo capture;
        std::string route_error_message;
        int response_status = 0;
        if (!controller_service_->start_agent_capture(
                agent_id,
                request,
                agent,
                capture,
                route_error_message,
                response_status
            )) {
            res.set_content(make_error_json(route_error_message), "application/json");
            res.status = map_capture_proxy_status(response_status);
            return;
        }

        res.set_content(to_json(agent, capture), "application/json");
        res.status = 202;
    });

    server_.Get(R"(/api/agents/([A-Za-z0-9_-]+)/captures)", [this](const httplib::Request& req, httplib::Response& res) {
        const auto agent_id = req.matches[1].str();

        KnownAgent agent;
        std::vector<RemoteCaptureSessionInfo> captures;
        std::string route_error_message;
        int response_status = 0;
        if (!controller_service_->list_agent_captures(
                agent_id,
                agent,
                captures,
                route_error_message,
                response_status
            )) {
            res.set_content(make_error_json(route_error_message), "application/json");
            res.status = map_info_proxy_status(response_status);
            return;
        }

        res.set_content(to_json(agent, captures), "application/json");
        res.status = 200;
    });

    server_.Post(R"(/api/agents/([A-Za-z0-9_-]+)/captures/([A-Za-z0-9\-_]+)/stop)", [this](const httplib::Request& req, httplib::Response& res) {
        const auto agent_id = req.matches[1].str();
        const auto capture_id = req.matches[2].str();

        KnownAgent agent;
        RemoteCaptureSessionInfo capture;
        std::string route_error_message;
        int response_status = 0;
        if (!controller_service_->stop_agent_capture(
                agent_id,
                capture_id,
                agent,
                capture,
                route_error_message,
                response_status
            )) {
            res.set_content(make_error_json(route_error_message), "application/json");
            res.status = map_capture_proxy_status(response_status);
            return;
        }

        res.set_content(to_json(agent, capture), "application/json");
        res.status = 202;
    });

    server_.Get(R"(/api/agents/([A-Za-z0-9_-]+)/captures/([A-Za-z0-9\-_]+))", [this](const httplib::Request& req, httplib::Response& res) {
        const auto agent_id = req.matches[1].str();
        const auto capture_id = req.matches[2].str();

        KnownAgent agent;
        RemoteCaptureSessionInfo capture;
        std::string route_error_message;
        int response_status = 0;
        if (!controller_service_->get_agent_capture(
                agent_id,
                capture_id,
                agent,
                capture,
                route_error_message,
                response_status
            )) {
            res.set_content(make_error_json(route_error_message), "application/json");
            res.status = map_capture_proxy_status(response_status);
            return;
        }

        res.set_content(to_json(agent, capture), "application/json");
        res.status = 200;
    });

    server_.Get(R"(/api/agents/([A-Za-z0-9_-]+))", [this](const httplib::Request& req, httplib::Response& res) {
        const auto agent_id = req.matches[1].str();
        const auto agent = controller_service_->get_agent(agent_id);

        if (!agent.has_value()) {
            res.set_content(make_error_json("Agent not found"), "application/json");
            res.status = 404;
            return;
        }

        res.set_content(to_json(*agent), "application/json");
        res.status = 200;
    });

    server_.set_error_handler([](const httplib::Request&, httplib::Response& res) {
        res.set_content(make_error_json("Resource not found"), "application/json");
        res.status = 404;
    });

    std::cout << "Controller HTTP server listening on "
              << config_.bind_address << ":" << config_.port << '\n';

    const bool listen_ok = server_.listen(config_.bind_address.c_str(), config_.port);
    if (!listen_ok) {
        error_message = "Failed to bind HTTP server to " +
                        config_.bind_address + ":" + std::to_string(config_.port);
        return false;
    }

    return true;
}

void ControllerHttpServer::stop() {
    server_.stop();
}

}  // namespace controller
