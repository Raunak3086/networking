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

SenderBase::SenderBase(UdpSocket& socket, uint32_t timeout_ms, uint32_t window_size)
    : socket_(socket), timeout_ms_(timeout_ms), window_size_(window_size) {
}

bool SenderBase::send_file(const std::string& filepath) {
    stats_.start();
    
    std::filesystem::path p(filepath);
    if (!std::filesystem::exists(p)) {
        // Log error
        return false;
    }
    
    uint64_t file_size = std::filesystem::file_size(p);
    std::string filename = p.filename().string();
    
    stats_.file_size = file_size;
    stats_.window_size = window_size_;
    
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
    return true;
}

bool SenderBase::recv_response(PacketHeader& header, std::vector<uint8_t>& recv_buf, int timeout_override_ms) {
    int current_timeout = timeout_override_ms >= 0 ? timeout_override_ms : timeout_ms_;
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
