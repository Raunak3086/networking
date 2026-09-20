#pragma once

#include <vector>
#include <cstdint>
#include <algorithm>
#include <cmath>

struct CwndSample {
    double time_ms;
    double cwnd;
    double ssthresh;
};

class CongestionController {
public:
    CongestionController()
        : cwnd_(1.0), ssthresh_(64.0), dup_ack_count_(0), in_fast_recovery_(false), enabled_(false) {}

    void enable() {
        enabled_ = true;
    }

    bool is_enabled() const {
        return enabled_;
    }

    void on_ack(double elapsed_ms) {
        if (!enabled_) return;

        if (cwnd_ < ssthresh_) {
            cwnd_ += 1.0;
        } else {
            cwnd_ += 1.0 / cwnd_;
        }

        dup_ack_count_ = 0;
        log_sample(elapsed_ms);
    }

    void on_dup_ack(double elapsed_ms) {
        if (!enabled_) return;

        dup_ack_count_++;
        if (dup_ack_count_ == 3) {
            ssthresh_ = std::max(cwnd_ / 2.0, 2.0);
            cwnd_ = ssthresh_ + 3.0;
            in_fast_recovery_ = true;
            log_sample(elapsed_ms);
        }
    }

    void on_timeout(double elapsed_ms) {
        if (!enabled_) return;

        ssthresh_ = std::max(cwnd_ / 2.0, 2.0);
        cwnd_ = 1.0;
        dup_ack_count_ = 0;
        in_fast_recovery_ = false;
        log_sample(elapsed_ms);
    }

    uint32_t get_cwnd() const {
        return std::max<uint32_t>(1, static_cast<uint32_t>(cwnd_));
    }

    double get_cwnd_double() const {
        return cwnd_;
    }

    double get_ssthresh() const {
        return ssthresh_;
    }

    bool should_fast_retransmit() const {
        return dup_ack_count_ == 3;
    }

    void exit_fast_recovery(double elapsed_ms) {
        cwnd_ = ssthresh_;
        in_fast_recovery_ = false;
        dup_ack_count_ = 0;
        log_sample(elapsed_ms);
    }

    const std::vector<CwndSample>& get_cwnd_log() const {
        return cwnd_log_;
    }

private:
    void log_sample(double elapsed_ms) {
        cwnd_log_.push_back({elapsed_ms, cwnd_, ssthresh_});
    }

    double cwnd_;
    double ssthresh_;
    uint32_t dup_ack_count_;
    bool in_fast_recovery_;
    bool enabled_;
    std::vector<CwndSample> cwnd_log_;
};
