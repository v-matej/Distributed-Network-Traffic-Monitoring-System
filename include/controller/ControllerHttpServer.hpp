#pragma once

#include "controller/ControllerService.hpp"

#include <yhirose/httplib.h>

#include <memory>
#include <string>

namespace controller {

struct ControllerHttpServerConfig {
    std::string bind_address = "0.0.0.0";
    int port = 8090;
};

class ControllerHttpServer {
public:
    ControllerHttpServer(
        std::shared_ptr<ControllerService> controller_service,
        ControllerHttpServerConfig config
    );

    bool start(std::string& error_message);
    void stop();

private:
    std::shared_ptr<ControllerService> controller_service_;
    ControllerHttpServerConfig config_;
    httplib::Server server_;
};

}  // namespace controller
