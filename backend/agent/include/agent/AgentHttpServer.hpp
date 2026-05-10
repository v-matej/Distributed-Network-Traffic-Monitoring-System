#pragma once

#include "agent/AgentService.hpp"

#include <yhirose/httplib.h>

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

    ~AgentHttpServer();

    bool start(std::string& error_message);
    void stop();

private:
    std::shared_ptr<AgentService> agent_service_;
    HttpServerConfig config_;
    httplib::Server server_;
};

}  // namespace agent
