#include "controller/PcapAnalyzer.hpp"

#include <pcap/pcap.h>

#include <arpa/inet.h>

#include <algorithm>
#include <array>
#include <cstdint>
#include <ctime>
#include <string>
#include <unordered_map>
#include <vector>

namespace controller {

namespace {

constexpr std::uint16_t kEtherTypeIpv4 = 0x0800;
constexpr std::uint16_t kEtherTypeArp = 0x0806;
constexpr std::uint16_t kEtherTypeVlan = 0x8100;
constexpr std::uint16_t kEtherTypeIpv6 = 0x86DD;

constexpr std::uint8_t kProtocolIcmp = 1;
constexpr std::uint8_t kProtocolTcp = 6;
constexpr std::uint8_t kProtocolUdp = 17;
constexpr std::uint8_t kProtocolIcmpv6 = 58;

struct CounterValue {
    std::uint64_t packets = 0;
    std::uint64_t bytes = 0;
};

struct CountryInfo {
    std::string code = "unknown";
    std::string name = "unknown";
};

using CounterMap = std::unordered_map<std::string, CounterValue>;
using ServiceMap = std::unordered_map<std::string, PcapServiceAnalysis>;
using ConversationMap = std::unordered_map<std::string, PcapConversationAnalysis>;

std::uint16_t read_u16_be(const unsigned char* data) {
    return static_cast<std::uint16_t>(
        (static_cast<std::uint16_t>(data[0]) << 8U) |
        static_cast<std::uint16_t>(data[1])
    );
}

std::string ipv4_to_string(const unsigned char* data) {
    std::array<char, INET_ADDRSTRLEN> buffer {};
    const auto* result = inet_ntop(AF_INET, data, buffer.data(), buffer.size());
    return result ? std::string(result) : std::string("invalid-ipv4");
}

std::string ipv6_to_string(const unsigned char* data) {
    std::array<char, INET6_ADDRSTRLEN> buffer {};
    const auto* result = inet_ntop(AF_INET6, data, buffer.data(), buffer.size());
    return result ? std::string(result) : std::string("invalid-ipv6");
}

void increment_counter(
    CounterMap& counters,
    const std::string& key,
    std::uint64_t bytes
) {
    auto& counter = counters[key];
    counter.packets += 1;
    counter.bytes += bytes;
}

std::vector<PcapAnalysisCounter> make_top_counters(
    const CounterMap& counters,
    std::size_t limit
) {
    std::vector<PcapAnalysisCounter> result;
    result.reserve(counters.size());

    for (const auto& [key, value] : counters) {
        result.push_back({
            key,
            value.packets,
            value.bytes
        });
    }

    std::sort(
        result.begin(),
        result.end(),
        [](const PcapAnalysisCounter& left, const PcapAnalysisCounter& right) {
            if (left.packets != right.packets) {
                return left.packets > right.packets;
            }

            if (left.bytes != right.bytes) {
                return left.bytes > right.bytes;
            }

            return left.key < right.key;
        }
    );

    if (result.size() > limit) {
        result.resize(limit);
    }

    return result;
}

bool is_ipv6_unspecified(const in6_addr& address) {
    for (const auto byte : address.s6_addr) {
        if (byte != 0) {
            return false;
        }
    }

    return true;
}

bool is_ipv6_loopback(const in6_addr& address) {
    for (std::size_t index = 0; index < 15; ++index) {
        if (address.s6_addr[index] != 0) {
            return false;
        }
    }

    return address.s6_addr[15] == 1;
}

std::string classify_ip_address(const std::string& ip_address) {
    in_addr ipv4 {};
    if (inet_pton(AF_INET, ip_address.c_str(), &ipv4) == 1) {
        const auto address = ntohl(ipv4.s_addr);

        if (address == 0) {
            return "unknown";
        }
        if (address == 0xFFFFFFFFU) {
            return "broadcast";
        }
        if ((address & 0xF0000000U) == 0xE0000000U) {
            return "multicast";
        }
        if ((address & 0xFF000000U) == 0x7F000000U) {
            return "loopback";
        }
        if ((address & 0xFFFF0000U) == 0xA9FE0000U) {
            return "link-local";
        }
        if ((address & 0xFF000000U) == 0x0A000000U ||
            (address & 0xFFF00000U) == 0xAC100000U ||
            (address & 0xFFFF0000U) == 0xC0A80000U) {
            return "private/local";
        }

        return "public";
    }

    in6_addr ipv6 {};
    if (inet_pton(AF_INET6, ip_address.c_str(), &ipv6) == 1) {
        if (is_ipv6_unspecified(ipv6)) {
            return "unknown";
        }
        if (is_ipv6_loopback(ipv6)) {
            return "loopback";
        }
        if (ipv6.s6_addr[0] == 0xFFU) {
            return "multicast";
        }
        if (ipv6.s6_addr[0] == 0xFEU && (ipv6.s6_addr[1] & 0xC0U) == 0x80U) {
            return "link-local";
        }
        if ((ipv6.s6_addr[0] & 0xFEU) == 0xFCU) {
            return "private/local";
        }

        return "public";
    }

    return "unknown";
}

CountryInfo lookup_country_for_ip(const std::string& ip_address) {
    (void)ip_address;
    return {};
}

std::string service_name_for(const std::string& protocol, std::uint16_t port) {
    if (protocol == "tcp") {
        switch (port) {
            case 22:
                return "SSH";
            case 53:
                return "DNS";
            case 80:
                return "HTTP";
            case 443:
                return "HTTPS";
            case 445:
                return "SMB";
            case 3306:
                return "MySQL";
            case 5432:
                return "PostgreSQL";
            case 8080:
                return "HTTP alternate";
            default:
                return "Unknown";
        }
    }

    if (protocol == "udp") {
        switch (port) {
            case 53:
                return "DNS";
            case 123:
                return "NTP";
            default:
                return "Unknown";
        }
    }

    return "Unknown";
}

std::string service_display_name(const PcapServiceAnalysis& service) {
    if (!service.service_name.empty() && service.service_name != "Unknown") {
        return service.service_name;
    }

    if (!service.transport_protocol.empty() && service.port > 0) {
        return service.transport_protocol + "/" + std::to_string(service.port);
    }

    return "unknown";
}

std::string main_service_display_name(const std::vector<PcapServiceAnalysis>& services) {
    for (const auto& service : services) {
        if (!service.service_name.empty() && service.service_name != "Unknown") {
            return service_display_name(service);
        }
    }

    if (!services.empty()) {
        return service_display_name(services.front());
    }

    return "unknown";
}

void increment_service(
    ServiceMap& services,
    const std::string& protocol,
    std::uint16_t port,
    std::uint64_t bytes
) {
    const auto key = protocol + "/" + std::to_string(port);
    auto& service = services[key];

    if (service.transport_protocol.empty()) {
        service.transport_protocol = protocol;
        service.port = port;
        service.service_name = service_name_for(protocol, port);
    }

    service.packets += 1;
    service.bytes += bytes;
}

void increment_conversation(
    ConversationMap& conversations,
    const std::string& source_ip,
    const std::string& destination_ip,
    const std::string& protocol,
    std::uint16_t service_port,
    const std::string& service_name,
    std::uint64_t bytes
) {
    const auto key = source_ip + "|" + destination_ip + "|" + protocol + "|"
        + std::to_string(service_port);
    auto& conversation = conversations[key];

    if (conversation.source_ip.empty()) {
        const auto classification = classify_ip_address(destination_ip);
        const auto country = classification == "public"
            ? lookup_country_for_ip(destination_ip)
            : CountryInfo {};

        conversation.source_ip = source_ip;
        conversation.destination_ip = destination_ip;
        conversation.destination_classification = classification;
        conversation.destination_country_code = country.code;
        conversation.destination_country_name = country.name;
        conversation.transport_protocol = protocol;
        conversation.service_name = service_name;
        conversation.service_port = service_port;
    }

    conversation.packets += 1;
    conversation.bytes += bytes;
}

std::vector<PcapServiceAnalysis> make_top_services(
    const ServiceMap& services,
    std::size_t limit
) {
    std::vector<PcapServiceAnalysis> result;
    result.reserve(services.size());

    for (const auto& [_, service] : services) {
        result.push_back(service);
    }

    std::sort(
        result.begin(),
        result.end(),
        [](const PcapServiceAnalysis& left, const PcapServiceAnalysis& right) {
            if (left.packets != right.packets) {
                return left.packets > right.packets;
            }
            if (left.bytes != right.bytes) {
                return left.bytes > right.bytes;
            }
            if (left.transport_protocol != right.transport_protocol) {
                return left.transport_protocol < right.transport_protocol;
            }
            return left.port < right.port;
        }
    );

    if (result.size() > limit) {
        result.resize(limit);
    }

    return result;
}

std::vector<PcapConversationAnalysis> make_top_conversations(
    const ConversationMap& conversations,
    std::size_t limit
) {
    std::vector<PcapConversationAnalysis> result;
    result.reserve(conversations.size());

    for (const auto& [_, conversation] : conversations) {
        result.push_back(conversation);
    }

    std::sort(
        result.begin(),
        result.end(),
        [](const PcapConversationAnalysis& left, const PcapConversationAnalysis& right) {
            if (left.packets != right.packets) {
                return left.packets > right.packets;
            }
            if (left.bytes != right.bytes) {
                return left.bytes > right.bytes;
            }
            if (left.source_ip != right.source_ip) {
                return left.source_ip < right.source_ip;
            }
            return left.destination_ip < right.destination_ip;
        }
    );

    if (result.size() > limit) {
        result.resize(limit);
    }

    return result;
}

std::vector<PcapProtocolDistributionEntry> make_protocol_distribution(
    const CounterMap& protocols,
    std::uint64_t total_packets
) {
    std::vector<PcapProtocolDistributionEntry> result;
    result.reserve(protocols.size());

    for (const auto& [name, value] : protocols) {
        PcapProtocolDistributionEntry entry;
        entry.name = name;
        entry.packets = value.packets;
        entry.bytes = value.bytes;
        entry.percentage = total_packets == 0
            ? 0.0
            : (static_cast<double>(value.packets) * 100.0)
                / static_cast<double>(total_packets);
        result.push_back(entry);
    }

    std::sort(
        result.begin(),
        result.end(),
        [](const PcapProtocolDistributionEntry& left, const PcapProtocolDistributionEntry& right) {
            if (left.packets != right.packets) {
                return left.packets > right.packets;
            }
            if (left.bytes != right.bytes) {
                return left.bytes > right.bytes;
            }
            return left.name < right.name;
        }
    );

    return result;
}

std::vector<PcapDestinationIpAnalysis> make_top_destination_ip_details(
    const CounterMap& destination_ips,
    std::size_t limit
) {
    std::vector<PcapDestinationIpAnalysis> result;
    result.reserve(destination_ips.size());

    for (const auto& [ip_address, value] : destination_ips) {
        const auto classification = classify_ip_address(ip_address);
        const auto country = classification == "public"
            ? lookup_country_for_ip(ip_address)
            : CountryInfo {};

        result.push_back({
            ip_address,
            classification,
            country.code,
            country.name,
            value.packets,
            value.bytes
        });
    }

    std::sort(
        result.begin(),
        result.end(),
        [](const PcapDestinationIpAnalysis& left, const PcapDestinationIpAnalysis& right) {
            if (left.packets != right.packets) {
                return left.packets > right.packets;
            }
            if (left.bytes != right.bytes) {
                return left.bytes > right.bytes;
            }
            return left.ip_address < right.ip_address;
        }
    );

    if (result.size() > limit) {
        result.resize(limit);
    }

    return result;
}

bool extract_network_packet(
    int datalink,
    const unsigned char* packet,
    std::uint32_t captured_length,
    std::uint16_t& ether_type,
    const unsigned char*& network_packet,
    std::uint32_t& network_length,
    PcapProtocolCounters& protocols
) {
    if (!packet || captured_length == 0) {
        return false;
    }

    if (datalink == DLT_EN10MB) {
        if (captured_length < 14) {
            return false;
        }

        protocols.ethernet += 1;

        ether_type = read_u16_be(packet + 12);
        auto header_length = 14U;

        if (ether_type == kEtherTypeVlan && captured_length >= 18) {
            ether_type = read_u16_be(packet + 16);
            header_length = 18U;
        }

        network_packet = packet + header_length;
        network_length = captured_length - header_length;
        return true;
    }

#ifdef DLT_LINUX_SLL
    if (datalink == DLT_LINUX_SLL) {
        if (captured_length < 16) {
            return false;
        }

        ether_type = read_u16_be(packet + 14);
        network_packet = packet + 16;
        network_length = captured_length - 16;
        return true;
    }
#endif

#ifdef DLT_LINUX_SLL2
    if (datalink == DLT_LINUX_SLL2) {
        if (captured_length < 20) {
            return false;
        }

        ether_type = read_u16_be(packet);
        network_packet = packet + 20;
        network_length = captured_length - 20;
        return true;
    }
#endif

#ifdef DLT_RAW
    if (datalink == DLT_RAW) {
        const auto version = static_cast<std::uint8_t>(packet[0] >> 4U);

        if (version == 4) {
            ether_type = kEtherTypeIpv4;
        } else if (version == 6) {
            ether_type = kEtherTypeIpv6;
        } else {
            return false;
        }

        network_packet = packet;
        network_length = captured_length;
        return true;
    }
#endif

    return false;
}

void analyze_transport_ports(
    const unsigned char* transport_packet,
    std::uint32_t transport_length,
    std::uint8_t protocol,
    std::uint64_t packet_bytes,
    const std::string& source_ip,
    const std::string& destination_ip,
    PcapAnalysisResult& result,
    CounterMap& source_ports,
    CounterMap& destination_ports,
    ServiceMap& destination_services,
    ConversationMap& conversations
) {
    if (transport_length < 4) {
        return;
    }

    if (protocol != kProtocolTcp && protocol != kProtocolUdp) {
        return;
    }

    const auto source_port = read_u16_be(transport_packet);
    const auto destination_port = read_u16_be(transport_packet + 2);
    const auto protocol_name = protocol == kProtocolTcp ? std::string("tcp") : std::string("udp");

    increment_counter(
        source_ports,
        protocol_name + "/" + std::to_string(source_port),
        packet_bytes
    );

    increment_counter(
        destination_ports,
        protocol_name + "/" + std::to_string(destination_port),
        packet_bytes
    );

    increment_service(destination_services, protocol_name, destination_port, packet_bytes);

    const auto destination_service_name = service_name_for(protocol_name, destination_port);
    const auto source_service_name = service_name_for(protocol_name, source_port);
    const bool use_source_service =
        destination_service_name == "Unknown" && source_service_name != "Unknown";

    const auto service_port = use_source_service ? source_port : destination_port;
    const auto service_name = use_source_service ? source_service_name : destination_service_name;

    increment_conversation(
        conversations,
        source_ip,
        destination_ip,
        protocol_name,
        service_port,
        service_name,
        packet_bytes
    );

    if (source_port == 53 || destination_port == 53) {
        result.summary.dns_traffic_detected = true;
    }
}

void analyze_ipv4_packet(
    const unsigned char* packet,
    std::uint32_t length,
    std::uint64_t packet_bytes,
    PcapAnalysisResult& result,
    CounterMap& source_ips,
    CounterMap& destination_ips,
    CounterMap& source_ports,
    CounterMap& destination_ports,
    CounterMap& protocol_distribution,
    ServiceMap& destination_services,
    ConversationMap& conversations
) {
    if (length < 20) {
        return;
    }

    const auto version = static_cast<std::uint8_t>(packet[0] >> 4U);
    const auto ihl = static_cast<std::uint8_t>((packet[0] & 0x0FU) * 4U);

    if (version != 4 || ihl < 20 || length < ihl) {
        return;
    }

    result.protocols.ipv4 += 1;

    const auto protocol = packet[9];
    const auto source_ip = ipv4_to_string(packet + 12);
    const auto destination_ip = ipv4_to_string(packet + 16);

    increment_counter(source_ips, source_ip, packet_bytes);
    increment_counter(destination_ips, destination_ip, packet_bytes);

    if (classify_ip_address(source_ip) == "public" ||
        classify_ip_address(destination_ip) == "public") {
        result.summary.external_traffic_detected = true;
    }

    if (protocol == kProtocolTcp) {
        result.protocols.tcp += 1;
        increment_counter(protocol_distribution, "TCP", packet_bytes);
    } else if (protocol == kProtocolUdp) {
        result.protocols.udp += 1;
        increment_counter(protocol_distribution, "UDP", packet_bytes);
    } else if (protocol == kProtocolIcmp) {
        result.protocols.icmp += 1;
        increment_counter(protocol_distribution, "ICMP", packet_bytes);
    } else {
        result.protocols.other_l4 += 1;
        increment_counter(protocol_distribution, "Other L4", packet_bytes);
    }

    analyze_transport_ports(
        packet + ihl,
        length - ihl,
        protocol,
        packet_bytes,
        source_ip,
        destination_ip,
        result,
        source_ports,
        destination_ports,
        destination_services,
        conversations
    );
}

void analyze_ipv6_packet(
    const unsigned char* packet,
    std::uint32_t length,
    std::uint64_t packet_bytes,
    PcapAnalysisResult& result,
    CounterMap& source_ips,
    CounterMap& destination_ips,
    CounterMap& source_ports,
    CounterMap& destination_ports,
    CounterMap& protocol_distribution,
    ServiceMap& destination_services,
    ConversationMap& conversations
) {
    if (length < 40) {
        return;
    }

    const auto version = static_cast<std::uint8_t>(packet[0] >> 4U);
    if (version != 6) {
        return;
    }

    result.protocols.ipv6 += 1;

    const auto next_header = packet[6];
    const auto source_ip = ipv6_to_string(packet + 8);
    const auto destination_ip = ipv6_to_string(packet + 24);

    increment_counter(source_ips, source_ip, packet_bytes);
    increment_counter(destination_ips, destination_ip, packet_bytes);

    if (classify_ip_address(source_ip) == "public" ||
        classify_ip_address(destination_ip) == "public") {
        result.summary.external_traffic_detected = true;
    }

    if (next_header == kProtocolTcp) {
        result.protocols.tcp += 1;
        increment_counter(protocol_distribution, "TCP", packet_bytes);
    } else if (next_header == kProtocolUdp) {
        result.protocols.udp += 1;
        increment_counter(protocol_distribution, "UDP", packet_bytes);
    } else if (next_header == kProtocolIcmpv6) {
        result.protocols.icmpv6 += 1;
        increment_counter(protocol_distribution, "ICMPv6", packet_bytes);
    } else {
        result.protocols.other_l4 += 1;
        increment_counter(protocol_distribution, "Other L4", packet_bytes);
    }

    analyze_transport_ports(
        packet + 40,
        length - 40,
        next_header,
        packet_bytes,
        source_ip,
        destination_ip,
        result,
        source_ports,
        destination_ports,
        destination_services,
        conversations
    );
}

}  // namespace

bool PcapAnalyzer::analyze(
    const ControllerStoredCaptureInfo& stored_capture,
    PcapAnalysisResult& result,
    std::string& error_message
) {
    if (!stored_capture.exists) {
        error_message = "Stored capture does not exist";
        return false;
    }

    char error_buffer[PCAP_ERRBUF_SIZE] = {};
    pcap_t* raw_handle = pcap_open_offline(stored_capture.local_path.c_str(), error_buffer);

    if (!raw_handle) {
        error_message = std::string("Failed to open PCAP file: ") + error_buffer;
        return false;
    }

    struct PcapHandleGuard {
        pcap_t* handle = nullptr;

        ~PcapHandleGuard() {
            if (handle) {
                pcap_close(handle);
            }
        }
    } handle_guard {raw_handle};

    result = {};
    result.agent_id = stored_capture.agent_id;
    result.capture_id = stored_capture.capture_id;
    result.local_path = stored_capture.local_path;
    result.file_size_bytes = stored_capture.file_size_bytes;
    result.analyzed_at = std::time(nullptr);

    const auto datalink = pcap_datalink(raw_handle);
    const auto* datalink_name = pcap_datalink_val_to_name(datalink);
    result.datalink_name = datalink_name ? datalink_name : "unknown";

    CounterMap source_ips;
    CounterMap destination_ips;
    CounterMap source_ports;
    CounterMap destination_ports;
    CounterMap protocol_distribution;
    ServiceMap destination_services;
    ConversationMap conversations;

    pcap_pkthdr* header = nullptr;
    const unsigned char* packet = nullptr;

    while (true) {
        const auto next_result = pcap_next_ex(raw_handle, &header, &packet);

        if (next_result == 1) {
            const auto packet_bytes = static_cast<std::uint64_t>(header->len);
            result.packet_count += 1;
            result.byte_count += packet_bytes;

            const auto packet_time = static_cast<std::time_t>(header->ts.tv_sec);
            if (result.first_packet_time == 0) {
                result.first_packet_time = packet_time;
            }
            result.last_packet_time = packet_time;

            std::uint16_t ether_type = 0;
            const unsigned char* network_packet = nullptr;
            std::uint32_t network_length = 0;

            if (!extract_network_packet(
                    datalink,
                    packet,
                    header->caplen,
                    ether_type,
                    network_packet,
                    network_length,
                    result.protocols
                )) {
                result.protocols.other_l3 += 1;
                increment_counter(protocol_distribution, "Other L3", packet_bytes);
                continue;
            }

            if (ether_type == kEtherTypeIpv4) {
                analyze_ipv4_packet(
                    network_packet,
                    network_length,
                    packet_bytes,
                    result,
                    source_ips,
                    destination_ips,
                    source_ports,
                    destination_ports,
                    protocol_distribution,
                    destination_services,
                    conversations
                );
            } else if (ether_type == kEtherTypeIpv6) {
                analyze_ipv6_packet(
                    network_packet,
                    network_length,
                    packet_bytes,
                    result,
                    source_ips,
                    destination_ips,
                    source_ports,
                    destination_ports,
                    protocol_distribution,
                    destination_services,
                    conversations
                );
            } else if (ether_type == kEtherTypeArp) {
                result.protocols.arp += 1;
                increment_counter(protocol_distribution, "ARP", packet_bytes);
            } else {
                result.protocols.other_l3 += 1;
                increment_counter(protocol_distribution, "Other L3", packet_bytes);
            }

            continue;
        }

        if (next_result == 0) {
            continue;
        }

        if (next_result == -2) {
            break;
        }

        error_message = std::string("Failed while reading PCAP: ") + pcap_geterr(raw_handle);
        return false;
    }

    if (result.first_packet_time > 0 && result.last_packet_time >= result.first_packet_time) {
        result.duration_seconds = static_cast<double>(
            result.last_packet_time - result.first_packet_time
        );
    }

    if (result.packet_count > 0) {
        result.summary.average_packet_size_bytes =
            static_cast<double>(result.byte_count) / static_cast<double>(result.packet_count);
    }

    result.protocol_distribution =
        make_protocol_distribution(protocol_distribution, result.packet_count);
    result.top_conversations = make_top_conversations(conversations, 10);
    result.top_destination_services = make_top_services(destination_services, 10);
    result.top_destination_ip_details = make_top_destination_ip_details(destination_ips, 10);

    result.top_source_ips = make_top_counters(source_ips, 10);
    result.top_destination_ips = make_top_counters(destination_ips, 10);
    result.top_source_ports = make_top_counters(source_ports, 10);
    result.top_destination_ports = make_top_counters(destination_ports, 10);

    if (!result.protocol_distribution.empty()) {
        result.summary.main_protocol = result.protocol_distribution.front().name;
    }

    result.summary.main_service = main_service_display_name(result.top_destination_services);

    return true;
}

}  // namespace controller
