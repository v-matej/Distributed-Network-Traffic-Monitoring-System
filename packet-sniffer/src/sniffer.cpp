#include "sniffer.hpp"

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

namespace {
struct CaptureContext {
    pcap_dumper_t* dumper = nullptr;
    bool live_output = false;
};

enum class StopReason {
    None,
    UserSignal,
    TimeLimit,
    PacketLimit
};

static pcap_t* global_handle = nullptr;
static std::atomic<StopReason> stop_reason {StopReason::None};
static std::atomic<bool> capture_finished {false};

static std::mutex timer_mutex;
static std::condition_variable timer_cv;

void signal_handler(int) {
    stop_reason = StopReason::UserSignal;

    if (global_handle != nullptr) {
        pcap_breakloop(global_handle);
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

void list_interfaces() {
    char errbuf[PCAP_ERRBUF_SIZE] {};
    pcap_if_t* devices = nullptr;

    if (pcap_findalldevs(&devices, errbuf) == -1) {
        std::cerr << "Error finding devices: " << errbuf << '\n';
        return;
    }

    std::cout << "Available network interfaces:\n";

    for (pcap_if_t* device = devices; device != nullptr; device = device->next) {
        std::cout << " - " << device->name;

        if (device->description != nullptr) {
            std::cout << " (" << device->description << ")";
        }

        std::cout << '\n';
    }

    pcap_freealldevs(devices);
}

bool run_capture(const Config& config, std::string& error_message) {
    if (config.interface_name.empty()) {
        error_message = "No network interface specified. Use -i <interface>.";
        return false;
    }

    stop_reason = StopReason::None;
    capture_finished = false;

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
        error_message = std::string("Failed to open interface: ") + errbuf;
        return false;
    }

    std::unique_ptr<pcap_t, decltype(&pcap_close)> handle(raw_handle, &pcap_close);

    if (!config.filter_expression.empty()) {
        bpf_program filter_program {};
        constexpr bpf_u_int32 netmask = PCAP_NETMASK_UNKNOWN;

        if (pcap_compile(handle.get(), &filter_program, config.filter_expression.c_str(), 1, netmask) == -1) {
            error_message = std::string("Failed to compile filter: ") + pcap_geterr(handle.get());
            return false;
        }

        const int setfilter_result = pcap_setfilter(handle.get(), &filter_program);
        pcap_freecode(&filter_program);

        if (setfilter_result == -1) {
            error_message = std::string("Failed to apply filter: ") + pcap_geterr(handle.get());
            return false;
        }
    }

    pcap_dumper_t* raw_dumper = pcap_dump_open(handle.get(), config.output_file.c_str());
    if (raw_dumper == nullptr) {
        error_message = std::string("Failed to open output PCAP file: ") + pcap_geterr(handle.get());
        return false;
    }

    std::unique_ptr<pcap_dumper_t, decltype(&pcap_dump_close)> dumper(raw_dumper, &pcap_dump_close);

    CaptureContext context;
    context.dumper = dumper.get();
    context.live_output = config.live_output;

    global_handle = handle.get();
    std::signal(SIGINT, signal_handler);

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

    std::thread timer_thread;
    if (config.duration_seconds > 0) {
        timer_thread = std::thread([duration = config.duration_seconds]() {
            std::unique_lock<std::mutex> lock(timer_mutex);

            const bool finished_early = timer_cv.wait_for(
                lock,
                std::chrono::seconds(duration),
                [] { return capture_finished.load(); }
            );

            if (!finished_early && !capture_finished.load()) {
                stop_reason = StopReason::TimeLimit;

                if (global_handle != nullptr) {
                    pcap_breakloop(global_handle);
                }
            }
        });
    }

    const int loop_count = (config.packet_count > 0) ? config.packet_count : -1;

    const int result = pcap_loop(
        handle.get(),
        loop_count,
        packet_handler,
        reinterpret_cast<unsigned char*>(&context)
    );

    pcap_dump_flush(dumper.get());

    capture_finished = true;
    timer_cv.notify_all();

    global_handle = nullptr;
    std::signal(SIGINT, SIG_DFL);

    if (timer_thread.joinable()) {
        timer_thread.join();
    }

    if (result == -1) {
        error_message = std::string("Capture error: ") + pcap_geterr(handle.get());
        return false;
    }

    if (result >= 0 && config.packet_count > 0 && stop_reason.load() == StopReason::None) {
        stop_reason = StopReason::PacketLimit;
    }

    switch (stop_reason.load()) {
        case StopReason::UserSignal:
            std::cout << "Capture terminated by user.\n";
            break;
        case StopReason::TimeLimit:
            std::cout << "Capture finished: time limit reached.\n";
            break;
        case StopReason::PacketLimit:
            std::cout << "Capture finished: packet count limit reached.\n";
            break;
        case StopReason::None:
            break;
    }

    std::cout << "Capture finished successfully.\n";
    return true;
}