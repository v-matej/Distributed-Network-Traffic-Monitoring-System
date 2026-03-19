#include "cli.hpp"
#include "sniffer/capture.hpp"
#include "sniffer/types.hpp"

#include <iostream>
#include <string>

int main(int argc, char* argv[]) {
    if (argc == 1) {
        print_help(argv[0]);
        return 0;
    }

    CliOptions options;
    std::string error_message;

    if (!parse_arguments(argc, argv, options, error_message)) {
        std::cerr << "Argument parsing error: " << error_message << "\n\n";
        print_help(argv[0]);
        return 1;
    }

    if (options.show_help) {
        print_help(argv[0]);
        return 0;
    }

    if (options.list_interfaces) {
        const auto interfaces = sniffer::list_interfaces(error_message);

        if (!error_message.empty()) {
            std::cerr << "Failed to list interfaces: " << error_message << '\n';
            return 1;
        }

        std::cout << "Available network interfaces:\n";
        for (const auto& iface : interfaces) {
            std::cout << " - " << iface.name;
            if (!iface.description.empty()) {
                std::cout << " (" << iface.description << ")";
            }
            std::cout << '\n';
        }

        return 0;
    }

    if (options.capture_config.interface_name.empty()) {
        std::cerr << "Error: network interface must be specified with -i <interface>\n\n";
        print_help(argv[0]);
        return 1;
    }

    const sniffer::CaptureResult result = sniffer::run_capture(options.capture_config);

    if (!result.success) {
        std::cerr << "Capture failed: " << result.error_message << '\n';
        return 1;
    }

    std::cout << "\nCapture summary:\n";
    std::cout << "  Interface: " << result.interface_name << '\n';
    std::cout << "  Output file: " << result.output_file << '\n';
    std::cout << "  Filter: " << (result.filter_expression.empty() ? "<none>" : result.filter_expression) << '\n';
    std::cout << "  Packets captured: " << result.packets_captured << '\n';
    std::cout << "  Bytes captured: " << result.bytes_captured << '\n';
    std::cout << "  Stop reason: " << sniffer::to_string(result.stop_reason) << '\n';

    return 0;
}