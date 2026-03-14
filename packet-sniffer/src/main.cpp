#include "cli.hpp"
#include "sniffer.hpp"

#include <iostream>
#include <string>

int main(int argc, char* argv[]) {

    if (argc == 1) {
        print_help(argv[0]);
        return 0;
    }

    Config config;
    std::string error_message;

    if (!parse_arguments(argc, argv, config, error_message)) {
        std::cerr << "Argument parsing error: " << error_message << "\n\n";
        print_help(argv[0]);
        return 1;
    }

    if (config.show_help) {
        print_help(argv[0]);
        return 0;
    }

    if (config.list_interfaces) {
        list_interfaces();
        return 0;
    }

    if (config.interface_name.empty()) {
        std::cerr << "Error: network interface must be specified with -i <interface>\n\n";
        print_help(argv[0]);
        return 1;
    }

    if (!run_capture(config, error_message)) {
        std::cerr << "Capture failed: " << error_message << '\n';
        return 1;
    }

    return 0;
}