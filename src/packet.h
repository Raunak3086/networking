#pragma once

#include <cstdint>
#include <vector>
#include <cstddef>

enum class PacketType : uint8_t {
    DATA = 0, ACK = 1, SYN = 2, SYN_ACK = 3, FIN = 4, FIN_ACK = 5
};

constexpr size_t HEADER_SIZE = 10;
constexpr size_t MAX_PAYLOAD_SIZE = 1400;
constexpr size_t MAX_PACKET_SIZE = 1410;

struct PacketHeader {
    PacketType type;
    uint8_t flags;
    uint16_t payload_length;
    uint32_t sequence_number;
    uint16_t checksum;
};

// Build a packet: serialize header + payload into a byte vector
// Checksum is computed over the entire packet with checksum field set to 0
std::vector<uint8_t> build_packet(PacketType type, uint8_t flags, uint32_t seq_num, 
                                   const uint8_t* payload, uint16_t payload_len);

// Parse a packet from raw bytes. Returns false if too short or checksum fails.
bool parse_packet(const uint8_t* data, size_t len, PacketHeader& header, 
                  const uint8_t*& payload, uint16_t& payload_len);

// Internet checksum (RFC 1071)
uint16_t compute_checksum(const void* data, size_t length);

// Verify checksum: compute over entire packet including checksum field, should be 0
bool verify_checksum(const void* data, size_t length);

// Helper to get packet type name as string
const char* packet_type_name(PacketType type);
