#pragma once

#include "controller/ControllerTypes.hpp"
#include "controller/IAgentClient.hpp"

#include <string>
#include <vector>

namespace controller {

class HttpAgentClient : public IAgentClient {
public:
    explicit HttpAgentClient(AgentEndpoint endpoint);

    bool get_health(
        RemoteHealthInfo& health,
        std::string& error_message,
        int& response_status
    ) override;

    bool list_interfaces(
        std::vector<RemoteInterfaceInfo>& interfaces,
        std::string& error_message,
        int& response_status
    ) override;

    bool start_capture(
        const RemoteCaptureRequest& request,
        RemoteCaptureSessionInfo& session,
        std::string& error_message,
        int& response_status
    ) override;

    bool list_captures(
        std::vector<RemoteCaptureSessionInfo>& captures,
        std::string& error_message,
        int& response_status
    ) override;

    bool get_capture(
        const std::string& capture_id,
        RemoteCaptureSessionInfo& session,
        std::string& error_message,
        int& response_status
    ) override;

    bool stop_capture(
        const std::string& capture_id,
        RemoteCaptureSessionInfo& session,
        std::string& error_message,
        int& response_status
    ) override;

private:
    AgentEndpoint endpoint_;

    std::string base_url() const;
};

}  // namespace controller
