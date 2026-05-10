#include "sniffer/capture.hpp"

#include <pcap.h>

#include <atomic>
#include <condition_variable>
#include <csignal>
#include <ctime>
#include <iomanip>
#include <iostream>
#include <memory>
#include <mutex>
#include <sstream>
#include <string>
#include <thread>
#include <vector>

namespace {
struct CaptureContext {
    pcap_dumper_t* dumper = nullptr;
    bool live_output = false;
    std::uint64_t packets_captured = 0;
    std::uint64_t bytes_captured = 0;
};

static pcap_t* signal_handle = nullptr;
static std::atomic<bool> signal_stop_requested {false};

void signal_handler(int) {
    signal_stop_requested = true;

    if (signal_handle != nullptr) {
        pcap_breakloop(signal_handle);
    }
}

std::string format_timestamp(const timeval& ts) {
    std::time_t seconds = ts.tv_sec;
    std::tm local_time {};

    localtime_r(&seconds, &local_time);

    std::ostringstream oss;
    oss << std::put_time(&local_time, "%Y-%m-%d %H:%M:%S")
        << "." << std::setw(6) << std::setfill('0') << ts.tv_usec;

    return oss.str();
}

void packet_handler(unsigned char* user_data, const pcap_pkthdr* header, const unsigned char* packet_data) {
    auto* context = reinterpret_cast<CaptureContext*>(user_data);

    if (context == nullptr || header == nullptr || packet_data == nullptr) {
        return;
    }

    context->packets_captured++;
    context->bytes_captured += header->caplen;

    if (context->dumper != nullptr) {
        pcap_dump(reinterpret_cast<unsigned char*>(context->dumper), header, packet_data);
    }

    if (context->live_output) {
        std::cout
            << "[PACKET] "
            << "timestamp=" << format_timestamp(header->ts)
            << ", captured_length=" << header->caplen
            << ", original_length=" << header->len
            << '\n';
    }
}
}  // namespace

