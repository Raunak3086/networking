#include "udp_socket.h"
#include "receiver.h"
#include "logger.h"
#include "fault_injection.h"
#include <iostream>
#include <string>
#include <memory>

void print_usage() {
    std::cout << "Usage: receiver <port> [output_dir] [options]\n"
              << "Options:\n"
              << "  --verbose          (enable verbose logging)\n"
              << "  --csv              (print stats as CSV)\n"
              << "  --loss PERCENT     (0-100, default: 0)\n"
              << "  --dup PERCENT      (0-100, default: 0)\n"
              << "  --corrupt PERCENT  (0-100, default: 0)\n"
              << "  --delay MS         (per-packet delay, default: 0)\n"
              << "  --seed N           (default: 0 = random)\n";
}

int main(int argc, char** argv) {
    if (argc < 2) {
        print_usage();
        return 1;
    }

    uint16_t port = std::stoi(argv[1]);
    std::string output_dir = ".";
    int opt_start = 2;

    if (argc > 2 && argv[2][0] != '-') {
        output_dir = argv[2];
        opt_start = 3;
    }

    double loss = 0, dup = 0, corrupt = 0;
    uint32_t delay_ms = 0;
    uint32_t seed = 0;
    bool verbose = false;
    bool csv = false;

    for (int i = opt_start; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "--loss" && i + 1 < argc) loss = std::stod(argv[++i]) / 100.0;
        else if (arg == "--dup" && i + 1 < argc) dup = std::stod(argv[++i]) / 100.0;
        else if (arg == "--corrupt" && i + 1 < argc) corrupt = std::stod(argv[++i]) / 100.0;
        else if (arg == "--delay" && i + 1 < argc) delay_ms = std::stoi(argv[++i]);
        else if (arg == "--seed" && i + 1 < argc) seed = std::stoi(argv[++i]);
        else if (arg == "--verbose") verbose = true;
        else if (arg == "--csv") csv = true;
    }

    Logger::set_level(verbose ? LogLevel::VERBOSE : LogLevel::NORMAL);

    UdpSocket socket;
    socket.bind(port);

    FaultConfig fcfg = {loss, dup, corrupt, delay_ms, seed};
    socket.set_fault_injector(std::make_shared<FaultInjector>(fcfg));

    Receiver receiver(socket, output_dir);
    if (!receiver.receive_file()) {
        LOG_ERROR("Receive failed.");
        return 1;
    }

    const auto& stats = receiver.get_stats();
    if (csv) {
        stats.print_csv_row();
    } else {
        stats.print_summary();
    }

    return 0;
}
