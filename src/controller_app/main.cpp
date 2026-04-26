#include "controller/AgentRegistry.hpp"
#include "controller/ControllerHttpServer.hpp"
#include "controller/ControllerService.hpp"

#include <iostream>
#include <memory>
#include <string>

int main() {
    auto agent_registry = std::make_shared<controller::AgentRegistry>();
    auto controller_service = std::make_shared<controller::ControllerService>(agent_registry);

    controller::ControllerHttpServerConfig http_config;
    http_config.bind_address = "0.0.0.0";
    http_config.port = 8090;

    controller::ControllerHttpServer http_server(controller_service, http_config);

    std::string error_message;
    if (!http_server.start(error_message)) {
        std::cerr << "Failed to start controller server: " << error_message << '\n';
        return 1;
    }

    return 0;
}
