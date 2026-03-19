#include "agent/http_server.hpp"
#include "agent/service.hpp"
#include "agent/types.hpp"
#include "sniffer/capture.hpp"

#include <iostream>
#include <memory>
#include <string>

int main() {
    try {
        agent::AgentConfig agent_config;
        agent_config.agent_name = "Client-1";
        agent_config.version = "0.1.0";

        auto capture_service = sniffer::create_pcap_capture_service();
        auto agent_service = std::make_shared<agent::AgentService>(agent_config, capture_service);

        agent::HttpServerConfig http_config;
        http_config.bind_address = "0.0.0.0";
        http_config.port = 8080;

        agent::AgentHttpServer server(agent_service, http_config);

        std::string error_message;
        if (!server.start(error_message)) {
            std::cerr << "Failed to start agent server: " << error_message << '\n';
            return 1;
        }

        return 0;
    } catch (const std::exception& ex) {
        std::cerr << "Fatal error: " << ex.what() << '\n';
        return 1;
    }
}