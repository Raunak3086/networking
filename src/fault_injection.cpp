#include "fault_injection.h"
#include <thread>
#include <chrono>

FaultInjector::FaultInjector(const FaultConfig& config) : config_(config) {
    if (config_.seed == 0) {
        std::random_device rd;
        rng_.seed(rd());
    } else {
        rng_.seed(config_.seed);
    }
}

bool FaultInjector::is_active() const {
    return config_.loss_rate > 0.0 || config_.dup_rate > 0.0 || 
           config_.corrupt_rate > 0.0 || config_.delay_ms > 0;
}

std::vector<std::vector<uint8_t>> FaultInjector::process(const std::vector<uint8_t>& packet) {
    if (!is_active()) {
        return {packet};
    }

    // Apply delay if configured
    if (config_.delay_ms > 0) {
        std::this_thread::sleep_for(std::chrono::milliseconds(config_.delay_ms));
        delayed_++;
    }

    std::uniform_real_distribution<double> dist(0.0, 1.0);

    // Check for drop
    if (config_.loss_rate > 0.0 && dist(rng_) < config_.loss_rate) {
        dropped_++;
        return {};
    }

    // Check for duplication
    bool dup = (config_.dup_rate > 0.0 && dist(rng_) < config_.dup_rate);
    int copies = dup ? 2 : 1;
    if (dup) duplicated_++;

    std::vector<std::vector<uint8_t>> results;
    for (int i = 0; i < copies; i++) {
        std::vector<uint8_t> pkt = packet;
        // Check for corruption
        if (config_.corrupt_rate > 0.0 && dist(rng_) < config_.corrupt_rate) {
            corrupted_++;
            if (!pkt.empty()) {
                std::uniform_int_distribution<size_t> byte_dist(0, pkt.size() - 1);
                std::uniform_int_distribution<unsigned int> bit_dist(0, 7);
                pkt[byte_dist(rng_)] ^= static_cast<uint8_t>(1 << bit_dist(rng_));
            }
        }
        results.push_back(std::move(pkt));
    }

    return results;
}

uint64_t FaultInjector::packets_dropped() const { return dropped_; }
uint64_t FaultInjector::packets_duplicated() const { return duplicated_; }
uint64_t FaultInjector::packets_corrupted() const { return corrupted_; }
uint64_t FaultInjector::packets_delayed() const { return delayed_; }
