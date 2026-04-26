#pragma once

#include "controller/AgentRegistry.hpp"
#include "controller/ControllerTypes.hpp"

#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace controller {

class ControllerService {
public:
    explicit ControllerService(std::shared_ptr<AgentRegistry> agent_registry);

    bool add_agent(const AddAgentRequest& request, KnownAgent& added_agent, std::string& error_message);
    std::vector<KnownAgent> list_agents() const;
    std::optional<KnownAgent> get_agent(const std::string& agent_id) const;

    bool get_agent_health(
        const std::string& agent_id,
        KnownAgentWithHealth& result,
        std::string& error_message
    ) const;

    bool get_agent_interfaces(
        const std::string& agent_id,
        KnownAgent& agent,
        std::vector<RemoteInterfaceInfo>& interfaces,
        std::string& error_message
    ) const;

private:
    AgentEndpoint endpoint_from_agent(const KnownAgent& agent) const;

    std::shared_ptr<AgentRegistry> agent_registry_;
};

}  // namespace controller
