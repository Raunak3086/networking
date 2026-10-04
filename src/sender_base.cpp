#include "sender_base.h"
#include "logger.h"
#include <filesystem>
#include <cstring>

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <winsock2.h>
#else
#include <arpa/inet.h>
#endif

SenderBase::SenderBase(UdpSocket& socket, uint32_t timeout_ms, uint32_t window_size,
                       bool adaptive_rto, bool cc_enabled)
    : socket_(socket), timeout_ms_(timeout_ms), window_size_(window_size),
      adaptive_rto_(adaptive_rto) {
    if (cc_enabled) {
        congestion_ctrl_.enable();
    }
}

bool SenderBase::send_file(const std::string& filepath) {
    stats_.start();
    
    std::filesystem::path p(filepath);
    if (!std::filesystem::is_regular_file(p)) {
        LOG_ERROR("Path does not exist or is not a regular file: %s", filepath.c_str());
        return false;
    }
    
    uint64_t file_size = std::filesystem::file_size(p);
    std::string filename = p.filename().string();
    
    stats_.file_size = file_size;
    stats_.window_size = window_size_;
    stats_.adaptive_rto = adaptive_rto_;
    stats_.cc_enabled = congestion_ctrl_.is_enabled();
    
    if (!perform_handshake(filename, file_size)) {
        return false;
    }
    
    std::ifstream file(filepath, std::ios::binary);
    if (!file) {
        return false;
    }
    
    if (!send_data(file, file_size)) {
        return false;
    }
    
    if (!perform_teardown()) {
        return false;
    }
    
    stats_.stop();
    
    // Record final RTT estimator state
    if (adaptive_rto_ && rtt_estimator_.has_samples()) {
        stats_.final_srtt_ms = rtt_estimator_.get_srtt_ms();
        stats_.final_rto_ms = rtt_estimator_.get_rto_ms();
    }
    
    // Record congestion control log
    if (congestion_ctrl_.is_enabled()) {
        stats_.cwnd_log = congestion_ctrl_.get_cwnd_log();
    }
    
    return true;
}

uint32_t SenderBase::get_effective_timeout() const {
    if (adaptive_rto_ && rtt_estimator_.has_samples()) {
        return static_cast<uint32_t>(rtt_estimator_.get_rto_ms());
    }
    return timeout_ms_;
}

uint32_t SenderBase::get_effective_window() const {
    if (congestion_ctrl_.is_enabled()) {
        uint32_t cwnd = congestion_ctrl_.get_cwnd();
        return std::min(window_size_, cwnd);
    }
    return window_size_;
}

void SenderBase::record_rtt_sample(double rtt_ms) {
    stats_.rtt_samples_ms.push_back(rtt_ms);
    if (adaptive_rto_) {
        rtt_estimator_.add_sample(rtt_ms);
        LOG_VERBOSE("[RTT] sample=%.1f srtt=%.1f rttvar=%.1f rto=%.1f",
                    rtt_ms, rtt_estimator_.get_srtt_ms(),
                    rtt_estimator_.get_rttvar_ms(), rtt_estimator_.get_rto_ms());
    }
}

bool SenderBase::recv_response(PacketHeader& header, std::vector<uint8_t>& recv_buf, int timeout_override_ms) {
    int current_timeout = timeout_override_ms >= 0 ? timeout_override_ms : static_cast<int>(get_effective_timeout());
    socket_.set_recv_timeout(current_timeout);
    
    sockaddr_in sender_addr;
    int bytes = socket_.recv_packet(recv_buf, sender_addr);
    
    if (bytes > 0) {
        const uint8_t* payload = nullptr;
        uint16_t payload_len = 0;
        if (parse_packet(recv_buf.data(), bytes, header, payload, payload_len)) {
            return true;
        }
    }
    return false;
}

bool SenderBase::perform_handshake(const std::string& filename, uint64_t file_size) {
    std::vector<uint8_t> syn_payload(10 + filename.size() + 1);
    for (int i = 7; i >= 0; --i) {
        syn_payload[7 - i] = static_cast<uint8_t>((file_size >> (i * 8)) & 0xFF);
    }
    uint16_t ws_net = htons(static_cast<uint16_t>(window_size_));
    std::memcpy(&syn_payload[8], &ws_net, 2);
    std::memcpy(&syn_payload[10], filename.c_str(), filename.size() + 1);
    
    auto syn_packet = build_packet(PacketType::SYN, get_mode_flag(), 0, syn_payload.data(), static_cast<uint16_t>(syn_payload.size()));
    
    for (int i = 0; i < 10; ++i) {
        socket_.send_packet(syn_packet);
        
        PacketHeader header;
        std::vector<uint8_t> recv_buf;
        if (recv_response(header, recv_buf) && header.type == PacketType::SYN_ACK) {
            return true;
        }
        stats_.timeouts++;
        stats_.retransmissions++;
    }
    return false;
}

bool SenderBase::perform_teardown() {
    auto fin_packet = build_packet(PacketType::FIN, 0, 0, nullptr, 0);
    
    for (int i = 0; i < 5; ++i) {
        socket_.send_packet(fin_packet);
        
        PacketHeader header;
        std::vector<uint8_t> recv_buf;
        if (recv_response(header, recv_buf) && header.type == PacketType::FIN_ACK) {
            return true;
        }
    }
    return false;
}
