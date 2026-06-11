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

using CounterMap = std::unordered_map<std::string, CounterValue>;

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
    CounterMap& source_ports,
    CounterMap& destination_ports
) {
    if (transport_length < 4) {
        return;
    }

    if (protocol != kProtocolTcp && protocol != kProtocolUdp) {
        return;
    }

    const auto source_port = read_u16_be(transport_packet);
    const auto destination_port = read_u16_be(transport_packet + 2);

    const auto protocol_name = protocol == kProtocolTcp ? "tcp" : "udp";

    increment_counter(
        source_ports,
        std::string(protocol_name) + "/" + std::to_string(source_port),
        packet_bytes
    );

    increment_counter(
        destination_ports,
        std::string(protocol_name) + "/" + std::to_string(destination_port),
        packet_bytes
    );
}

void analyze_ipv4_packet(
    const unsigned char* packet,
    std::uint32_t length,
    std::uint64_t packet_bytes,
    PcapAnalysisResult& result,
    CounterMap& source_ips,
    CounterMap& destination_ips,
    CounterMap& source_ports,
    CounterMap& destination_ports
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

    if (protocol == kProtocolTcp) {
        result.protocols.tcp += 1;
    } else if (protocol == kProtocolUdp) {
        result.protocols.udp += 1;
    } else if (protocol == kProtocolIcmp) {
        result.protocols.icmp += 1;
    } else {
        result.protocols.other_l4 += 1;
    }

    analyze_transport_ports(
        packet + ihl,
        length - ihl,
        protocol,
        packet_bytes,
        source_ports,
        destination_ports
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
    CounterMap& destination_ports
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

    if (next_header == kProtocolTcp) {
        result.protocols.tcp += 1;
    } else if (next_header == kProtocolUdp) {
        result.protocols.udp += 1;
    } else if (next_header == kProtocolIcmpv6) {
        result.protocols.icmpv6 += 1;
    } else {
        result.protocols.other_l4 += 1;
    }

    analyze_transport_ports(
        packet + 40,
        length - 40,
        next_header,
        packet_bytes,
        source_ports,
        destination_ports
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
                    destination_ports
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
                    destination_ports
                );
            } else if (ether_type == kEtherTypeArp) {
                result.protocols.arp += 1;
            } else {
                result.protocols.other_l3 += 1;
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

    result.top_source_ips = make_top_counters(source_ips, 10);
    result.top_destination_ips = make_top_counters(destination_ips, 10);
    result.top_source_ports = make_top_counters(source_ports, 10);
    result.top_destination_ports = make_top_counters(destination_ports, 10);

    return true;
}

}  // namespace controller