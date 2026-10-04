#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <winsock2.h>
#else
#include <arpa/inet.h>
#endif

#include "packet.h"
#include <cstring>
#include <stdexcept>

const char* packet_type_name(PacketType type) {
    switch(type) {
        case PacketType::DATA: return "DATA";
        case PacketType::ACK: return "ACK";
        case PacketType::SYN: return "SYN";
        case PacketType::SYN_ACK: return "SYN_ACK";
        case PacketType::FIN: return "FIN";
        case PacketType::FIN_ACK: return "FIN_ACK";
        default: return "UNKNOWN";
    }
}

uint16_t compute_checksum(const void* data, size_t length) {
    const uint8_t* ptr = static_cast<const uint8_t*>(data);
    uint32_t sum = 0;
    while (length > 1) {
        uint16_t word;
        std::memcpy(&word, ptr, 2);
        sum += word;
        ptr += 2;
        length -= 2;
    }
    if (length == 1) {
        uint16_t word = 0;
        std::memcpy(&word, ptr, 1);
        sum += word;
    }
    while (sum >> 16) {
        sum = (sum & 0xFFFF) + (sum >> 16);
    }
    return static_cast<uint16_t>(~sum);
}

bool verify_checksum(const void* data, size_t length) {
    return compute_checksum(data, length) == 0;
}

std::vector<uint8_t> build_packet(PacketType type, uint8_t flags, uint32_t seq_num, 
                                   const uint8_t* payload, uint16_t payload_len) {
    if (payload_len > MAX_PAYLOAD_SIZE) {
        throw std::invalid_argument("Payload too large");
    }
    std::vector<uint8_t> packet(HEADER_SIZE + payload_len);
    
    packet[0] = static_cast<uint8_t>(type);
    packet[1] = flags;
    
    uint16_t net_len = htons(payload_len);
    std::memcpy(&packet[2], &net_len, 2);
    
    uint32_t net_seq = htonl(seq_num);
    std::memcpy(&packet[4], &net_seq, 4);
    
    uint16_t zero_checksum = 0;
    std::memcpy(&packet[8], &zero_checksum, 2);
    
    if (payload_len > 0 && payload != nullptr) {
        std::memcpy(&packet[10], payload, payload_len);
    }
    
    uint16_t checksum = compute_checksum(packet.data(), packet.size());
    std::memcpy(&packet[8], &checksum, 2);
    
    return packet;
}

bool parse_packet(const uint8_t* data, size_t len, PacketHeader& header, 
                  const uint8_t*& payload, uint16_t& payload_len) {
    if (len < HEADER_SIZE) return false;
    if (!verify_checksum(data, len)) return false;
    
    header.type = static_cast<PacketType>(data[0]);
    header.flags = data[1];
    
    uint16_t net_len;
    std::memcpy(&net_len, &data[2], 2);
    header.payload_length = ntohs(net_len);
    
    uint32_t net_seq;
    std::memcpy(&net_seq, &data[4], 4);
    header.sequence_number = ntohl(net_seq);
    
    std::memcpy(&header.checksum, &data[8], 2);
    
    if (HEADER_SIZE + header.payload_length > len) return false;
    
    payload_len = header.payload_length;
    payload = payload_len > 0 ? (data + HEADER_SIZE) : nullptr;
    
    return true;
}
