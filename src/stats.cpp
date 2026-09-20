#include "stats.h"
#include <iostream>
#include <numeric>

void TransferStats::start() {
    start_time = std::chrono::steady_clock::now();
}

void TransferStats::stop() {
    end_time = std::chrono::steady_clock::now();
}

double TransferStats::elapsed_seconds() const {
    std::chrono::duration<double> diff = end_time - start_time;
    return diff.count();
}

double TransferStats::throughput_kbps() const {
    double secs = elapsed_seconds();
    if (secs <= 0) return 0.0;
    return (bytes_sent * 8.0 / 1000.0) / secs;
}

double TransferStats::avg_rtt_ms() const {
    if (rtt_samples_ms.empty()) return 0.0;
    double sum = std::accumulate(rtt_samples_ms.begin(), rtt_samples_ms.end(), 0.0);
    return sum / rtt_samples_ms.size();
}

void TransferStats::print_summary() const {
    std::cerr << "--- Transfer Summary ---\n"
              << "Mode: " << mode << "\n"
              << "Window Size: " << window_size << "\n"
              << "File Size: " << file_size << " bytes\n"
              << "Time Elapsed: " << elapsed_seconds() << " s\n"
              << "Throughput: " << throughput_kbps() << " kbps\n"
              << "Avg RTT: " << avg_rtt_ms() << " ms\n"
              << "Bytes Sent: " << bytes_sent << "\n"
              << "Bytes Received: " << bytes_received << "\n"
              << "Packets Sent: " << packets_sent << "\n"
              << "Packets Received: " << packets_received << "\n"
              << "Retransmissions: " << retransmissions << "\n"
              << "Timeouts: " << timeouts << "\n"
              << "Duplicate ACKs: " << duplicate_acks << "\n"
              << "Out of Order: " << out_of_order << "\n"
              << "Checksum Failures: " << checksum_failures << "\n"
              << "------------------------\n";
}

void TransferStats::print_csv_header() {
    std::cout << "mode,window,file_size,bytes_sent,packets_sent,retransmissions,timeouts,elapsed_s,throughput_kbps,avg_rtt_ms\n";
}

void TransferStats::print_csv_row() const {
    std::cout << mode << ","
              << window_size << ","
              << file_size << ","
              << bytes_sent << ","
              << packets_sent << ","
              << retransmissions << ","
              << timeouts << ","
              << elapsed_seconds() << ","
              << throughput_kbps() << ","
              << avg_rtt_ms() << "\n";
}
