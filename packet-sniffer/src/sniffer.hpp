#pragma once

#include "cli.hpp"

#include <string>

void list_interfaces();

bool run_capture(const Config& config, std::string& error_message);