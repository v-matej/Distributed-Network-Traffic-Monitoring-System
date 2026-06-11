#include "controller/PcapPacketInspector.hpp"

#include <pcap/pcap.h>

#include <arpa/inet.h>

#include <algorithm>
#include <array>
#include <cctype>
#include <cstdint>
#include <iomanip>
#include <sstream>
#include <string>
#include <utility>
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

struct NetworkSlice {
    std::uint16_t ether_type = 0;
    const unsigned char* data = nullptr;
    std::uint32_t length = 0;
    std::uint32_t link_header_length = 0;
};

struct PcapHandleGuard {
    pcap_t* handle = nullptr;

    ~PcapHandleGuard() {
        if (handle) {
            pcap_close(handle);
        }
    }
};

std::uint16_t read_u16_be(const unsigned char* data) {
    return static_cast<std::uint16_t>(
        (static_cast<std::uint16_t>(data[0]) << 8U) |
        static_cast<std::uint16_t>(data[1])
    );
}

std::uint32_t read_u32_be(const unsigned char* data) {
    return (static_cast<std::uint32_t>(data[0]) << 24U) |
           (static_cast<std::uint32_t>(data[1]) << 16U) |
           (static_cast<std::uint32_t>(data[2]) << 8U) |
           static_cast<std::uint32_t>(data[3]);
}

