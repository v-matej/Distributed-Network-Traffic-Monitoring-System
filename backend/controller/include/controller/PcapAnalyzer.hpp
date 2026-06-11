#pragma once

#include "controller/ControllerTypes.hpp"

#include <string>

namespace controller {

class PcapAnalyzer {
public:
    static bool analyze(
        const ControllerStoredCaptureInfo& stored_capture,
        PcapAnalysisResult& result,
        std::string& error_message
    );
};

}  // namespace controller