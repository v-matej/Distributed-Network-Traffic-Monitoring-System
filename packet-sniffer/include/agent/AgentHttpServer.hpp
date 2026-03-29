#pragma once

#include "agent/AgentService.hpp"

#include <memory>
#include <string>

namespace agent {

struct HttpServerConfig {
    std::string bind_address = "0.0.0.0";
    int port = 8080;
};

class AgentHttpServer {
public:
    AgentHttpServer(
        std::shared_ptr<AgentService> agent_service,
        HttpServerConfig config
    );

    bool start(std::string& error_message);
    void stop();

private:
    std::shared_ptr<AgentService> agent_service_;
    HttpServerConfig config_;
};

}  // namespace agent
