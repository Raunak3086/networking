#include "sender_sr.h"

bool SenderSR::send_data(std::ifstream& file, uint64_t file_size) {
    stats_.mode = "SR";
    std::thread ack_thread(&SenderSR::ack_thread_func, this);
    std::thread timer_thread(&SenderSR::timer_thread_func, this);
    std::vector<uint8_t> payload_buf(MAX_PAYLOAD_SIZE);
    
    while (!transfer_done_ && !transfer_error_) {
        std::unique_lock<std::mutex> lock(mtx_);
        
        cv_send_.wait(lock, [this]() {
            uint32_t eff_window = get_effective_window();
            return (next_seq_ < base_ + eff_window && !all_data_read_) || transfer_done_ || transfer_error_;
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
                    
                    // Karn's algorithm: only sample non-retransmitted packets
                    if (it->second.retries == 0) {
                        double rtt = std::chrono::duration<double, std::milli>(
                            std::chrono::steady_clock::now() - it->second.send_time).count();
                        record_rtt_sample(rtt);
                    }
                    
                    // Congestion control: on ACK
                    if (congestion_ctrl_.is_enabled()) {
                        double elapsed = std::chrono::duration<double, std::milli>(
                            std::chrono::steady_clock::now() - stats_.start_time).count();
                        congestion_ctrl_.on_ack(elapsed);
                    }
                    
                    slide_window();
                    cv_send_.notify_all();
                    cv_timer_.notify_all();
                } else if (it == window_.end() && header.sequence_number < base_) {
                    // Duplicate ACK (already acked packet)
                    if (congestion_ctrl_.is_enabled()) {
                        double elapsed = std::chrono::duration<double, std::milli>(
                            std::chrono::steady_clock::now() - stats_.start_time).count();
                        congestion_ctrl_.on_dup_ack(elapsed);
                        
                        // Fast retransmit on 3 dup ACKs
                        if (congestion_ctrl_.should_fast_retransmit()) {
                            // Retransmit the oldest unacked packet
                            if (!window_.empty()) {
                                auto first_unacked = window_.begin();
                                if (!first_unacked->second.acked) {
                                    if (first_unacked->second.retries >= MAX_RETRIES) {
                                        LOG_ERROR("SR: Max retries (%d) exceeded for seq %u during fast retransmit.", MAX_RETRIES, first_unacked->first);
                                        transfer_error_ = true;
                                        cv_send_.notify_all();
                                        cv_timer_.notify_all();
                                        break;
                                    }
                                    
                                    auto data_copy = first_unacked->second.data;
                                    first_unacked->second.retries++;
                                    first_unacked->second.send_time = std::chrono::steady_clock::now();
                                    stats_.retransmissions++;
                                    socket_.send_packet(data_copy);
                                }
                            }
                            congestion_ctrl_.exit_fast_recovery(
                                std::chrono::duration<double, std::milli>(
                                    std::chrono::steady_clock::now() - stats_.start_time).count());
                        }
                    }
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
        uint32_t eff_timeout = get_effective_timeout();
        
        for (const auto& [seq, record] : window_) {
            if (!record.acked) {
                auto deadline = record.send_time + std::chrono::milliseconds(eff_timeout);
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
                if (it->second.retries >= MAX_RETRIES) {
                    LOG_ERROR("SR: Max retries (%d) exceeded for seq %u, aborting transfer.", MAX_RETRIES, timeout_seq);
                    transfer_error_ = true;
                    cv_send_.notify_all();
                    cv_timer_.notify_all();
                    break;
                }
                
                auto data_copy = it->second.data; // Copy before unlock
                it->second.retries++;
                it->second.send_time = std::chrono::steady_clock::now();
                stats_.retransmissions++;
                stats_.timeouts++;
                
                // Adaptive RTO: exponential backoff on timeout
                if (adaptive_rto_) {
                    rtt_estimator_.on_timeout();
                }
                // Congestion control: timeout
                if (congestion_ctrl_.is_enabled()) {
                    double elapsed = std::chrono::duration<double, std::milli>(
                        std::chrono::steady_clock::now() - stats_.start_time).count();
                    congestion_ctrl_.on_timeout(elapsed);
                }
                
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
