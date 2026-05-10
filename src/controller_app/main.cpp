#include "controller/AgentRegistry.hpp"
#include "controller/ControllerHttpServer.hpp"
#include "controller/ControllerService.hpp"

#include <iostream>
#include <memory>
#include <string>

namespace {
constexpr const char* kKnownAgentsStoragePath = "data/known_agents.json";
}

int main() {
    auto agent_registry = std::make_shared<controller::AgentRegistry>();
    auto controller_service = std::make_shared<controller::ControllerService>(agent_registry);

    std::string error_message;
    if (!agent_registry->set_storage_path(kKnownAgentsStoragePath, error_message)) {
        std::cerr << "Failed to configure controller storage: " << error_message << '\n';
        return 1;
    }

    if (!agent_registry->load_from_storage(error_message)) {
        std::cerr << "Failed to load controller storage: " << error_message << '\n';
        return 1;
    }

    controller::ControllerHttpServerConfig http_config;
    http_config.bind_address = "0.0.0.0";
    http_config.port = 8090;

    controller::ControllerHttpServer http_server(controller_service, http_config);

    if (!http_server.start(error_message)) {
        std::cerr << "Failed to start controller server: " << error_message << '\n';
        return 1;
    }

    return 0;
}
