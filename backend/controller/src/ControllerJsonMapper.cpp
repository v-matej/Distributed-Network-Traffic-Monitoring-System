#include "controller/ControllerJsonMapper.hpp"

#include <nlohmann/json.hpp>

#include <cstdint>
#include <exception>
#include <utility>

namespace controller {

using json = nlohmann::json;

namespace {

bool parse_time_field(
    const json& j,
    const char* field_name,
    std::time_t& value,
    std::string& error_message
) {
    if (!j.contains(field_name) || !j[field_name].is_number_integer()) {
        error_message = std::string("Missing or invalid '") + field_name + "'";
        return false;
    }

    value = static_cast<std::time_t>(j[field_name].get<long long>());
    return true;
}

bool parse_unsigned_counter_field(
    const json& j,
    const char* field_name,
    std::uint64_t& value,
    std::string& error_message
) {
    if (!j.contains(field_name) ||
        !(j[field_name].is_number_unsigned() || j[field_name].is_number_integer())) {
        error_message = std::string("Missing or invalid '") + field_name + "'";
        return false;
    }

    value = j[field_name].get<std::uint64_t>();
    return true;
}

bool parse_string_field(
    const json& j,
    const char* field_name,
    std::string& value,
    std::string& error_message
) {
    if (!j.contains(field_name) || !j[field_name].is_string()) {
        error_message = std::string("Missing or invalid '") + field_name + "'";
        return false;
    }

    value = j[field_name].get<std::string>();
    return true;
}

bool parse_bool_field(
    const json& j,
    const char* field_name,
    bool& value,
    std::string& error_message
) {
    if (!j.contains(field_name) || !j[field_name].is_boolean()) {
        error_message = std::string("Missing or invalid '") + field_name + "'";
        return false;
    }

    value = j[field_name].get<bool>();
    return true;
}

bool parse_int_field(
    const json& j,
    const char* field_name,
    int& value,
    std::string& error_message
) {
    if (!j.contains(field_name) || !j[field_name].is_number_integer()) {
        error_message = std::string("Missing or invalid '") + field_name + "'";
        return false;
    }

    value = j[field_name].get<int>();
    return true;
}

bool parse_double_field(
    const json& j,
    const char* field_name,
    double& value,
    std::string& error_message
) {
    if (!j.contains(field_name) || !j[field_name].is_number()) {
        error_message = std::string("Missing or invalid '") + field_name + "'";
        return false;
    }

    value = j[field_name].get<double>();
    return true;
}

json agent_to_json_value(const KnownAgent& agent) {
    json j;
    j["agent_id"] = agent.agent_id;
    j["display_name"] = agent.display_name;
    j["host"] = agent.host;
    j["port"] = agent.port;
    j["created_at"] = agent.created_at;
    return j;
}

json health_to_json_value(const RemoteHealthInfo& health) {
    json j;
    j["status"] = health.status;
    j["agent_name"] = health.agent_name;
    j["version"] = health.version;
    j["hostname"] = health.hostname;
    return j;
}

json interfaces_to_json_value(const std::vector<RemoteInterfaceInfo>& interfaces) {
    json interfaces_json = json::array();

    for (const auto& iface : interfaces) {
        interfaces_json.push_back({
            {"name", iface.name},
            {"description", iface.description}
        });
    }

    return interfaces_json;
}

json capture_config_to_json_value(const RemoteCaptureConfig& config) {
    json j;
    j["interface_name"] = config.interface_name;
    j["output_file"] = config.output_file;
    j["filter_expression"] = config.filter_expression;
    j["packet_count"] = config.packet_count;
    j["duration_seconds"] = config.duration_seconds;
    j["live_output"] = config.live_output;
    return j;
}

json capture_result_to_json_value(const RemoteCaptureResult& result) {
    json j;
    j["success"] = result.success;
    j["interface_name"] = result.interface_name;
    j["output_file"] = result.output_file;
    j["filter_expression"] = result.filter_expression;
    j["packets_captured"] = result.packets_captured;
    j["bytes_captured"] = result.bytes_captured;
    j["stop_reason"] = result.stop_reason;
    j["start_time"] = result.start_time;
    j["end_time"] = result.end_time;
    j["error_message"] = result.error_message;
    return j;
}

json capture_to_json_value(const RemoteCaptureSessionInfo& capture) {
    json j;
    j["capture_id"] = capture.capture_id;
    j["status"] = capture.status;
    j["stop_requested"] = capture.stop_requested;
    j["created_at"] = capture.created_at;
    j["started_at"] = capture.started_at;
    j["finished_at"] = capture.finished_at;
    j["config"] = capture_config_to_json_value(capture.config);
    j["result"] = capture_result_to_json_value(capture.result);
    return j;
}

json stored_capture_to_json_value(const ControllerStoredCaptureInfo& stored_capture) {
    json j;
    j["agent_id"] = stored_capture.agent_id;
    j["capture_id"] = stored_capture.capture_id;
    j["local_path"] = stored_capture.local_path;
    j["file_size_bytes"] = stored_capture.file_size_bytes;
    j["fetched_at"] = stored_capture.fetched_at;
    j["exists"] = stored_capture.exists;
    return j;
}

bool parse_remote_capture_config_json(
    const json& j,
    RemoteCaptureConfig& config,
    std::string& error_message
) {
    if (!j.is_object()) {
        error_message = "Invalid 'config' object in capture response";
        return false;
    }

    if (!j.contains("interface_name") || !j["interface_name"].is_string()) {
        error_message = "Missing or invalid 'interface_name' in capture config";
        return false;
    }
    if (!j.contains("output_file") || !j["output_file"].is_string()) {
        error_message = "Missing or invalid 'output_file' in capture config";
        return false;
    }
    if (!j.contains("filter_expression") || !j["filter_expression"].is_string()) {
        error_message = "Missing or invalid 'filter_expression' in capture config";
        return false;
    }
    if (!j.contains("packet_count") || !j["packet_count"].is_number_integer()) {
        error_message = "Missing or invalid 'packet_count' in capture config";
        return false;
    }
    if (!j.contains("duration_seconds") || !j["duration_seconds"].is_number_integer()) {
        error_message = "Missing or invalid 'duration_seconds' in capture config";
        return false;
    }
    if (!j.contains("live_output") || !j["live_output"].is_boolean()) {
        error_message = "Missing or invalid 'live_output' in capture config";
        return false;
    }

    config.interface_name = j["interface_name"].get<std::string>();
    config.output_file = j["output_file"].get<std::string>();
    config.filter_expression = j["filter_expression"].get<std::string>();
    config.packet_count = j["packet_count"].get<int>();
    config.duration_seconds = j["duration_seconds"].get<int>();
    config.live_output = j["live_output"].get<bool>();
    return true;
}

bool parse_remote_capture_result_json(
    const json& j,
    RemoteCaptureResult& result,
    std::string& error_message
) {
    if (!j.is_object()) {
        error_message = "Invalid 'result' object in capture response";
        return false;
    }

    if (!j.contains("success") || !j["success"].is_boolean()) {
        error_message = "Missing or invalid 'success' in capture result";
        return false;
    }
    if (!j.contains("interface_name") || !j["interface_name"].is_string()) {
        error_message = "Missing or invalid 'interface_name' in capture result";
        return false;
    }
    if (!j.contains("output_file") || !j["output_file"].is_string()) {
        error_message = "Missing or invalid 'output_file' in capture result";
        return false;
    }
    if (!j.contains("filter_expression") || !j["filter_expression"].is_string()) {
        error_message = "Missing or invalid 'filter_expression' in capture result";
        return false;
    }
    if (!j.contains("stop_reason") || !j["stop_reason"].is_string()) {
        error_message = "Missing or invalid 'stop_reason' in capture result";
        return false;
    }
    if (!j.contains("error_message") || !j["error_message"].is_string()) {
        error_message = "Missing or invalid 'error_message' in capture result";
        return false;
    }

    result.success = j["success"].get<bool>();
    result.interface_name = j["interface_name"].get<std::string>();
    result.output_file = j["output_file"].get<std::string>();
    result.filter_expression = j["filter_expression"].get<std::string>();

    if (!parse_unsigned_counter_field(j, "packets_captured", result.packets_captured, error_message)) {
        return false;
    }
    if (!parse_unsigned_counter_field(j, "bytes_captured", result.bytes_captured, error_message)) {
        return false;
    }

    result.stop_reason = j["stop_reason"].get<std::string>();

    if (!parse_time_field(j, "start_time", result.start_time, error_message)) {
        return false;
    }
    if (!parse_time_field(j, "end_time", result.end_time, error_message)) {
        return false;
    }

    result.error_message = j["error_message"].get<std::string>();
    return true;
}

bool parse_remote_capture_session_json(
    const json& j,
    RemoteCaptureSessionInfo& capture,
    std::string& error_message
) {
    if (!j.is_object()) {
        error_message = "Invalid capture entry in response";
        return false;
    }

    if (!j.contains("capture_id") || !j["capture_id"].is_string()) {
        error_message = "Missing or invalid 'capture_id' in capture response";
        return false;
    }
    if (!j.contains("status") || !j["status"].is_string()) {
        error_message = "Missing or invalid 'status' in capture response";
        return false;
    }
    if (!j.contains("stop_requested") || !j["stop_requested"].is_boolean()) {
        error_message = "Missing or invalid 'stop_requested' in capture response";
        return false;
    }
    if (!j.contains("config")) {
        error_message = "Missing 'config' in capture response";
        return false;
    }
    if (!j.contains("result")) {
        error_message = "Missing 'result' in capture response";
        return false;
    }

    capture.capture_id = j["capture_id"].get<std::string>();
    capture.status = j["status"].get<std::string>();
    capture.stop_requested = j["stop_requested"].get<bool>();

    if (!parse_time_field(j, "created_at", capture.created_at, error_message)) {
        return false;
    }
    if (!parse_time_field(j, "started_at", capture.started_at, error_message)) {
        return false;
    }
    if (!parse_time_field(j, "finished_at", capture.finished_at, error_message)) {
        return false;
    }

    if (!parse_remote_capture_config_json(j["config"], capture.config, error_message)) {
        return false;
    }
    if (!parse_remote_capture_result_json(j["result"], capture.result, error_message)) {
        return false;
    }

    return true;
}

json analysis_counter_to_json_value(const PcapAnalysisCounter& counter) {
    json j;
    j["key"] = counter.key;
    j["packets"] = counter.packets;
    j["bytes"] = counter.bytes;
    return j;
}

json analysis_counter_vector_to_json_value(
    const std::vector<PcapAnalysisCounter>& counters
) {
    json j = json::array();

    for (const auto& counter : counters) {
        j.push_back(analysis_counter_to_json_value(counter));
    }

    return j;
}

json protocol_counters_to_json_value(const PcapProtocolCounters& protocols) {
    json j;
    j["ethernet"] = protocols.ethernet;
    j["arp"] = protocols.arp;
    j["ipv4"] = protocols.ipv4;
    j["ipv6"] = protocols.ipv6;
    j["tcp"] = protocols.tcp;
    j["udp"] = protocols.udp;
    j["icmp"] = protocols.icmp;
    j["icmpv6"] = protocols.icmpv6;
    j["other_l3"] = protocols.other_l3;
    j["other_l4"] = protocols.other_l4;
    return j;
}

json analysis_summary_to_json_value(const PcapAnalysisSummary& summary) {
    json j;
    j["average_packet_size_bytes"] = summary.average_packet_size_bytes;
    j["main_protocol"] = summary.main_protocol;
    j["main_service"] = summary.main_service;
    j["external_traffic_detected"] = summary.external_traffic_detected;
    j["dns_traffic_detected"] = summary.dns_traffic_detected;
    return j;
}

json protocol_distribution_to_json_value(
    const std::vector<PcapProtocolDistributionEntry>& distribution
) {
    json j = json::array();

    for (const auto& entry : distribution) {
        j.push_back({
            {"name", entry.name},
            {"packets", entry.packets},
            {"bytes", entry.bytes},
            {"percentage", entry.percentage}
        });
    }

    return j;
}

json destination_ip_analysis_to_json_value(const PcapDestinationIpAnalysis& destination) {
    json j;
    j["ip_address"] = destination.ip_address;
    j["classification"] = destination.classification;
    j["country_code"] = destination.country_code;
    j["country_name"] = destination.country_name;
    j["packets"] = destination.packets;
    j["bytes"] = destination.bytes;
    return j;
}

json destination_ip_analysis_vector_to_json_value(
    const std::vector<PcapDestinationIpAnalysis>& destinations
) {
    json j = json::array();

    for (const auto& destination : destinations) {
        j.push_back(destination_ip_analysis_to_json_value(destination));
    }

    return j;
}

json service_analysis_to_json_value(const PcapServiceAnalysis& service) {
    json j;
    j["service_name"] = service.service_name;
    j["transport_protocol"] = service.transport_protocol;
    j["port"] = service.port;
    j["packets"] = service.packets;
    j["bytes"] = service.bytes;
    return j;
}

json service_analysis_vector_to_json_value(
    const std::vector<PcapServiceAnalysis>& services
) {
    json j = json::array();

    for (const auto& service : services) {
        j.push_back(service_analysis_to_json_value(service));
    }

    return j;
}

json conversation_analysis_to_json_value(const PcapConversationAnalysis& conversation) {
    json j;
    j["source_ip"] = conversation.source_ip;
    j["destination_ip"] = conversation.destination_ip;
    j["destination_classification"] = conversation.destination_classification;
    j["destination_country_code"] = conversation.destination_country_code;
    j["destination_country_name"] = conversation.destination_country_name;
    j["transport_protocol"] = conversation.transport_protocol;
    j["service_name"] = conversation.service_name;
    j["service_port"] = conversation.service_port;
    j["packets"] = conversation.packets;
    j["bytes"] = conversation.bytes;
    return j;
}

json conversation_analysis_vector_to_json_value(
    const std::vector<PcapConversationAnalysis>& conversations
) {
    json j = json::array();

    for (const auto& conversation : conversations) {
        j.push_back(conversation_analysis_to_json_value(conversation));
    }

    return j;
}

bool parse_analysis_counter_json(
    const json& j,
    PcapAnalysisCounter& counter,
    std::string& error_message
) {
    if (!j.is_object()) {
        error_message = "Invalid analysis counter entry";
        return false;
    }

    return parse_string_field(j, "key", counter.key, error_message)
        && parse_unsigned_counter_field(j, "packets", counter.packets, error_message)
        && parse_unsigned_counter_field(j, "bytes", counter.bytes, error_message);
}

bool parse_analysis_counter_vector_json(
    const json& j,
    std::vector<PcapAnalysisCounter>& counters,
    std::string& error_message
) {
    if (!j.is_array()) {
        error_message = "Invalid analysis counter array";
        return false;
    }

    counters.clear();
    for (const auto& item : j) {
        PcapAnalysisCounter counter;
        if (!parse_analysis_counter_json(item, counter, error_message)) {
            return false;
        }
        counters.push_back(std::move(counter));
    }

    return true;
}

bool parse_protocol_counters_json(
    const json& j,
    PcapProtocolCounters& protocols,
    std::string& error_message
) {
    if (!j.is_object()) {
        error_message = "Invalid protocol counters object";
        return false;
    }

    return parse_unsigned_counter_field(j, "ethernet", protocols.ethernet, error_message)
        && parse_unsigned_counter_field(j, "arp", protocols.arp, error_message)
        && parse_unsigned_counter_field(j, "ipv4", protocols.ipv4, error_message)
        && parse_unsigned_counter_field(j, "ipv6", protocols.ipv6, error_message)
        && parse_unsigned_counter_field(j, "tcp", protocols.tcp, error_message)
        && parse_unsigned_counter_field(j, "udp", protocols.udp, error_message)
        && parse_unsigned_counter_field(j, "icmp", protocols.icmp, error_message)
        && parse_unsigned_counter_field(j, "icmpv6", protocols.icmpv6, error_message)
        && parse_unsigned_counter_field(j, "other_l3", protocols.other_l3, error_message)
        && parse_unsigned_counter_field(j, "other_l4", protocols.other_l4, error_message);
}

bool parse_analysis_summary_json(
    const json& j,
    PcapAnalysisSummary& summary,
    std::string& error_message
) {
    if (!j.is_object()) {
        error_message = "Invalid analysis summary object";
        return false;
    }

    return parse_double_field(j, "average_packet_size_bytes", summary.average_packet_size_bytes, error_message)
        && parse_string_field(j, "main_protocol", summary.main_protocol, error_message)
        && parse_string_field(j, "main_service", summary.main_service, error_message)
        && parse_bool_field(j, "external_traffic_detected", summary.external_traffic_detected, error_message)
        && parse_bool_field(j, "dns_traffic_detected", summary.dns_traffic_detected, error_message);
}

bool parse_protocol_distribution_json(
    const json& j,
    std::vector<PcapProtocolDistributionEntry>& distribution,
    std::string& error_message
) {
    if (!j.is_array()) {
        error_message = "Invalid protocol distribution array";
        return false;
    }

    distribution.clear();
    for (const auto& item : j) {
        if (!item.is_object()) {
            error_message = "Invalid protocol distribution entry";
            return false;
        }

        PcapProtocolDistributionEntry entry;
        if (!parse_string_field(item, "name", entry.name, error_message)
            || !parse_unsigned_counter_field(item, "packets", entry.packets, error_message)
            || !parse_unsigned_counter_field(item, "bytes", entry.bytes, error_message)
            || !parse_double_field(item, "percentage", entry.percentage, error_message)) {
            return false;
        }

        distribution.push_back(std::move(entry));
    }

    return true;
}

bool parse_destination_ip_analysis_json(
    const json& j,
    PcapDestinationIpAnalysis& destination,
    std::string& error_message
) {
    if (!j.is_object()) {
        error_message = "Invalid destination IP analysis entry";
        return false;
    }

    return parse_string_field(j, "ip_address", destination.ip_address, error_message)
        && parse_string_field(j, "classification", destination.classification, error_message)
        && parse_string_field(j, "country_code", destination.country_code, error_message)
        && parse_string_field(j, "country_name", destination.country_name, error_message)
        && parse_unsigned_counter_field(j, "packets", destination.packets, error_message)
        && parse_unsigned_counter_field(j, "bytes", destination.bytes, error_message);
}

bool parse_destination_ip_analysis_vector_json(
    const json& j,
    std::vector<PcapDestinationIpAnalysis>& destinations,
    std::string& error_message
) {
    if (!j.is_array()) {
        error_message = "Invalid destination IP analysis array";
        return false;
    }

    destinations.clear();
    for (const auto& item : j) {
        PcapDestinationIpAnalysis destination;
        if (!parse_destination_ip_analysis_json(item, destination, error_message)) {
            return false;
        }
        destinations.push_back(std::move(destination));
    }

    return true;
}

bool parse_service_analysis_json(
    const json& j,
    PcapServiceAnalysis& service,
    std::string& error_message
) {
    if (!j.is_object()) {
        error_message = "Invalid service analysis entry";
        return false;
    }

    return parse_string_field(j, "service_name", service.service_name, error_message)
        && parse_string_field(j, "transport_protocol", service.transport_protocol, error_message)
        && parse_int_field(j, "port", service.port, error_message)
        && parse_unsigned_counter_field(j, "packets", service.packets, error_message)
        && parse_unsigned_counter_field(j, "bytes", service.bytes, error_message);
}

bool parse_service_analysis_vector_json(
    const json& j,
    std::vector<PcapServiceAnalysis>& services,
    std::string& error_message
) {
    if (!j.is_array()) {
        error_message = "Invalid service analysis array";
        return false;
    }

    services.clear();
    for (const auto& item : j) {
        PcapServiceAnalysis service;
        if (!parse_service_analysis_json(item, service, error_message)) {
            return false;
        }
        services.push_back(std::move(service));
    }

    return true;
}

bool parse_conversation_analysis_json(
    const json& j,
    PcapConversationAnalysis& conversation,
    std::string& error_message
) {
    if (!j.is_object()) {
        error_message = "Invalid conversation analysis entry";
        return false;
    }

    return parse_string_field(j, "source_ip", conversation.source_ip, error_message)
        && parse_string_field(j, "destination_ip", conversation.destination_ip, error_message)
        && parse_string_field(j, "destination_classification", conversation.destination_classification, error_message)
        && parse_string_field(j, "destination_country_code", conversation.destination_country_code, error_message)
        && parse_string_field(j, "destination_country_name", conversation.destination_country_name, error_message)
        && parse_string_field(j, "transport_protocol", conversation.transport_protocol, error_message)
        && parse_string_field(j, "service_name", conversation.service_name, error_message)
        && parse_int_field(j, "service_port", conversation.service_port, error_message)
        && parse_unsigned_counter_field(j, "packets", conversation.packets, error_message)
        && parse_unsigned_counter_field(j, "bytes", conversation.bytes, error_message);
}

bool parse_conversation_analysis_vector_json(
    const json& j,
    std::vector<PcapConversationAnalysis>& conversations,
    std::string& error_message
) {
    if (!j.is_array()) {
        error_message = "Invalid conversation analysis array";
        return false;
    }

    conversations.clear();
    for (const auto& item : j) {
        PcapConversationAnalysis conversation;
        if (!parse_conversation_analysis_json(item, conversation, error_message)) {
            return false;
        }
        conversations.push_back(std::move(conversation));
    }

    return true;
}

bool parse_pcap_analysis_value_json(
    const json& j,
    PcapAnalysisResult& analysis,
    std::string& error_message
) {
    if (!j.is_object()) {
        error_message = "Invalid analysis object";
        return false;
    }

    if (!parse_string_field(j, "agent_id", analysis.agent_id, error_message)
        || !parse_string_field(j, "capture_id", analysis.capture_id, error_message)
        || !parse_string_field(j, "local_path", analysis.local_path, error_message)
        || !parse_string_field(j, "datalink_name", analysis.datalink_name, error_message)
        || !parse_unsigned_counter_field(j, "file_size_bytes", analysis.file_size_bytes, error_message)
        || !parse_unsigned_counter_field(j, "packet_count", analysis.packet_count, error_message)
        || !parse_unsigned_counter_field(j, "byte_count", analysis.byte_count, error_message)
        || !parse_time_field(j, "analyzed_at", analysis.analyzed_at, error_message)
        || !parse_time_field(j, "first_packet_time", analysis.first_packet_time, error_message)
        || !parse_time_field(j, "last_packet_time", analysis.last_packet_time, error_message)
        || !parse_double_field(j, "duration_seconds", analysis.duration_seconds, error_message)) {
        return false;
    }

    if (!j.contains("protocols") ||
        !parse_protocol_counters_json(j["protocols"], analysis.protocols, error_message)) {
        return false;
    }

    if (j.contains("summary") &&
        !parse_analysis_summary_json(j["summary"], analysis.summary, error_message)) {
        return false;
    }

    if (j.contains("protocol_distribution") &&
        !parse_protocol_distribution_json(j["protocol_distribution"], analysis.protocol_distribution, error_message)) {
        return false;
    }

    if (j.contains("top_conversations") &&
        !parse_conversation_analysis_vector_json(j["top_conversations"], analysis.top_conversations, error_message)) {
        return false;
    }

    if (j.contains("top_destination_services") &&
        !parse_service_analysis_vector_json(j["top_destination_services"], analysis.top_destination_services, error_message)) {
        return false;
    }

    if (j.contains("top_destination_ip_details") &&
        !parse_destination_ip_analysis_vector_json(j["top_destination_ip_details"], analysis.top_destination_ip_details, error_message)) {
        return false;
    }

    return j.contains("top_source_ips")
        && parse_analysis_counter_vector_json(j["top_source_ips"], analysis.top_source_ips, error_message)
        && j.contains("top_destination_ips")
        && parse_analysis_counter_vector_json(j["top_destination_ips"], analysis.top_destination_ips, error_message)
        && j.contains("top_source_ports")
        && parse_analysis_counter_vector_json(j["top_source_ports"], analysis.top_source_ports, error_message)
        && j.contains("top_destination_ports")
        && parse_analysis_counter_vector_json(j["top_destination_ports"], analysis.top_destination_ports, error_message);
}

json pcap_analysis_to_json_value(const PcapAnalysisResult& analysis) {
    json j;
    j["agent_id"] = analysis.agent_id;
    j["capture_id"] = analysis.capture_id;
    j["local_path"] = analysis.local_path;
    j["datalink_name"] = analysis.datalink_name;
    j["file_size_bytes"] = analysis.file_size_bytes;
    j["packet_count"] = analysis.packet_count;
    j["byte_count"] = analysis.byte_count;
    j["analyzed_at"] = analysis.analyzed_at;
    j["first_packet_time"] = analysis.first_packet_time;
    j["last_packet_time"] = analysis.last_packet_time;
    j["duration_seconds"] = analysis.duration_seconds;
    j["protocols"] = protocol_counters_to_json_value(analysis.protocols);
    j["summary"] = analysis_summary_to_json_value(analysis.summary);
    j["protocol_distribution"] =
        protocol_distribution_to_json_value(analysis.protocol_distribution);
    j["top_conversations"] =
        conversation_analysis_vector_to_json_value(analysis.top_conversations);
    j["top_destination_services"] =
        service_analysis_vector_to_json_value(analysis.top_destination_services);
    j["top_destination_ip_details"] =
        destination_ip_analysis_vector_to_json_value(analysis.top_destination_ip_details);
    j["top_source_ips"] = analysis_counter_vector_to_json_value(analysis.top_source_ips);
    j["top_destination_ips"] = analysis_counter_vector_to_json_value(analysis.top_destination_ips);
    j["top_source_ports"] = analysis_counter_vector_to_json_value(analysis.top_source_ports);
    j["top_destination_ports"] = analysis_counter_vector_to_json_value(analysis.top_destination_ports);
    return j;
}

}  // namespace

std::string make_error_json(const std::string& message) {
    json j;
    j["status"] = "error";
    j["message"] = message;
    return j.dump(4);
}

std::string to_json(const KnownAgent& agent) {
    return agent_to_json_value(agent).dump(4);
}

std::string to_json(const std::vector<KnownAgent>& agents) {
    json j;
    j["agents"] = json::array();

    for (const auto& agent : agents) {
        j["agents"].push_back(agent_to_json_value(agent));
    }

    return j.dump(4);
}

std::string to_json(const KnownAgentWithHealth& agent_with_health) {
    json j;
    j["agent"] = agent_to_json_value(agent_with_health.agent);
    j["health"] = health_to_json_value(agent_with_health.health);
    return j.dump(4);
}

std::string to_json(const KnownAgent& agent, const std::vector<RemoteInterfaceInfo>& interfaces) {
    json j;
    j["agent"] = agent_to_json_value(agent);
    j["interfaces"] = interfaces_to_json_value(interfaces);
    return j.dump(4);
}

std::string to_json(const KnownAgent& agent, const RemoteCaptureSessionInfo& capture) {
    json j;
    j["agent"] = agent_to_json_value(agent);
    j["capture"] = capture_to_json_value(capture);
    return j.dump(4);
}

std::string to_json(const KnownAgent& agent, const std::vector<RemoteCaptureSessionInfo>& captures) {
    json j;
    j["agent"] = agent_to_json_value(agent);
    j["captures"] = json::array();

    for (const auto& capture : captures) {
        j["captures"].push_back(capture_to_json_value(capture));
    }

    return j.dump(4);
}

std::string to_json(const ControllerStoredCaptureInfo& stored_capture) {
    json j;
    j["stored_capture"] = stored_capture_to_json_value(stored_capture);
    return j.dump(4);
}

std::string to_json(const std::vector<ControllerStoredCaptureInfo>& stored_captures) {
    json j;
    j["stored_captures"] = json::array();

    for (const auto& stored_capture : stored_captures) {
        j["stored_captures"].push_back(stored_capture_to_json_value(stored_capture));
    }

    return j.dump(4);
}

std::string to_json(const PcapAnalysisResult& analysis) {
    json j;
    j["analysis"] = pcap_analysis_to_json_value(analysis);
    return j.dump(4);
}

bool parse_pcap_analysis_json(
    const std::string& response_body,
    PcapAnalysisResult& analysis,
    std::string& error_message
) {
    try {
        const json j = json::parse(response_body);

        if (!j.contains("analysis")) {
            error_message = "Missing analysis object";
            return false;
        }

        return parse_pcap_analysis_value_json(j["analysis"], analysis, error_message);
    } catch (const std::exception& ex) {
        error_message = std::string("Invalid analysis JSON: ") + ex.what();
        return false;
    }
}

bool parse_add_agent_request_json(
    const std::string& request_body,
    AddAgentRequest& request,
    std::string& error_message
) {
    try {
        const json j = json::parse(request_body);

        if (!j.contains("host") || !j["host"].is_string()) {
            error_message = "Missing or invalid 'host'";
            return false;
        }

        request.host = j["host"].get<std::string>();

        if (j.contains("display_name")) {
            if (!j["display_name"].is_string()) {
                error_message = "Invalid 'display_name'";
                return false;
            }
            request.display_name = j["display_name"].get<std::string>();
        }

        if (j.contains("port")) {
            if (!j["port"].is_number_integer()) {
                error_message = "Invalid 'port'";
                return false;
            }
            request.port = j["port"].get<int>();
        }

        return true;
    } catch (const std::exception& ex) {
        error_message = std::string("Invalid JSON: ") + ex.what();
        return false;
    }
}

bool parse_remote_capture_request_json(
    const std::string& request_body,
    RemoteCaptureRequest& request,
    std::string& error_message
) {
    try {
        const json j = json::parse(request_body);

        if (!j.contains("interface_name") || !j["interface_name"].is_string()) {
            error_message = "Missing or invalid 'interface_name'";
            return false;
        }

        request.interface_name = j["interface_name"].get<std::string>();
        if (request.interface_name.empty()) {
            error_message = "'interface_name' must not be empty";
            return false;
        }

        if (j.contains("filter_expression")) {
            if (!j["filter_expression"].is_string()) {
                error_message = "Invalid 'filter_expression'";
                return false;
            }
            request.filter_expression = j["filter_expression"].get<std::string>();
        }

        if (j.contains("packet_count")) {
            if (!j["packet_count"].is_number_integer()) {
                error_message = "Invalid 'packet_count'";
                return false;
            }
            request.packet_count = j["packet_count"].get<int>();
            if (request.packet_count < 0) {
                error_message = "'packet_count' must be >= 0";
                return false;
            }
        }

        if (j.contains("duration_seconds")) {
            if (!j["duration_seconds"].is_number_integer()) {
                error_message = "Invalid 'duration_seconds'";
                return false;
            }
            request.duration_seconds = j["duration_seconds"].get<int>();
            if (request.duration_seconds < 0) {
                error_message = "'duration_seconds' must be >= 0";
                return false;
            }
        }

        return true;
    } catch (const std::exception& ex) {
        error_message = std::string("Invalid JSON: ") + ex.what();
        return false;
    }
}

std::string to_agent_capture_request_json(const RemoteCaptureRequest& request) {
    json j;
    j["interface_name"] = request.interface_name;

    if (!request.filter_expression.empty()) {
        j["filter_expression"] = request.filter_expression;
    }
    if (request.packet_count > 0) {
        j["packet_count"] = request.packet_count;
    }
    if (request.duration_seconds > 0) {
        j["duration_seconds"] = request.duration_seconds;
    }

    return j.dump();
}

bool parse_remote_health_json(
    const std::string& response_body,
    RemoteHealthInfo& health,
    std::string& error_message
) {
    try {
        const json j = json::parse(response_body);

        if (!j.contains("status") || !j["status"].is_string()) {
            error_message = "Missing or invalid 'status' in health response";
            return false;
        }
        if (!j.contains("agent_name") || !j["agent_name"].is_string()) {
            error_message = "Missing or invalid 'agent_name' in health response";
            return false;
        }
        if (!j.contains("version") || !j["version"].is_string()) {
            error_message = "Missing or invalid 'version' in health response";
            return false;
        }
        if (!j.contains("hostname") || !j["hostname"].is_string()) {
            error_message = "Missing or invalid 'hostname' in health response";
            return false;
        }

        health.status = j["status"].get<std::string>();
        health.agent_name = j["agent_name"].get<std::string>();
        health.version = j["version"].get<std::string>();
        health.hostname = j["hostname"].get<std::string>();
        return true;
    } catch (const std::exception& ex) {
        error_message = std::string("Invalid health JSON: ") + ex.what();
        return false;
    }
}

bool parse_remote_interfaces_json(
    const std::string& response_body,
    std::vector<RemoteInterfaceInfo>& interfaces,
    std::string& error_message
) {
    try {
        const json j = json::parse(response_body);

        if (!j.contains("interfaces") || !j["interfaces"].is_array()) {
            error_message = "Missing or invalid 'interfaces' in response";
            return false;
        }

        interfaces.clear();
        for (const auto& item : j["interfaces"]) {
            if (!item.is_object()) {
                error_message = "Invalid interface entry in response";
                return false;
            }
            if (!item.contains("name") || !item["name"].is_string()) {
                error_message = "Missing or invalid 'name' in interface entry";
                return false;
            }

            RemoteInterfaceInfo iface;
            iface.name = item["name"].get<std::string>();

            if (item.contains("description") && item["description"].is_string()) {
                iface.description = item["description"].get<std::string>();
            }

            interfaces.push_back(iface);
        }

        return true;
    } catch (const std::exception& ex) {
        error_message = std::string("Invalid interfaces JSON: ") + ex.what();
        return false;
    }
}

bool parse_remote_capture_json(
    const std::string& response_body,
    RemoteCaptureSessionInfo& capture,
    std::string& error_message
) {
    try {
        const json j = json::parse(response_body);
        return parse_remote_capture_session_json(j, capture, error_message);
    } catch (const std::exception& ex) {
        error_message = std::string("Invalid capture JSON: ") + ex.what();
        return false;
    }
}

bool parse_remote_captures_json(
    const std::string& response_body,
    std::vector<RemoteCaptureSessionInfo>& captures,
    std::string& error_message
) {
    try {
        const json j = json::parse(response_body);

        if (!j.contains("captures") || !j["captures"].is_array()) {
            error_message = "Missing or invalid 'captures' in response";
            return false;
        }

        captures.clear();
        for (const auto& item : j["captures"]) {
            RemoteCaptureSessionInfo capture;
            if (!parse_remote_capture_session_json(item, capture, error_message)) {
                return false;
            }
            captures.push_back(std::move(capture));
        }

        return true;
    } catch (const std::exception& ex) {
        error_message = std::string("Invalid captures JSON: ") + ex.what();
        return false;
    }
}

bool parse_error_message_json(const std::string& response_body, std::string& message) {
    try {
        const json j = json::parse(response_body);
        if (j.contains("message") && j["message"].is_string()) {
            message = j["message"].get<std::string>();
            return true;
        }
    } catch (const std::exception&) {
    }

    return false;
}

}  // namespace controller