std::string to_hex_u16(std::uint16_t value) {
    std::ostringstream stream;
    stream << "0x" << std::hex << std::uppercase << std::setw(4)
           << std::setfill('0') << value;
    return stream.str();
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

std::string mac_to_string(const unsigned char* data) {
    std::ostringstream stream;

    for (int index = 0; index < 6; ++index) {
        if (index > 0) {
            stream << ':';
        }

        stream << std::hex << std::setw(2) << std::setfill('0')
               << static_cast<int>(data[index]);
    }

    return stream.str();
}

std::string protocol_name(std::uint8_t protocol) {
    switch (protocol) {
        case kProtocolIcmp:
            return "ICMP";
        case kProtocolTcp:
            return "TCP";
        case kProtocolUdp:
            return "UDP";
        case kProtocolIcmpv6:
            return "ICMPv6";
        default:
            return "IP";
    }
}

std::string tcp_flags_to_string(std::uint8_t flags) {
    std::vector<std::string> names;

    if ((flags & 0x01U) != 0U) {
        names.push_back("FIN");
    }
    if ((flags & 0x02U) != 0U) {
        names.push_back("SYN");
    }
    if ((flags & 0x04U) != 0U) {
        names.push_back("RST");
    }
    if ((flags & 0x08U) != 0U) {
        names.push_back("PSH");
    }
    if ((flags & 0x10U) != 0U) {
        names.push_back("ACK");
    }
    if ((flags & 0x20U) != 0U) {
        names.push_back("URG");
    }
    if ((flags & 0x40U) != 0U) {
        names.push_back("ECE");
    }
    if ((flags & 0x80U) != 0U) {
        names.push_back("CWR");
    }

    if (names.empty()) {
        return "none";
    }

    std::ostringstream stream;
    for (std::size_t index = 0; index < names.size(); ++index) {
        if (index > 0) {
            stream << ", ";
        }
        stream << names[index];
    }

    return stream.str();
}

PcapPacketLayer make_layer(const std::string& name) {
    PcapPacketLayer layer;
    layer.name = name;
    return layer;
}

void add_field(PcapPacketLayer& layer, const std::string& name, const std::string& value) {
    layer.fields.push_back({name, value});
}

bool extract_network_slice(
    int datalink,
    const unsigned char* packet,
    std::uint32_t captured_length,
    NetworkSlice& slice,
    std::vector<PcapPacketLayer>* layers
) {
    if (!packet || captured_length == 0) {
        return false;
    }

    if (datalink == DLT_EN10MB) {
        if (captured_length < 14) {
            return false;
        }

        slice.ether_type = read_u16_be(packet + 12);
        slice.link_header_length = 14;

        if (layers) {
            auto layer = make_layer("Ethernet II");
            add_field(layer, "Destination", mac_to_string(packet));
            add_field(layer, "Source", mac_to_string(packet + 6));
            add_field(layer, "Type", to_hex_u16(slice.ether_type));
            layers->push_back(std::move(layer));
        }

        if (slice.ether_type == kEtherTypeVlan && captured_length >= 18) {
            slice.ether_type = read_u16_be(packet + 16);
            slice.link_header_length = 18;

            if (layers) {
                auto layer = make_layer("802.1Q Virtual LAN");
                add_field(layer, "Encapsulated type", to_hex_u16(slice.ether_type));
                layers->push_back(std::move(layer));
            }
        }

        slice.data = packet + slice.link_header_length;
        slice.length = captured_length - slice.link_header_length;
        return true;
    }

#ifdef DLT_LINUX_SLL
    if (datalink == DLT_LINUX_SLL) {
        if (captured_length < 16) {
            return false;
        }

        slice.ether_type = read_u16_be(packet + 14);
        slice.data = packet + 16;
        slice.length = captured_length - 16;
        slice.link_header_length = 16;

        if (layers) {
            auto layer = make_layer("Linux cooked capture");
            add_field(layer, "Protocol", to_hex_u16(slice.ether_type));
            layers->push_back(std::move(layer));
        }

        return true;
    }
#endif

#ifdef DLT_LINUX_SLL2
    if (datalink == DLT_LINUX_SLL2) {
        if (captured_length < 20) {
            return false;
        }

        slice.ether_type = read_u16_be(packet);
        slice.data = packet + 20;
        slice.length = captured_length - 20;
        slice.link_header_length = 20;

        if (layers) {
            auto layer = make_layer("Linux cooked capture v2");
            add_field(layer, "Protocol", to_hex_u16(slice.ether_type));
            layers->push_back(std::move(layer));
        }

        return true;
    }
#endif

#ifdef DLT_RAW
    if (datalink == DLT_RAW) {
        const auto version = static_cast<std::uint8_t>(packet[0] >> 4U);

        if (version == 4) {
            slice.ether_type = kEtherTypeIpv4;
        } else if (version == 6) {
            slice.ether_type = kEtherTypeIpv6;
        } else {
            return false;
        }

        slice.data = packet;
        slice.length = captured_length;
        slice.link_header_length = 0;
        return true;
    }
#endif

    return false;
}

void decode_tcp(
    const unsigned char* data,
    std::uint32_t length,
    PcapPacketSummary& summary,
    std::vector<PcapPacketLayer>* layers
) {
    summary.protocol = "TCP";

    if (length < 20) {
        summary.info = "Malformed TCP segment";
        return;
    }

    const auto source_port = read_u16_be(data);
    const auto destination_port = read_u16_be(data + 2);
    const auto sequence_number = read_u32_be(data + 4);
    const auto acknowledgement_number = read_u32_be(data + 8);
    const auto header_length = static_cast<std::uint8_t>((data[12] >> 4U) * 4U);
    const auto flags = data[13];
    const auto window = read_u16_be(data + 14);

    const auto payload_length =
        length >= header_length ? static_cast<std::uint32_t>(length - header_length) : 0U;

    std::ostringstream info;
    info << source_port << " -> " << destination_port
         << " [" << tcp_flags_to_string(flags) << "]"
         << " Seq=" << sequence_number
         << " Ack=" << acknowledgement_number
         << " Win=" << window
         << " Len=" << payload_length;
    summary.info = info.str();

    if (layers) {
        auto layer = make_layer("Transmission Control Protocol");
        add_field(layer, "Source Port", std::to_string(source_port));
        add_field(layer, "Destination Port", std::to_string(destination_port));
        add_field(layer, "Sequence Number", std::to_string(sequence_number));
        add_field(layer, "Acknowledgment Number", std::to_string(acknowledgement_number));
        add_field(layer, "Header Length", std::to_string(header_length) + " bytes");
        add_field(layer, "Flags", tcp_flags_to_string(flags));
        add_field(layer, "Window", std::to_string(window));
        add_field(layer, "Payload Length", std::to_string(payload_length));
        layers->push_back(std::move(layer));
    }
}

void decode_udp(
    const unsigned char* data,
    std::uint32_t length,
    PcapPacketSummary& summary,
    std::vector<PcapPacketLayer>* layers
) {
    summary.protocol = "UDP";

    if (length < 8) {
        summary.info = "Malformed UDP datagram";
        return;
    }

    const auto source_port = read_u16_be(data);
    const auto destination_port = read_u16_be(data + 2);
    const auto udp_length = read_u16_be(data + 4);

    std::ostringstream info;
    info << source_port << " -> " << destination_port << " Len=" << udp_length;
    summary.info = info.str();

    if (layers) {
        auto layer = make_layer("User Datagram Protocol");
        add_field(layer, "Source Port", std::to_string(source_port));
        add_field(layer, "Destination Port", std::to_string(destination_port));
        add_field(layer, "Length", std::to_string(udp_length));
        layers->push_back(std::move(layer));
    }
}

void decode_icmp(
    const unsigned char* data,
    std::uint32_t length,
    bool ipv6,
    PcapPacketSummary& summary,
    std::vector<PcapPacketLayer>* layers
) {
    summary.protocol = ipv6 ? "ICMPv6" : "ICMP";

    if (length < 4) {
        summary.info = "Malformed ICMP packet";
        return;
    }

    const auto type = data[0];
    const auto code = data[1];

    std::ostringstream info;
    info << "Type=" << static_cast<int>(type)
         << " Code=" << static_cast<int>(code);
    summary.info = info.str();

    if (layers) {
        auto layer = make_layer(
            ipv6 ? "Internet Control Message Protocol v6"
                 : "Internet Control Message Protocol"
        );
        add_field(layer, "Type", std::to_string(type));
        add_field(layer, "Code", std::to_string(code));
        layers->push_back(std::move(layer));
    }
}

void decode_ipv4(
    const unsigned char* data,
    std::uint32_t length,
    PcapPacketSummary& summary,
    std::vector<PcapPacketLayer>* layers
) {
    summary.protocol = "IPv4";

    if (length < 20) {
        summary.info = "Malformed IPv4 packet";
        return;
    }

    const auto version = static_cast<std::uint8_t>(data[0] >> 4U);
    const auto header_length = static_cast<std::uint8_t>((data[0] & 0x0FU) * 4U);

    if (version != 4 || header_length < 20 || length < header_length) {
        summary.info = "Invalid IPv4 header";
        return;
    }

    const auto total_length = read_u16_be(data + 2);
    const auto bounded_length = std::min<std::uint32_t>(length, total_length);
    const auto ttl = data[8];
    const auto protocol = data[9];

    summary.source = ipv4_to_string(data + 12);
    summary.destination = ipv4_to_string(data + 16);

    if (layers) {
        auto layer = make_layer("Internet Protocol Version 4");
        add_field(layer, "Version", std::to_string(version));
        add_field(layer, "Header Length", std::to_string(header_length) + " bytes");
        add_field(layer, "Total Length", std::to_string(total_length));
        add_field(layer, "Time to Live", std::to_string(ttl));
        add_field(layer, "Protocol", protocol_name(protocol));
        add_field(layer, "Source Address", summary.source);
        add_field(layer, "Destination Address", summary.destination);
        layers->push_back(std::move(layer));
    }

    if (bounded_length < header_length) {
        summary.info = "Invalid IPv4 total length";
        return;
    }

    const auto* transport_data = data + header_length;
    const auto transport_length = bounded_length - header_length;

    if (protocol == kProtocolTcp) {
        decode_tcp(transport_data, transport_length, summary, layers);
    } else if (protocol == kProtocolUdp) {
        decode_udp(transport_data, transport_length, summary, layers);
    } else if (protocol == kProtocolIcmp) {
        decode_icmp(transport_data, transport_length, false, summary, layers);
    } else {
        summary.info = "Protocol " + std::to_string(protocol);
    }
}

void decode_ipv6(
    const unsigned char* data,
    std::uint32_t length,
    PcapPacketSummary& summary,
    std::vector<PcapPacketLayer>* layers
) {
    summary.protocol = "IPv6";

    if (length < 40) {
        summary.info = "Malformed IPv6 packet";
        return;
    }

    const auto version = static_cast<std::uint8_t>(data[0] >> 4U);
    if (version != 6) {
        summary.info = "Invalid IPv6 header";
        return;
    }

    const auto payload_length = read_u16_be(data + 4);
    const auto bounded_length = std::min<std::uint32_t>(
        length,
        static_cast<std::uint32_t>(40U + payload_length)
    );

    const auto next_header = data[6];
    const auto hop_limit = data[7];

    summary.source = ipv6_to_string(data + 8);
    summary.destination = ipv6_to_string(data + 24);

    if (layers) {
        auto layer = make_layer("Internet Protocol Version 6");
        add_field(layer, "Version", std::to_string(version));
        add_field(layer, "Payload Length", std::to_string(payload_length));
        add_field(layer, "Next Header", protocol_name(next_header));
        add_field(layer, "Hop Limit", std::to_string(hop_limit));
        add_field(layer, "Source Address", summary.source);
        add_field(layer, "Destination Address", summary.destination);
        layers->push_back(std::move(layer));
    }

    if (bounded_length < 40) {
        summary.info = "Invalid IPv6 payload length";
        return;
    }

    const auto* transport_data = data + 40;
    const auto transport_length = bounded_length - 40;

    if (next_header == kProtocolTcp) {
        decode_tcp(transport_data, transport_length, summary, layers);
    } else if (next_header == kProtocolUdp) {
        decode_udp(transport_data, transport_length, summary, layers);
    } else if (next_header == kProtocolIcmpv6) {
        decode_icmp(transport_data, transport_length, true, summary, layers);
    } else {
        summary.info = "Next header " + std::to_string(next_header);
    }
}

void decode_arp(
    const unsigned char* data,
    std::uint32_t length,
    PcapPacketSummary& summary,
    std::vector<PcapPacketLayer>* layers
) {
    summary.protocol = "ARP";

    if (length < 28) {
        summary.info = "Malformed ARP packet";
        return;
    }

    const auto opcode = read_u16_be(data + 6);
    const auto sender_mac = mac_to_string(data + 8);
    const auto sender_ip = ipv4_to_string(data + 14);
    const auto target_mac = mac_to_string(data + 18);
    const auto target_ip = ipv4_to_string(data + 24);

    summary.source = sender_ip;
    summary.destination = target_ip;

    if (opcode == 1) {
        summary.info = "Who has " + target_ip + "? Tell " + sender_ip;
    } else if (opcode == 2) {
        summary.info = sender_ip + " is at " + sender_mac;
    } else {
        summary.info = "Opcode " + std::to_string(opcode);
    }

    if (layers) {
        auto layer = make_layer("Address Resolution Protocol");
        add_field(layer, "Opcode", std::to_string(opcode));
        add_field(layer, "Sender MAC", sender_mac);
        add_field(layer, "Sender IP", sender_ip);
        add_field(layer, "Target MAC", target_mac);
        add_field(layer, "Target IP", target_ip);
        layers->push_back(std::move(layer));
    }
}

PcapPacketSummary decode_packet(
    int datalink,
    const pcap_pkthdr& header,
    const unsigned char* packet,
    std::uint64_t number,
    std::time_t first_time,
    int first_microseconds,
    std::vector<PcapPacketLayer>* layers
) {
    PcapPacketSummary summary;
    summary.number = number;
    summary.timestamp = static_cast<std::time_t>(header.ts.tv_sec);
    summary.timestamp_microseconds = static_cast<int>(header.ts.tv_usec);
    summary.length = header.len;
    summary.captured_length = header.caplen;
    summary.source = "-";
    summary.destination = "-";
    summary.protocol = "Unknown";
    summary.info = "Unsupported or malformed packet";

    summary.relative_time_seconds =
        static_cast<double>(summary.timestamp - first_time) +
        static_cast<double>(summary.timestamp_microseconds - first_microseconds) / 1000000.0;

    if (layers) {
        auto frame = make_layer("Frame");
        add_field(frame, "Frame Number", std::to_string(summary.number));
        add_field(frame, "Captured Length", std::to_string(summary.captured_length) + " bytes");
        add_field(frame, "Original Length", std::to_string(summary.length) + " bytes");
        add_field(frame, "Epoch Time", std::to_string(summary.timestamp));
        add_field(frame, "Relative Time", std::to_string(summary.relative_time_seconds) + " seconds");
        layers->push_back(std::move(frame));
    }

    NetworkSlice slice;
    if (!extract_network_slice(datalink, packet, header.caplen, slice, layers)) {
        return summary;
    }

    if (slice.ether_type == kEtherTypeIpv4) {
        decode_ipv4(slice.data, slice.length, summary, layers);
    } else if (slice.ether_type == kEtherTypeIpv6) {
        decode_ipv6(slice.data, slice.length, summary, layers);
    } else if (slice.ether_type == kEtherTypeArp) {
        decode_arp(slice.data, slice.length, summary, layers);
    } else {
        summary.protocol = "L2";
        summary.info = "EtherType " + to_hex_u16(slice.ether_type);
    }

    return summary;
}

std::vector<PcapHexLine> make_hex_lines(
    const unsigned char* packet,
    std::uint32_t captured_length
) {
    std::vector<PcapHexLine> lines;

    constexpr std::uint32_t bytes_per_line = 16;

    for (std::uint32_t offset = 0; offset < captured_length; offset += bytes_per_line) {
        const auto line_length = std::min(bytes_per_line, captured_length - offset);

        std::ostringstream hex;
        std::ostringstream ascii;

        for (std::uint32_t index = 0; index < bytes_per_line; ++index) {
            if (index < line_length) {
                const auto value = packet[offset + index];

                hex << std::hex << std::setw(2) << std::setfill('0')
                    << static_cast<int>(value);

                const auto printable =
                    std::isprint(static_cast<unsigned char>(value)) != 0;
                ascii << (printable ? static_cast<char>(value) : '.');
            } else {
                hex << "  ";
                ascii << ' ';
            }

            if (index != bytes_per_line - 1) {
                hex << ' ';
            }
        }

        PcapHexLine line;
        line.offset = offset;
        line.hex = hex.str();
        line.ascii = ascii.str();
        lines.push_back(std::move(line));
    }

    return lines;
}

bool open_pcap(
    const ControllerStoredCaptureInfo& stored_capture,
    pcap_t*& handle,
    std::string& error_message
) {
    if (!stored_capture.exists) {
        error_message = "Stored capture does not exist";
        return false;
    }

    char error_buffer[PCAP_ERRBUF_SIZE] = {};
    handle = pcap_open_offline(stored_capture.local_path.c_str(), error_buffer);

    if (!handle) {
        error_message = std::string("Failed to open PCAP file: ") + error_buffer;
        return false;
    }

    return true;
}

}  // namespace