namespace sniffer {

std::vector<InterfaceInfo> PcapCaptureService::list_interfaces(std::string& error_message) {
    std::vector<InterfaceInfo> interfaces;

    char errbuf[PCAP_ERRBUF_SIZE] {};
    pcap_if_t* devices = nullptr;

    if (pcap_findalldevs(&devices, errbuf) == -1) {
        error_message = errbuf;
        return interfaces;
    }

    for (pcap_if_t* device = devices; device != nullptr; device = device->next) {
        InterfaceInfo info;
        info.name = device->name != nullptr ? device->name : "";
        info.description = device->description != nullptr ? device->description : "";
        interfaces.push_back(info);
    }

    pcap_freealldevs(devices);
    return interfaces;
}

CaptureResult PcapCaptureService::run_capture(const CaptureConfig& config) {
    CaptureResult result;
    result.interface_name = config.interface_name;
    result.output_file = config.output_file;
    result.filter_expression = config.filter_expression;
    result.start_time = std::time(nullptr);

    if (config.interface_name.empty()) {
        result.stop_reason = StopReason::Error;
        result.error_message = "No network interface specified.";
        result.end_time = std::time(nullptr);
        return result;
    }

    if (config.control && config.control->is_stop_requested()) {
        result.success = true;
        result.stop_reason = StopReason::ExternalStop;
        result.end_time = std::time(nullptr);
        return result;
    }

    std::atomic<StopReason> stop_reason {StopReason::None};
    std::atomic<bool> capture_finished {false};
    std::mutex timer_mutex;
    std::condition_variable timer_cv;

    signal_stop_requested = false;

    char errbuf[PCAP_ERRBUF_SIZE] {};
    constexpr int snapshot_length = 65535;
    constexpr int promiscuous_mode = 1;
    constexpr int read_timeout_ms = 1000;

    pcap_t* raw_handle = pcap_open_live(
        config.interface_name.c_str(),
        snapshot_length,
        promiscuous_mode,
        read_timeout_ms,
        errbuf
    );

    if (raw_handle == nullptr) {
        result.stop_reason = StopReason::Error;
        result.error_message = std::string("Failed to open interface: ") + errbuf;
        result.end_time = std::time(nullptr);
        return result;
    }

    std::unique_ptr<pcap_t, decltype(&pcap_close)> handle(raw_handle, &pcap_close);

    if (config.control) {
        config.control->set_stop_callback([raw_handle]() {
            pcap_breakloop(raw_handle);
        });
    }

    if (!config.filter_expression.empty()) {
        bpf_program filter_program {};
        constexpr bpf_u_int32 netmask = PCAP_NETMASK_UNKNOWN;

        if (pcap_compile(handle.get(), &filter_program, config.filter_expression.c_str(), 1, netmask) == -1) {
            if (config.control) {
                config.control->clear_stop_callback();
            }
            result.stop_reason = StopReason::Error;
            result.error_message = std::string("Failed to compile filter: ") + pcap_geterr(handle.get());
            result.end_time = std::time(nullptr);
            return result;
        }

        const int setfilter_result = pcap_setfilter(handle.get(), &filter_program);
        pcap_freecode(&filter_program);

        if (setfilter_result == -1) {
            if (config.control) {
                config.control->clear_stop_callback();
            }
            result.stop_reason = StopReason::Error;
            result.error_message = std::string("Failed to apply filter: ") + pcap_geterr(handle.get());
            result.end_time = std::time(nullptr);
            return result;
        }
    }

    pcap_dumper_t* raw_dumper = pcap_dump_open(handle.get(), config.output_file.c_str());
    if (raw_dumper == nullptr) {
        if (config.control) {
            config.control->clear_stop_callback();
        }
        result.stop_reason = StopReason::Error;
        result.error_message = std::string("Failed to open output PCAP file: ") + pcap_geterr(handle.get());
        result.end_time = std::time(nullptr);
        return result;
    }

    std::unique_ptr<pcap_dumper_t, decltype(&pcap_dump_close)> dumper(raw_dumper, &pcap_dump_close);

    CaptureContext context;
    context.dumper = dumper.get();
    context.live_output = config.live_output;

    if (config.enable_signal_stop) {
        signal_handle = handle.get();
        std::signal(SIGINT, signal_handler);
    }

    if (config.enable_console_output) {
        std::cout << "Starting capture on interface: " << config.interface_name << '\n';
        std::cout << "Output file: " << config.output_file << '\n';

        if (!config.filter_expression.empty()) {
            std::cout << "Filter: " << config.filter_expression << '\n';
        }

        if (config.packet_count > 0) {
            std::cout << "Packet count limit: " << config.packet_count << '\n';
        } else {
            std::cout << "Packet count limit: unlimited\n";
        }

        if (config.duration_seconds > 0) {
            std::cout << "Duration limit: " << config.duration_seconds << " second(s)\n";
        } else {
            std::cout << "Duration limit: unlimited\n";
        }
    }

    std::thread timer_thread;
    if (config.duration_seconds > 0) {
        timer_thread = std::thread([duration = config.duration_seconds, &capture_finished, &timer_cv, &timer_mutex, &stop_reason, raw_handle]() {
            std::unique_lock<std::mutex> lock(timer_mutex);

            const bool finished_early = timer_cv.wait_for(
                lock,
                std::chrono::seconds(duration),
                [&capture_finished] { return capture_finished.load(); }
            );

            if (!finished_early && !capture_finished.load()) {
                stop_reason = StopReason::TimeLimit;
                pcap_breakloop(raw_handle);
            }
        });
    }

    if (config.control && config.control->is_stop_requested()) {
        stop_reason = StopReason::ExternalStop;
        pcap_breakloop(handle.get());
    }

    const int loop_count = (config.packet_count > 0) ? config.packet_count : -1;

    const int loop_result = pcap_loop(
        handle.get(),
        loop_count,
        packet_handler,
        reinterpret_cast<unsigned char*>(&context)
    );

    pcap_dump_flush(dumper.get());

    capture_finished = true;
    timer_cv.notify_all();

    if (timer_thread.joinable()) {
        timer_thread.join();
    }

    if (config.control) {
        config.control->clear_stop_callback();
    }

    if (config.enable_signal_stop) {
        signal_handle = nullptr;
        std::signal(SIGINT, SIG_DFL);
    }

    result.packets_captured = context.packets_captured;
    result.bytes_captured = context.bytes_captured;
    result.end_time = std::time(nullptr);

    if (loop_result == -1) {
        result.stop_reason = StopReason::Error;
        result.error_message = std::string("Capture error: ") + pcap_geterr(handle.get());
        return result;
    }

    if (signal_stop_requested.load() && stop_reason.load() == StopReason::None) {
        stop_reason = StopReason::UserSignal;
    }

    if (config.control && config.control->is_stop_requested() && stop_reason.load() == StopReason::None) {
        stop_reason = StopReason::ExternalStop;
    }

    if (loop_result >= 0 && config.packet_count > 0 && stop_reason.load() == StopReason::None) {
        stop_reason = StopReason::PacketLimit;
    }

    result.stop_reason = stop_reason.load();
    result.success = true;

    if (config.enable_console_output) {
        switch (result.stop_reason) {
            case StopReason::UserSignal:
                std::cout << "Capture terminated by user.\n";
                break;
            case StopReason::TimeLimit:
                std::cout << "Capture finished: time limit reached.\n";
                break;
            case StopReason::PacketLimit:
                std::cout << "Capture finished: packet count limit reached.\n";
                break;
            case StopReason::ExternalStop:
                std::cout << "Capture finished: stop requested externally.\n";
                break;
            case StopReason::None:
                std::cout << "Capture finished.\n";
                break;
            case StopReason::Error:
                break;
        }

        std::cout << "Capture finished successfully.\n";
    }
    return result;
}

std::shared_ptr<ICaptureService> create_pcap_capture_service() {
    return std::make_shared<PcapCaptureService>();
}

}  // namespace sniffer
