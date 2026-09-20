#include "receiver.h"
#include "logger.h"
#include "sha256.h"
#include <iostream>
#include <filesystem>
#include <cstring>

Receiver::Receiver(UdpSocket& socket, const std::string& output_dir)
    : socket_(socket), output_dir_(output_dir) {}

bool Receiver::receive_file() {
    socket_.set_recv_timeout(30000); // 30 seconds master timeout

    std::vector<uint8_t> buffer(MAX_PACKET_SIZE);
    sockaddr_in sender_addr;
    bool in_transfer = false;
    std::ofstream file;

    while (true) {
        int bytes_received = socket_.recv_packet(buffer, sender_addr);
        
        if (bytes_received <= 0) {
            if (in_transfer) {
                LOG_ERROR("Timeout or error waiting for packet");
                file.close();
                return false;
            }
            continue; // Keep waiting for SYN
        }

        PacketHeader header;
        const uint8_t* payload;
        uint16_t payload_len;

        if (!parse_packet(buffer.data(), bytes_received, header, payload, payload_len)) {
            stats_.checksum_failures++;
            continue; // Invalid packet
        }

        stats_.packets_received++;
        stats_.bytes_received += bytes_received;

        if (header.type == PacketType::SYN) {
            if (in_transfer && memcmp(&sender_addr_, &sender_addr, sizeof(sockaddr_in)) != 0) {
                // Ignore SYN from different sender while in transfer
                continue;
            }

            if (!handle_syn(header, payload, payload_len, sender_addr)) {
                continue;
            }
            
            in_transfer = true;
            
            std::filesystem::path p(output_dir_);
            p /= filename_;
            file.open(p.string(), std::ios::binary);
            if (!file.is_open()) {
                LOG_ERROR("Failed to open file for writing: %s", p.string().c_str());
                return false;
            }
        }
        else if (in_transfer && header.type == PacketType::DATA) {
            if (mode_ == 0) handle_data_saw(header, payload, payload_len, file);
            else if (mode_ == 1) handle_data_gbn(header, payload, payload_len, file);
            else if (mode_ == 2) handle_data_sr(header, payload, payload_len, file);
        }
        else if (in_transfer && header.type == PacketType::FIN) {
            if (handle_fin(header)) {
                break;
            }
        }
    }

    file.close();
    
    stats_.stop();
    
    std::filesystem::path p(output_dir_);
    p /= filename_;
    
    if (bytes_written_ != file_size_) {
        LOG_ERROR("File size mismatch. Expected: %llu, Got: %llu", file_size_, bytes_written_);
        return false;
    }

    if (file_size_ > 0) {
        std::string hash = sha256_file(p.string());
        LOG_INFO("Received file %s successfully. SHA256: %s", p.string().c_str(), hash.c_str());
    } else {
        LOG_INFO("Received empty file %s successfully.", p.string().c_str());
    }

    return true;
}

bool Receiver::handle_syn(const PacketHeader& header, const uint8_t* payload, uint16_t payload_len, const sockaddr_in& addr) {
    if (payload_len < 10) return false;

    sender_addr_ = addr;
    mode_ = header.flags & 0x03;

    file_size_ = 0;
    for (int i = 0; i < 8; ++i) {
        file_size_ = (file_size_ << 8) | payload[i];
    }
    
    window_size_ = (payload[8] << 8) | payload[9];
    
    if (payload_len > 10) {
        filename_ = std::string(reinterpret_cast<const char*>(payload + 10), payload_len - 10);
        // Null-terminate string if it wasn't
        if (filename_.back() == '\0') filename_.pop_back();
    } else {
        filename_ = "unknown.bin";
    }

    expected_seq_ = 0;
    rcv_base_ = 0;
    bytes_written_ = 0;
    sr_buffer_.clear();

    stats_.file_size = file_size_;
    stats_.window_size = window_size_;
    stats_.mode = (mode_ == 0) ? "SAW" : (mode_ == 1) ? "GBN" : "SR";
    stats_.start();

    LOG_INFO("[SYN] file=%s size=%llu mode=%d window=%d", filename_.c_str(), file_size_, mode_, window_size_);

    send_ack(PacketType::SYN_ACK, 0);
    return true;
}

bool Receiver::handle_data_saw(const PacketHeader& header, const uint8_t* payload, uint16_t payload_len, std::ofstream& file) {
    uint32_t seq = header.sequence_number;

    if (seq == expected_seq_) {
        if (payload_len > 0) {
            file.write(reinterpret_cast<const char*>(payload), payload_len);
            bytes_written_ += payload_len;
        }
        expected_seq_++;
        send_ack(PacketType::ACK, expected_seq_);
    } else if (seq < expected_seq_) {
        // Duplicate, re-ack
        stats_.duplicate_acks++;
        send_ack(PacketType::ACK, expected_seq_);
    } else {
        // Out of order (shouldn't happen in SAW)
        stats_.out_of_order++;
        send_ack(PacketType::ACK, expected_seq_);
    }
    return true;
}

bool Receiver::handle_data_gbn(const PacketHeader& header, const uint8_t* payload, uint16_t payload_len, std::ofstream& file) {
    uint32_t seq = header.sequence_number;

    if (seq == expected_seq_) {
        if (payload_len > 0) {
            file.write(reinterpret_cast<const char*>(payload), payload_len);
            bytes_written_ += payload_len;
        }
        expected_seq_++;
        LOG_VERBOSE("[RECV] DATA seq=%u, [SEND] ACK next_expected=%u", seq, expected_seq_);
        send_ack(PacketType::ACK, expected_seq_);
    } else {
        stats_.out_of_order++;
        LOG_VERBOSE("[RECV] DATA seq=%u OUT OF ORDER, expected=%u", seq, expected_seq_);
        send_ack(PacketType::ACK, expected_seq_); // Re-ack next expected
    }
    return true;
}

bool Receiver::handle_data_sr(const PacketHeader& header, const uint8_t* payload, uint16_t payload_len, std::ofstream& file) {
    uint32_t seq = header.sequence_number;

    if (seq >= rcv_base_ && seq < rcv_base_ + window_size_) {
        send_ack(PacketType::ACK, seq);

        if (seq == rcv_base_) {
            if (payload_len > 0) {
                file.write(reinterpret_cast<const char*>(payload), payload_len);
                bytes_written_ += payload_len;
            }
            rcv_base_++;
            flush_sr_buffer(file);
        } else if (sr_buffer_.find(seq) == sr_buffer_.end()) {
            sr_buffer_[seq] = std::vector<uint8_t>(payload, payload + payload_len);
            stats_.out_of_order++;
        }
    } else if (seq < rcv_base_) {
        stats_.duplicate_acks++;
        send_ack(PacketType::ACK, seq);
    }
    // Else beyond window, ignore
    return true;
}

void Receiver::flush_sr_buffer(std::ofstream& file) {
    while (sr_buffer_.find(rcv_base_) != sr_buffer_.end()) {
        const auto& data = sr_buffer_[rcv_base_];
        if (!data.empty()) {
            file.write(reinterpret_cast<const char*>(data.data()), data.size());
            bytes_written_ += data.size();
        }
        sr_buffer_.erase(rcv_base_);
        rcv_base_++;
    }
}

bool Receiver::handle_fin(const PacketHeader& header) {
    send_ack(PacketType::FIN_ACK, 0);
    return true;
}

void Receiver::send_ack(PacketType type, uint32_t seq_num) {
    std::vector<uint8_t> packet = build_packet(type, 0, seq_num, nullptr, 0);
    socket_.send_packet_to(packet, sender_addr_);
}
