#pragma once

#include "sniffer/capture_service.hpp"

#include <memory>

namespace sniffer {

class PcapCaptureService : public ICaptureService {
public:
    std::vector<InterfaceInfo> list_interfaces(std::string& error_message) override;
    CaptureResult run_capture(const CaptureConfig& config) override;
};

std::shared_ptr<ICaptureService> create_pcap_capture_service();

}  // namespace sniffer