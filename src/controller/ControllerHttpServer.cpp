#include "controller/ControllerHttpServer.hpp"

#include "controller/ControllerJsonMapper.hpp"

#include <iostream>
#include <utility>

namespace controller {

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

    server_.Get(R"(/api/agents/([A-Za-z0-9_-]+)/health)", [this](const httplib::Request& req, httplib::Response& res) {
        const auto agent_id = req.matches[1].str();

        KnownAgentWithHealth result;
        std::string error_message;
        if (!controller_service_->get_agent_health(agent_id, result, error_message)) {
            const auto agent = controller_service_->get_agent(agent_id);
            if (!agent.has_value()) {
                res.set_content(make_error_json("Agent not found"), "application/json");
                res.status = 404;
            } else {
                res.set_content(make_error_json(error_message), "application/json");
                res.status = 502;
            }
            return;
        }

        res.set_content(to_json(result), "application/json");
        res.status = 200;
    });

    server_.Get(R"(/api/agents/([A-Za-z0-9_-]+)/interfaces)", [this](const httplib::Request& req, httplib::Response& res) {
        const auto agent_id = req.matches[1].str();

        KnownAgent agent;
        std::vector<RemoteInterfaceInfo> interfaces;
        std::string error_message;
        if (!controller_service_->get_agent_interfaces(agent_id, agent, interfaces, error_message)) {
            const auto known_agent = controller_service_->get_agent(agent_id);
            if (!known_agent.has_value()) {
                res.set_content(make_error_json("Agent not found"), "application/json");
                res.status = 404;
            } else {
                res.set_content(make_error_json(error_message), "application/json");
                res.status = 502;
            }
            return;
        }

        res.set_content(to_json(agent, interfaces), "application/json");
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