bool PcapPacketInspector::list_packets(
    const ControllerStoredCaptureInfo& stored_capture,
    std::size_t offset,
    std::size_t limit,
    PcapPacketList& packet_list,
    std::string& error_message
) {
    pcap_t* raw_handle = nullptr;
    if (!open_pcap(stored_capture, raw_handle, error_message)) {
        return false;
    }

    PcapHandleGuard guard {raw_handle};

    packet_list = {};
    packet_list.agent_id = stored_capture.agent_id;
    packet_list.capture_id = stored_capture.capture_id;
    packet_list.local_path = stored_capture.local_path;
    packet_list.offset = offset;
    packet_list.limit = limit;

    const auto datalink = pcap_datalink(raw_handle);
    const auto* datalink_name = pcap_datalink_val_to_name(datalink);
    packet_list.datalink_name = datalink_name ? datalink_name : "unknown";

    pcap_pkthdr* header = nullptr;
    const unsigned char* packet = nullptr;

    std::time_t first_time = 0;
    int first_microseconds = 0;
    std::uint64_t number = 0;

    while (true) {
        const auto next_result = pcap_next_ex(raw_handle, &header, &packet);

        if (next_result == 1) {
            number += 1;
            packet_list.total_packets = number;

            if (first_time == 0) {
                first_time = static_cast<std::time_t>(header->ts.tv_sec);
                first_microseconds = static_cast<int>(header->ts.tv_usec);
            }

            const auto zero_based_index = number - 1;
            if (zero_based_index >= offset && packet_list.packets.size() < limit) {
                packet_list.packets.push_back(
                    decode_packet(
                        datalink,
                        *header,
                        packet,
                        number,
                        first_time,
                        first_microseconds,
                        nullptr
                    )
                );
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

    return true;
}

bool PcapPacketInspector::get_packet_detail(
    const ControllerStoredCaptureInfo& stored_capture,
    std::uint64_t packet_number,
    PcapPacketDetail& packet_detail,
    std::string& error_message
) {
    if (packet_number == 0) {
        error_message = "Packet number must be greater than zero";
        return false;
    }

    pcap_t* raw_handle = nullptr;
    if (!open_pcap(stored_capture, raw_handle, error_message)) {
        return false;
    }

    PcapHandleGuard guard {raw_handle};

    const auto datalink = pcap_datalink(raw_handle);

    pcap_pkthdr* header = nullptr;
    const unsigned char* packet = nullptr;

    std::time_t first_time = 0;
    int first_microseconds = 0;
    std::uint64_t number = 0;

    while (true) {
        const auto next_result = pcap_next_ex(raw_handle, &header, &packet);

        if (next_result == 1) {
            number += 1;

            if (first_time == 0) {
                first_time = static_cast<std::time_t>(header->ts.tv_sec);
                first_microseconds = static_cast<int>(header->ts.tv_usec);
            }

            if (number == packet_number) {
                packet_detail = {};
                packet_detail.summary = decode_packet(
                    datalink,
                    *header,
                    packet,
                    number,
                    first_time,
                    first_microseconds,
                    &packet_detail.layers
                );

                packet_detail.hex_lines = make_hex_lines(packet, header->caplen);
                return true;
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

    error_message = "Packet not found";
    return false;
}

}  // namespace controller