#pragma once
#include "sender_base.h"
#include <deque>
#include <mutex>
#include <condition_variable>
#include <thread>
#include <atomic>
#include <chrono>

class SenderGBN : public SenderBase {
public:
    using SenderBase::SenderBase;
    
protected:
    bool send_data(std::ifstream& file, uint64_t file_size) override;
    uint8_t get_mode_flag() const override { return 1; } // GBN = 1
    
private:
    struct BufferedPacket {
        std::vector<uint8_t> data;  // Raw packet bytes
        std::chrono::steady_clock::time_point send_time;
        bool retransmitted = false;  // Karn's algorithm: track if retransmitted
    };
    
    std::mutex mtx_;
    std::condition_variable cv_;
    uint32_t base_ = 0;           // Oldest unacked seq
    uint32_t next_seq_ = 0;       // Next seq to send
    uint32_t total_packets_ = 0;  // Total packets to send
    std::deque<BufferedPacket> buffer_;  // Sent but unacked packets
    bool all_data_read_ = false;
    bool transfer_done_ = false;
    bool transfer_error_ = false;
    std::chrono::steady_clock::time_point timer_deadline_;
    uint32_t consecutive_timeouts_ = 0;  // Tracks timeouts without progress
    static constexpr uint32_t MAX_CONSECUTIVE_TIMEOUTS = 20;
    
    void ack_thread_func();
    void resend_all_from_base();
};
