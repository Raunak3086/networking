#pragma once
#include <cstdint>
#include <random>
#include <vector>
#include <functional>

struct FaultConfig {
    double loss_rate = 0.0;      // 0.0 to 1.0
    double dup_rate = 0.0;       // 0.0 to 1.0  
    double corrupt_rate = 0.0;   // 0.0 to 1.0
    uint32_t delay_ms = 0;       // Per-packet delay in milliseconds (0 = no delay)
    uint32_t seed = 0;           // 0 = use random_device
};

class FaultInjector {
public:
    explicit FaultInjector(const FaultConfig& config = {});
    
    // Returns a list of packet copies to actually send.
    // May return: empty (packet dropped), one copy (normal), two copies (duplicated).
    // The returned packets may have corrupted bytes.
    // If delay_ms > 0, this function sleeps before returning.
    std::vector<std::vector<uint8_t>> process(const std::vector<uint8_t>& packet);
    
    // Returns true if any fault injection is active
    bool is_active() const;
    
    // Stats
    uint64_t packets_dropped() const;
    uint64_t packets_duplicated() const;
    uint64_t packets_corrupted() const;
    uint64_t packets_delayed() const;
    
private:
    FaultConfig config_;
    std::mt19937 rng_;
    uint64_t dropped_ = 0;
    uint64_t duplicated_ = 0;
    uint64_t corrupted_ = 0;
    uint64_t delayed_ = 0;
};
