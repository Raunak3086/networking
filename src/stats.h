#pragma once
#include <cstdint>
#include <chrono>
#include <string>
#include <mutex>
#include <vector>

struct TransferStats {
    uint64_t bytes_sent = 0;
    uint64_t bytes_received = 0;
    uint64_t packets_sent = 0;
    uint64_t packets_received = 0;
    uint64_t retransmissions = 0;
    uint64_t timeouts = 0;
    uint64_t duplicate_acks = 0;
    uint64_t out_of_order = 0;
    uint64_t checksum_failures = 0;
    std::vector<double> rtt_samples_ms;  // RTT samples in milliseconds
    
    std::chrono::steady_clock::time_point start_time;
    std::chrono::steady_clock::time_point end_time;
    
    std::string mode;  // "saw", "gbn", "sr"
    uint32_t window_size = 1;
    uint64_t file_size = 0;
    
    void start();
    void stop();
    double elapsed_seconds() const;
    double throughput_kbps() const;  // kilobits per second
    double avg_rtt_ms() const;
    
    // Print human-readable summary to stderr
    void print_summary() const;
    
    // Print CSV header and row to stdout
    static void print_csv_header();
    void print_csv_row() const;
};
