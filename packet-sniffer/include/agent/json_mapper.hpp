#pragma once

#include "agent/types.hpp"
#include "sniffer/types.hpp"

#include <string>
#include <vector>

namespace agent {

std::string to_json(const HealthInfo& health);
std::string to_json(const std::vector<sniffer::InterfaceInfo>& interfaces);

}  // namespace agent