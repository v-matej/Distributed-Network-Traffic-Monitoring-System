#pragma once

#include "controller/ControllerTypes.hpp"

#include <string>

namespace controller {

std::string to_packet_list_json(const PcapPacketList& packet_list);
std::string to_packet_detail_json(const PcapPacketDetail& packet_detail);

}  // namespace controller