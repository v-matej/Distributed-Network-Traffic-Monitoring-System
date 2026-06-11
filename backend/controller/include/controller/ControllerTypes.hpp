#pragma once

#include <cstdint>
#include <ctime>
#include <string>
#include <vector>

namespace controller {

struct ControllerConfig {
    std::string controller_name = "controller";
    std::string version = "0.1.0";
};

struct AgentEndpoint {
    std::string host;
    int port = 8080;
};

struct KnownAgent {
    std::string agent_id;
    std::string display_name;
    std::string host;
    int port = 8080;
    std::time_t created_at = 0;
};

struct AddAgentRequest {
    std::string display_name;
    std::string host;
    int port = 8080;
};

struct RemoteHealthInfo {
    std::string status;
    std::string agent_name;
    std::string version;
    std::string hostname;
};

struct RemoteInterfaceInfo {
    std::string name;
    std::string description;
};

struct RemoteCaptureRequest {
    std::string interface_name;
    std::string filter_expression;
    int packet_count = 0;
    int duration_seconds = 0;
};

struct RemoteCaptureConfig {
    std::string interface_name;
    std::string output_file;
    std::string filter_expression;
    int packet_count = 0;
    int duration_seconds = 0;
    bool live_output = false;
};

struct RemoteCaptureResult {
    bool success = false;
    std::string interface_name;
    std::string output_file;
    std::string filter_expression;
    std::uint64_t packets_captured = 0;
    std::uint64_t bytes_captured = 0;
    std::string stop_reason;
    std::time_t start_time = 0;
    std::time_t end_time = 0;
    std::string error_message;
};

struct RemoteCaptureSessionInfo {
    std::string capture_id;
    std::string status;
    bool stop_requested = false;
    RemoteCaptureConfig config;
    RemoteCaptureResult result;
    std::time_t created_at = 0;
    std::time_t started_at = 0;
    std::time_t finished_at = 0;
};

struct ControllerStoredCaptureInfo {
    std::string agent_id;
    std::string capture_id;
    std::string local_path;
    std::uint64_t file_size_bytes = 0;
    std::time_t fetched_at = 0;
    bool exists = false;
};

struct PcapAnalysisCounter {
    std::string key;
    std::uint64_t packets = 0;
    std::uint64_t bytes = 0;
};

struct PcapProtocolCounters {
    std::uint64_t ethernet = 0;
    std::uint64_t arp = 0;
    std::uint64_t ipv4 = 0;
    std::uint64_t ipv6 = 0;
    std::uint64_t tcp = 0;
    std::uint64_t udp = 0;
    std::uint64_t icmp = 0;
    std::uint64_t icmpv6 = 0;
    std::uint64_t other_l3 = 0;
    std::uint64_t other_l4 = 0;
};

struct PcapAnalysisResult {
    std::string agent_id;
    std::string capture_id;
    std::string local_path;
    std::string datalink_name;

    std::uint64_t file_size_bytes = 0;
    std::uint64_t packet_count = 0;
    std::uint64_t byte_count = 0;

    std::time_t analyzed_at = 0;
    std::time_t first_packet_time = 0;
    std::time_t last_packet_time = 0;
    double duration_seconds = 0.0;

    PcapProtocolCounters protocols;

    std::vector<PcapAnalysisCounter> top_source_ips;
    std::vector<PcapAnalysisCounter> top_destination_ips;
    std::vector<PcapAnalysisCounter> top_source_ports;
    std::vector<PcapAnalysisCounter> top_destination_ports;
};

struct PcapPacketSummary {
    std::uint64_t number = 0;

    std::time_t timestamp = 0;
    int timestamp_microseconds = 0;
    double relative_time_seconds = 0.0;

    std::string source;
    std::string destination;
    std::string protocol;

    std::uint32_t length = 0;
    std::uint32_t captured_length = 0;

    std::string info;
};

struct PcapPacketField {
    std::string name;
    std::string value;
};

struct PcapPacketLayer {
    std::string name;
    std::vector<PcapPacketField> fields;
};

struct PcapHexLine {
    std::uint32_t offset = 0;
    std::string hex;
    std::string ascii;
};

struct PcapPacketDetail {
    PcapPacketSummary summary;
    std::vector<PcapPacketLayer> layers;
    std::vector<PcapHexLine> hex_lines;
};

struct PcapPacketList {
    std::string agent_id;
    std::string capture_id;
    std::string local_path;
    std::string datalink_name;

    std::size_t offset = 0;
    std::size_t limit = 0;
    std::uint64_t total_packets = 0;

    std::vector<PcapPacketSummary> packets;
};

struct KnownAgentWithHealth {
    KnownAgent agent;
    RemoteHealthInfo health;
};

}  // namespace controller