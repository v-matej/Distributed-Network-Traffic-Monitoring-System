#pragma once

#include <string>

struct Config {
    std::string interface_name;
    std::string output_file = "capture.pcap";
    std::string filter_expression;
    int packet_count = 0;
    int duration_seconds = 0;
    bool list_interfaces = false;
    bool live_output = false;
    bool show_help = false;
};

bool parse_arguments(int argc, char* argv[], Config& config, std::string& error_message);
void print_help(const char* program_name);