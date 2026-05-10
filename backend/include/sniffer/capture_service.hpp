#pragma once

#include "sniffer/types.hpp"

#include <string>
#include <vector>

namespace sniffer {

class ICaptureService {
public:
    virtual ~ICaptureService() = default;

    virtual std::vector<InterfaceInfo> list_interfaces(std::string& error_message) = 0;
    virtual CaptureResult run_capture(const CaptureConfig& config) = 0;
};

}  // namespace sniffer