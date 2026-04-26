#pragma once

#include "controller/ControllerTypes.hpp"

#include <string>
#include <vector>

namespace controller {

class IAgentClient {
public:
    virtual ~IAgentClient() = default;

    virtual bool get_health(
        RemoteHealthInfo& health,
        std::string& error_message,
        int& response_status
    ) = 0;

    virtual bool list_interfaces(
        std::vector<RemoteInterfaceInfo>& interfaces,
        std::string& error_message,
        int& response_status
    ) = 0;

    virtual bool start_capture(
        const RemoteCaptureRequest& request,
        RemoteCaptureSessionInfo& session,
        std::string& error_message,
        int& response_status
    ) = 0;

    virtual bool list_captures(
        std::vector<RemoteCaptureSessionInfo>& captures,
        std::string& error_message,
        int& response_status
    ) = 0;

    virtual bool get_capture(
        const std::string& capture_id,
        RemoteCaptureSessionInfo& session,
        std::string& error_message,
        int& response_status
    ) = 0;

    virtual bool stop_capture(
        const std::string& capture_id,
        RemoteCaptureSessionInfo& session,
        std::string& error_message,
        int& response_status
    ) = 0;
};

}  // namespace controller
