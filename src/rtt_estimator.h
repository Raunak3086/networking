#pragma once

#include <cmath>
#include <algorithm>

class RttEstimator {
public:
    static constexpr double ALPHA = 0.125;
    static constexpr double BETA = 0.25;
    static constexpr double MIN_RTO = 50.0;
    static constexpr double MAX_RTO = 2000.0;
    static constexpr double INITIAL_RTO = 200.0;

    RttEstimator()
        : srtt_(0.0),
          rttvar_(0.0),
          rto_(INITIAL_RTO),
          has_samples_(false),
          backoff_count_(0) {}

    void add_sample(double rtt_ms) {
        if (!has_samples_) {
            srtt_ = rtt_ms;
            rttvar_ = rtt_ms / 2.0;
            has_samples_ = true;
        } else {
            rttvar_ = (1.0 - BETA) * rttvar_ + BETA * std::abs(srtt_ - rtt_ms);
            srtt_ = (1.0 - ALPHA) * srtt_ + ALPHA * rtt_ms;
        }
        update_rto();
        reset_backoff();
    }

    void on_timeout() {
        rto_ = std::min(rto_ * 2.0, MAX_RTO);
        backoff_count_++;
    }

    void reset_backoff() {
        backoff_count_ = 0;
    }

    double get_rto_ms() const {
        return rto_;
    }

    double get_srtt_ms() const {
        return srtt_;
    }

    double get_rttvar_ms() const {
        return rttvar_;
    }

    bool has_samples() const {
        return has_samples_;
    }

private:
    void update_rto() {
        rto_ = srtt_ + 4.0 * rttvar_;
        rto_ = std::clamp(rto_, MIN_RTO, MAX_RTO);
    }

    double srtt_;
    double rttvar_;
    double rto_;
    bool has_samples_;
    int backoff_count_;
};
