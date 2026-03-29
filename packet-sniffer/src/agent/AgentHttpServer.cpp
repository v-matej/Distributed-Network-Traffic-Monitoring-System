#include "agent/AgentHttpServer.hpp"

#include "agent/AgentJsonMapper.hpp"

#include <yhirose/httplib.h>
#include <nlohmann/json.hpp>

#include <iostream>
#include <memory>
#include <string>

namespace agent {

using json = nlohmann::json;

namespace {
httplib::Server* global_server_instance = nullptr;

std::string make_error_json(const std::string& message) {
    json error_json;
    error_json["status"] = "error";
    error_json["message"] = message;
    return error_json.dump(4);
}
}  // namespace

AgentHttpServer::AgentHttpServer(
    std::shared_ptr<AgentService> agent_service,
    HttpServerConfig config
)
    : agent_service_(std::move(agent_service)),
      config_(std::move(config)) {}

bool AgentHttpServer::start(std::string& error_message) {
    if (!agent_service_) {
        error_message = "Agent service is not initialized.";
        return false;
    }

    httplib::Server server;
    global_server_instance = &server;

    server.Get("/health", [this](const httplib::Request&, httplib::Response& res) {
        const auto health = agent_service_->get_health();
        res.set_content(to_json(health), "application/json");
        res.status = 200;
    });

    server.Get("/interfaces", [this](const httplib::Request&, httplib::Response& res) {
        std::string error_message;
        const auto interfaces = agent_service_->get_interfaces(error_message);

        if (!error_message.empty()) {
            res.set_content(make_error_json(error_message), "application/json");
            res.status = 500;
            return;
        }

        res.set_content(to_json(interfaces), "application/json");
        res.status = 200;
    });

    server.Post("/captures", [this](const httplib::Request& req, httplib::Response& res) {
        if (req.body.empty()) {
            res.set_content(make_error_json("Request body is empty"), "application/json");
            res.status = 400;
            return;
        }

        sniffer::CaptureConfig config;
        std::string parse_error;

        if (!parse_capture_config_json(req.body, config, parse_error)) {
            res.set_content(make_error_json(parse_error), "application/json");
            res.status = 400;
            return;
        }

        const auto session = agent_service_->start_capture_session(config);
        res.set_content(to_json(session), "application/json");
        res.status = 202;
    });

    server.Get("/captures", [this](const httplib::Request&, httplib::Response& res) {
        const auto sessions = agent_service_->list_capture_sessions();
        res.set_content(to_json(sessions), "application/json");
        res.status = 200;
    });

    server.Get(R"(/captures/([A-Za-z0-9\-_]+))", [this](const httplib::Request& req, httplib::Response& res) {
        const auto capture_id = req.matches[1].str();
        const auto session = agent_service_->get_capture_session(capture_id);

        if (!session.has_value()) {
            res.set_content(make_error_json("Capture session not found"), "application/json");
            res.status = 404;
            return;
        }

        res.set_content(to_json(*session), "application/json");
        res.status = 200;
    });

    server.Post(R"(/captures/([A-Za-z0-9\-_]+)/stop)", [this](const httplib::Request& req, httplib::Response& res) {
        const auto capture_id = req.matches[1].str();
        std::string error_message;

        if (!agent_service_->stop_capture_session(capture_id, error_message)) {
            const auto session = agent_service_->get_capture_session(capture_id);
            if (!session.has_value()) {
                res.set_content(make_error_json("Capture session not found"), "application/json");
                res.status = 404;
            } else {
                res.set_content(make_error_json(error_message), "application/json");
                res.status = 409;
            }
            return;
        }

        const auto session = agent_service_->get_capture_session(capture_id);
        res.set_content(to_json(*session), "application/json");
        res.status = 202;
    });

    server.set_error_handler([](const httplib::Request&, httplib::Response& res) {
        res.set_content(make_error_json("Resource not found"), "application/json");
        res.status = 404;
    });

    std::cout << "Agent HTTP server listening on "
              << config_.bind_address << ":" << config_.port << '\n';

    const bool listen_ok = server.listen(config_.bind_address.c_str(), config_.port);
    global_server_instance = nullptr;

    if (!listen_ok) {
        error_message = "Failed to bind HTTP server to " +
                        config_.bind_address + ":" + std::to_string(config_.port);
        return false;
    }

    return true;
}

void AgentHttpServer::stop() {
    if (global_server_instance != nullptr) {
        global_server_instance->stop();
    }
}

}  // namespace agent
