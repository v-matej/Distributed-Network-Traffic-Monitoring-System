#pragma once

#include "sniffer/types.hpp"

#include <string>
#include <vector>

namespace sniffer {

std::vector<InterfaceInfo> list_interfaces(std::string& error_message);
CaptureResult run_capture(const CaptureConfig& config);

}  // namespace sniffer