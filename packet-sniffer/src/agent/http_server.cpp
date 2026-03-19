#include "agent/http_server.hpp"

#include "agent/json_mapper.hpp"

#include <yhirose/httplib.h>
#include <nlohmann/json.hpp>

#include <memory>
#include <string>

namespace agent {

using json = nlohmann::json;

namespace {
httplib::Server* global_server_instance = nullptr;
}

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
            json error_json;
            error_json["status"] = "error";
            error_json["message"] = error_message;

            res.set_content(error_json.dump(4), "application/json");
            res.status = 500;
            return;
        }

        res.set_content(to_json(interfaces), "application/json");
        res.status = 200;
    });

    server.set_error_handler([](const httplib::Request&, httplib::Response& res) {
        json error_json;
        error_json["status"] = "error";
        error_json["message"] = "Resource not found";
        res.set_content(error_json.dump(4), "application/json");
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