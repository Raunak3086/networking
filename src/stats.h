#pragma once
#include <cstdint>
#include <chrono>
#include <string>
#include <mutex>
#include <vector>
#include "congestion_control.h"

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
    
    // Phase 5: Adaptive RTO final state
    bool adaptive_rto = false;
    double final_srtt_ms = 0.0;
    double final_rto_ms = 0.0;
    
    // Phase 6: Congestion control
    bool cc_enabled = false;
    std::vector<CwndSample> cwnd_log;
    
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
    
    // Write RTO log to file (for plotting)
    void write_rto_log(const std::string& filepath) const;
    
    // Write cwnd log to file (for plotting)
    void write_cwnd_log(const std::string& filepath) const;
};
