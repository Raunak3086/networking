#include "sender_sr.h"

bool SenderSR::send_data(std::ifstream& file, uint64_t file_size) {
    stats_.mode = "SR";
    std::thread ack_thread(&SenderSR::ack_thread_func, this);
    std::thread timer_thread(&SenderSR::timer_thread_func, this);
    std::vector<uint8_t> payload_buf(MAX_PAYLOAD_SIZE);
    
    while (!transfer_done_ && !transfer_error_) {
        std::unique_lock<std::mutex> lock(mtx_);
        
        cv_send_.wait(lock, [this]() {
            return (next_seq_ < base_ + window_size_ && !all_data_read_) || transfer_done_ || transfer_error_;
        });
        
        if (transfer_done_ || transfer_error_) break;
        
        if (file.read(reinterpret_cast<char*>(payload_buf.data()), MAX_PAYLOAD_SIZE) || file.gcount() > 0) {
            uint16_t bytes_read = static_cast<uint16_t>(file.gcount());
            auto packet = build_packet(PacketType::DATA, get_mode_flag(), next_seq_, payload_buf.data(), bytes_read);
            
            socket_.send_packet(packet);
            
            window_[next_seq_] = {packet, std::chrono::steady_clock::now(), false, 0};
            
            stats_.packets_sent++;
            stats_.bytes_sent += bytes_read;
            next_seq_++;
            
            cv_timer_.notify_one();
        } else {
            all_data_read_ = true;
            if (window_.empty()) {
                transfer_done_ = true;
                cv_timer_.notify_all();
            }
        }
    }
    
    if (ack_thread.joinable()) ack_thread.join();
    if (timer_thread.joinable()) timer_thread.join();
    
    return !transfer_error_;
}

void SenderSR::ack_thread_func() {
    PacketHeader header;
    std::vector<uint8_t> recv_buf;
    
    while (!transfer_done_ && !transfer_error_) {
        if (recv_response(header, recv_buf, 100)) {
            if (header.type == PacketType::ACK) {
                std::lock_guard<std::mutex> lock(mtx_);
                auto it = window_.find(header.sequence_number);
                if (it != window_.end() && !it->second.acked) {
                    it->second.acked = true;
                    if (it->second.retries == 0) {
                        double rtt = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - it->second.send_time).count();
                        stats_.rtt_samples_ms.push_back(rtt);
                    }
                    slide_window();
                    cv_send_.notify_all();
                    cv_timer_.notify_all();
                }
            }
        }
    }
}

void SenderSR::timer_thread_func() {
    while (!transfer_done_ && !transfer_error_) {
        std::unique_lock<std::mutex> lock(mtx_);
        
        if (window_.empty()) {
            cv_timer_.wait(lock, [this]() { return !window_.empty() || transfer_done_ || transfer_error_; });
            if (transfer_done_ || transfer_error_) break;
        }
        
        auto earliest_deadline = std::chrono::steady_clock::time_point::max();
        uint32_t timeout_seq = 0;
        bool has_unacked = false;
        
        for (const auto& [seq, record] : window_) {
            if (!record.acked) {
                auto deadline = record.send_time + std::chrono::milliseconds(timeout_ms_);
                if (deadline < earliest_deadline) {
                    earliest_deadline = deadline;
                    timeout_seq = seq;
                    has_unacked = true;
                }
            }
        }
        
        if (!has_unacked) {
            cv_timer_.wait(lock);
            continue;
        }
        
        if (cv_timer_.wait_until(lock, earliest_deadline) == std::cv_status::timeout) {
            auto it = window_.find(timeout_seq);
            if (it != window_.end() && !it->second.acked) {
                auto data_copy = it->second.data; // Copy before unlock
                it->second.retries++;
                it->second.send_time = std::chrono::steady_clock::now();
                stats_.retransmissions++;
                stats_.timeouts++;
                
                lock.unlock();
                socket_.send_packet(data_copy);
                lock.lock();
            }
        }
    }
}

void SenderSR::slide_window() {
    while (!window_.empty() && window_.begin()->second.acked) {
        window_.erase(window_.begin());
        base_++;
    }
    if (all_data_read_ && window_.empty()) {
        transfer_done_ = true;
        cv_send_.notify_all();
        cv_timer_.notify_all();
    }
}
