#pragma once

#include "controller/ControllerTypes.hpp"

#include <string>
#include <vector>

namespace controller {

class IAgentClient {
public:
    virtual ~IAgentClient() = default;

    virtual bool get_health(RemoteHealthInfo& health, std::string& error_message) = 0;
    virtual bool list_interfaces(std::vector<RemoteInterfaceInfo>& interfaces, std::string& error_message) = 0;
};

}  // namespace controller
