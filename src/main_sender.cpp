#include "udp_socket.h"
#include "sender_saw.h"
#include "sender_gbn.h"
#include "sender_sr.h"
#include "logger.h"
#include "fault_injection.h"
#include "sha256.h"
#include <iostream>
#include <string>
#include <memory>
#include <cstdlib>


void print_usage() {
    std::cout << "Usage: sender <host> <port> <file> [options]\n"
              << "Options:\n"
              << "  --mode saw|gbn|sr  (default: saw)\n"
              << "  --window N         (default: 1 for saw, 16 for gbn/sr)\n"
              << "  --timeout MS       (default: 200)\n"
              << "  --adaptive         (enable adaptive RTO / RFC 6298)\n"
              << "  --cc               (enable congestion control / TCP Reno)\n"
              << "  --loss PERCENT     (0-100, default: 0)\n"
              << "  --dup PERCENT      (0-100, default: 0)\n"
              << "  --corrupt PERCENT  (0-100, default: 0)\n"
              << "  --delay MS         (per-packet delay, default: 0)\n"
              << "  --seed N           (default: 0 = random)\n"
              << "  --verbose          (enable verbose logging)\n"
              << "  --csv              (print stats as CSV)\n"
              << "  --rto-log FILE     (write RTO samples to CSV file)\n"
              << "  --cwnd-log FILE    (write cwnd samples to CSV file)\n";
}

int main(int argc, char** argv) {
    if (argc < 4) {
        print_usage();
        return 1;
    }

    std::string host = argv[1];
    uint16_t port = std::stoi(argv[2]);
    std::string filepath = argv[3];

    std::string mode = "saw";
    uint32_t window = 0; // 0 means default
    uint32_t timeout = 200;
    double loss = 0, dup = 0, corrupt = 0;
    uint32_t delay_ms = 0;
    uint32_t seed = 0;
    bool verbose = false;
    bool csv = false;
    bool adaptive = false;
    bool cc = false;
    std::string rto_log_file;
    std::string cwnd_log_file;

    for (int i = 4; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "--mode" && i + 1 < argc) mode = argv[++i];
        else if (arg == "--window" && i + 1 < argc) window = std::stoi(argv[++i]);
        else if (arg == "--timeout" && i + 1 < argc) timeout = std::stoi(argv[++i]);
        else if (arg == "--loss" && i + 1 < argc) loss = std::stod(argv[++i]) / 100.0;
        else if (arg == "--dup" && i + 1 < argc) dup = std::stod(argv[++i]) / 100.0;
        else if (arg == "--corrupt" && i + 1 < argc) corrupt = std::stod(argv[++i]) / 100.0;
        else if (arg == "--delay" && i + 1 < argc) delay_ms = std::stoi(argv[++i]);
        else if (arg == "--seed" && i + 1 < argc) seed = std::stoi(argv[++i]);
        else if (arg == "--verbose") verbose = true;
        else if (arg == "--csv") csv = true;
        else if (arg == "--adaptive") adaptive = true;
        else if (arg == "--cc") cc = true;
        else if (arg == "--rto-log" && i + 1 < argc) rto_log_file = argv[++i];
        else if (arg == "--cwnd-log" && i + 1 < argc) cwnd_log_file = argv[++i];
    }

    if (window == 0) {
        window = (mode == "saw") ? 1 : 16;
    }

    Logger::set_level(verbose ? LogLevel::VERBOSE : LogLevel::NORMAL);

    UdpSocket socket;
    socket.set_remote(host, port);

    FaultConfig fcfg = {loss, dup, corrupt, delay_ms, seed};
    socket.set_fault_injector(std::make_shared<FaultInjector>(fcfg));

    std::unique_ptr<SenderBase> sender;
    if (mode == "gbn") {
        sender = std::make_unique<SenderGBN>(socket, timeout, window, adaptive, cc);
    } else if (mode == "sr") {
        sender = std::make_unique<SenderSR>(socket, timeout, window, adaptive, cc);
    } else {
        sender = std::make_unique<SenderSAW>(socket, timeout, window, adaptive, cc);
    }

    // Attempting to send_file
    if (!sender->send_file(filepath)) {
        LOG_ERROR("Transfer failed.");
        return 1;
    }

    const auto& stats = sender->get_stats();
    if (csv) {
        stats.print_csv_row();
    } else {
        stats.print_summary();
    }

    // Write optional log files
    if (!rto_log_file.empty()) {
        stats.write_rto_log(rto_log_file);
    }
    if (!cwnd_log_file.empty()) {
        stats.write_cwnd_log(cwnd_log_file);
    }

    try {
        std::string hash = sha256_file(filepath);
        LOG_INFO("Original file SHA256: %s", hash.c_str());
    } catch (...) {
        LOG_ERROR("Could not hash original file.");
    }

    return 0;
}
