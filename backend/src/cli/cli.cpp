#include "cli.hpp"

#include <iostream>
#include <string>

bool parse_arguments(int argc, char* argv[], CliOptions& options, std::string& error_message) {
    for (int i = 1; i < argc; ++i) {
        const std::string arg = argv[i];

        if (arg == "--list") {
            options.list_interfaces = true;
        } else if (arg == "-i") {
            if (i + 1 >= argc) {
                error_message = "Missing value for -i";
                return false;
            }
            options.capture_config.interface_name = argv[++i];
        } else if (arg == "-w") {
            if (i + 1 >= argc) {
                error_message = "Missing value for -w";
                return false;
            }
            options.capture_config.output_file = argv[++i];
        } else if (arg == "-f") {
            if (i + 1 >= argc) {
                error_message = "Missing value for -f";
                return false;
            }
            options.capture_config.filter_expression = argv[++i];
        } else if (arg == "-c") {
            if (i + 1 >= argc) {
                error_message = "Missing value for -c";
                return false;
            }

            try {
                options.capture_config.packet_count = std::stoi(argv[++i]);
            } catch (...) {
                error_message = "Invalid integer value for -c";
                return false;
            }

            if (options.capture_config.packet_count <= 0) {
                error_message = "Packet count for -c must be greater than 0";
                return false;
            }
        } else if (arg == "-t") {
            if (i + 1 >= argc) {
                error_message = "Missing value for -t";
                return false;
            }

            try {
                options.capture_config.duration_seconds = std::stoi(argv[++i]);
            } catch (...) {
                error_message = "Invalid integer value for -t";
                return false;
            }

            if (options.capture_config.duration_seconds <= 0) {
                error_message = "Duration for -t must be greater than 0";
                return false;
            }
        } else if (arg == "-l") {
            options.capture_config.live_output = true;
        } else if (arg == "-h" || arg == "--help") {
            options.show_help = true;
        } else {
            error_message = "Unknown argument: " + arg;
            return false;
        }
    }

    return true;
}

void print_help(const char* program_name) {
    std::cout
        << "Usage: " << program_name << " [options]\n\n"
        << "Options:\n"
        << "  --list            List available network interfaces\n"
        << "  -i <interface>    Network interface for capture\n"
        << "  -w <file>         Output PCAP file (default: capture.pcap)\n"
        << "  -f <filter>       BPF filter expression\n"
        << "  -c <count>        Number of packets to capture\n"
        << "  -t <seconds>      Capture duration in seconds\n"
        << "  -l                Print live packet info (debug)\n"
        << "  -h, --help        Show this help message\n";
}