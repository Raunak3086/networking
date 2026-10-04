#pragma once
#include "sender_base.h"
#include <map>
#include <mutex>
#include <condition_variable>
#include <thread>
#include <atomic>
#include <chrono>

class SenderSR : public SenderBase {
public:
    using SenderBase::SenderBase;
    
protected:
    bool send_data(std::ifstream& file, uint64_t file_size) override;
    uint8_t get_mode_flag() const override { return 2; } // SR = 2
    
private:
    struct PacketRecord {
        std::vector<uint8_t> data;     // Raw packet bytes
        std::chrono::steady_clock::time_point send_time;
        bool acked = false;
        int retries = 0;
    };
    
    std::mutex mtx_;
    std::condition_variable cv_send_;   // Signals window space available
    std::condition_variable cv_timer_;  // Signals timer state change
    uint32_t base_ = 0;
    uint32_t next_seq_ = 0;
    std::map<uint32_t, PacketRecord> window_;  // Active packets in window
    bool all_data_read_ = false;
    bool transfer_done_ = false;
    bool transfer_error_ = false;
    static constexpr int MAX_RETRIES = 20;
    
    void ack_thread_func();
    void timer_thread_func();
    void slide_window();
};
