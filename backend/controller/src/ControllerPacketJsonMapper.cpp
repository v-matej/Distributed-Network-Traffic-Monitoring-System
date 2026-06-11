#include "controller/ControllerPacketJsonMapper.hpp"

#include <nlohmann/json.hpp>

namespace controller {

using json = nlohmann::json;

namespace {

json packet_summary_to_json_value(const PcapPacketSummary& packet) {
    json j;
    j["number"] = packet.number;
    j["timestamp"] = packet.timestamp;
    j["timestamp_microseconds"] = packet.timestamp_microseconds;
    j["relative_time_seconds"] = packet.relative_time_seconds;
    j["source"] = packet.source;
    j["destination"] = packet.destination;
    j["protocol"] = packet.protocol;
    j["length"] = packet.length;
    j["captured_length"] = packet.captured_length;
    j["info"] = packet.info;
    return j;
}

json packet_field_to_json_value(const PcapPacketField& field) {
    json j;
    j["name"] = field.name;
    j["value"] = field.value;
    return j;
}

json packet_layer_to_json_value(const PcapPacketLayer& layer) {
    json j;
    j["name"] = layer.name;
    j["fields"] = json::array();

    for (const auto& field : layer.fields) {
        j["fields"].push_back(packet_field_to_json_value(field));
    }

    return j;
}

json hex_line_to_json_value(const PcapHexLine& line) {
    json j;
    j["offset"] = line.offset;
    j["hex"] = line.hex;
    j["ascii"] = line.ascii;
    return j;
}

}  // namespace

std::string to_packet_list_json(const PcapPacketList& packet_list) {
    json root;
    json list;

    list["agent_id"] = packet_list.agent_id;
    list["capture_id"] = packet_list.capture_id;
    list["local_path"] = packet_list.local_path;
    list["datalink_name"] = packet_list.datalink_name;
    list["offset"] = packet_list.offset;
    list["limit"] = packet_list.limit;
    list["total_packets"] = packet_list.total_packets;
    list["packets"] = json::array();

    for (const auto& packet : packet_list.packets) {
        list["packets"].push_back(packet_summary_to_json_value(packet));
    }

    root["packet_list"] = list;
    return root.dump(4);
}

std::string to_packet_detail_json(const PcapPacketDetail& packet_detail) {
    json root;
    json packet;

    packet["summary"] = packet_summary_to_json_value(packet_detail.summary);
    packet["layers"] = json::array();
    packet["hex_lines"] = json::array();

    for (const auto& layer : packet_detail.layers) {
        packet["layers"].push_back(packet_layer_to_json_value(layer));
    }

    for (const auto& line : packet_detail.hex_lines) {
        packet["hex_lines"].push_back(hex_line_to_json_value(line));
    }

    root["packet"] = packet;
    return root.dump(4);
}

}  // namespace controller