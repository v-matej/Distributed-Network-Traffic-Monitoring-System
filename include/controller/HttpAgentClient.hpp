#pragma once

#include "controller/ControllerTypes.hpp"
#include "controller/IAgentClient.hpp"

#include <string>

namespace controller {

class HttpAgentClient : public IAgentClient {
public:
    explicit HttpAgentClient(AgentEndpoint endpoint);

    bool get_health(RemoteHealthInfo& health, std::string& error_message) override;
    bool list_interfaces(std::vector<RemoteInterfaceInfo>& interfaces, std::string& error_message) override;

private:
    AgentEndpoint endpoint_;

    std::string base_url() const;
};

}  // namespace controller
