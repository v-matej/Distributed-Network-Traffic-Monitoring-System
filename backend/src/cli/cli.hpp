#pragma once

#include "sniffer/types.hpp"

#include <string>

struct CliOptions {
    sniffer::CaptureConfig capture_config;
    bool list_interfaces = false;
    bool show_help = false;
};

bool parse_arguments(int argc, char* argv[], CliOptions& options, std::string& error_message);
void print_help(const char* program_name);