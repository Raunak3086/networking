#include "sender_gbn.h"

bool SenderGBN::send_data(std::ifstream& file, uint64_t file_size) {
    stats_.mode = "GBN";
    std::thread ack_thread(&SenderGBN::ack_thread_func, this);
    std::vector<uint8_t> payload_buf(MAX_PAYLOAD_SIZE);
    
    while (!transfer_done_ && !transfer_error_) {
        std::unique_lock<std::mutex> lock(mtx_);
        
        while (next_seq_ < base_ + window_size_ && !all_data_read_) {
            if (file.read(reinterpret_cast<char*>(payload_buf.data()), MAX_PAYLOAD_SIZE) || file.gcount() > 0) {
                uint16_t bytes_read = static_cast<uint16_t>(file.gcount());
                auto packet = build_packet(PacketType::DATA, get_mode_flag(), next_seq_, payload_buf.data(), bytes_read);
                
                socket_.send_packet(packet);
                
                if (buffer_.empty()) {
                    timer_deadline_ = std::chrono::steady_clock::now() + std::chrono::milliseconds(timeout_ms_);
                }
                buffer_.push_back({packet, std::chrono::steady_clock::now()});
                
                stats_.packets_sent++;
                stats_.bytes_sent += bytes_read;
                next_seq_++;
            } else {
                all_data_read_ = true;
            }
        }
        
        if (all_data_read_ && base_ == next_seq_) {
            transfer_done_ = true;
            break;
        }
        
        if (!buffer_.empty()) {
            if (cv_.wait_until(lock, timer_deadline_) == std::cv_status::timeout) {
                resend_all_from_base();
                stats_.timeouts++;
            }
        } else {
            cv_.wait(lock);
        }
    }
    
    if (ack_thread.joinable()) {
        ack_thread.join();
    }
    
    return !transfer_error_;
}

void SenderGBN::ack_thread_func() {
    PacketHeader header;
    std::vector<uint8_t> recv_buf;
    
    while (!transfer_done_ && !transfer_error_) {
        if (recv_response(header, recv_buf, 100)) { // 100ms timeout
            if (header.type == PacketType::ACK) {
                std::lock_guard<std::mutex> lock(mtx_);
                if (header.sequence_number > base_) {
                    uint32_t acked_count = header.sequence_number - base_;
                    for (uint32_t i = 0; i < acked_count && !buffer_.empty(); ++i) {
                        buffer_.pop_front();
                    }
                    base_ = header.sequence_number;
                    
                    if (!buffer_.empty()) {
                        timer_deadline_ = std::chrono::steady_clock::now() + std::chrono::milliseconds(timeout_ms_);
                    }
                    cv_.notify_all();
                }
            }
        }
    }
}

void SenderGBN::resend_all_from_base() {
    for (const auto& bp : buffer_) {
        socket_.send_packet(bp.data);
        stats_.retransmissions++;
    }
    timer_deadline_ = std::chrono::steady_clock::now() + std::chrono::milliseconds(timeout_ms_);
}
