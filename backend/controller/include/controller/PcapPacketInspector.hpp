#pragma once

#include "controller/ControllerTypes.hpp"

#include <cstddef>
#include <cstdint>
#include <string>

namespace controller {

class PcapPacketInspector {
public:
    static bool list_packets(
        const ControllerStoredCaptureInfo& stored_capture,
        std::size_t offset,
        std::size_t limit,
        PcapPacketList& packet_list,
        std::string& error_message
    );

    static bool get_packet_detail(
        const ControllerStoredCaptureInfo& stored_capture,
        std::uint64_t packet_number,
        PcapPacketDetail& packet_detail,
        std::string& error_message
    );
};

}  // namespace controller