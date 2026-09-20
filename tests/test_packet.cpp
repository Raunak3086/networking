#include "../src/packet.h"
#include <iostream>
#include <cassert>
#include <cstring>
#include <vector>

void test_build_and_parse_data() {
    uint8_t payload[] = "Hello";
    auto pkt = build_packet(PacketType::DATA, 1, 100, payload, 5);
    
    PacketHeader hdr;
    const uint8_t* parsed_payload;
    uint16_t parsed_len;
    
    bool ok = parse_packet(pkt.data(), pkt.size(), hdr, parsed_payload, parsed_len);
    assert(ok);
    assert(hdr.type == PacketType::DATA);
    assert(hdr.flags == 1);
    assert(hdr.sequence_number == 100);
    assert(hdr.payload_length == 5);
    assert(parsed_len == 5);
    assert(std::memcmp(parsed_payload, payload, 5) == 0);
    std::cout << "test_build_and_parse_data: PASS\n";
}

void test_build_and_parse_ack() {
    auto pkt = build_packet(PacketType::ACK, 0, 200, nullptr, 0);
    PacketHeader hdr;
    const uint8_t* parsed_payload;
    uint16_t parsed_len;
    
    bool ok = parse_packet(pkt.data(), pkt.size(), hdr, parsed_payload, parsed_len);
    assert(ok);
    assert(hdr.type == PacketType::ACK);
    assert(hdr.flags == 0);
    assert(hdr.sequence_number == 200);
    assert(hdr.payload_length == 0);
    assert(parsed_len == 0);
    assert(parsed_payload == nullptr);
    std::cout << "test_build_and_parse_ack: PASS\n";
}

void test_packet_types() {
    PacketType types[] = { PacketType::SYN, PacketType::SYN_ACK, PacketType::FIN, PacketType::FIN_ACK };
    for (auto type : types) {
        auto pkt = build_packet(type, 0, 0, nullptr, 0);
        PacketHeader hdr;
        const uint8_t* parsed_payload;
        uint16_t parsed_len;
        bool ok = parse_packet(pkt.data(), pkt.size(), hdr, parsed_payload, parsed_len);
        assert(ok);
        assert(hdr.type == type);
    }
    std::cout << "test_packet_types: PASS\n";
}

void test_checksum_correct() {
    auto pkt = build_packet(PacketType::DATA, 0, 1, nullptr, 0);
    assert(verify_checksum(pkt.data(), pkt.size()));
    std::cout << "test_checksum_correct: PASS\n";
}

void test_corrupt_byte() {
    auto pkt = build_packet(PacketType::DATA, 0, 1, (const uint8_t*)"1234", 4);
    pkt[5] ^= 0xFF; // corrupt sequence number
    PacketHeader hdr;
    const uint8_t* parsed_payload;
    uint16_t parsed_len;
    bool ok = parse_packet(pkt.data(), pkt.size(), hdr, parsed_payload, parsed_len);
    assert(!ok); // Should fail checksum
    std::cout << "test_corrupt_byte: PASS\n";
}

void test_too_short() {
    std::vector<uint8_t> pkt(9, 0);
    PacketHeader hdr;
    const uint8_t* parsed_payload;
    uint16_t parsed_len;
    bool ok = parse_packet(pkt.data(), pkt.size(), hdr, parsed_payload, parsed_len);
    assert(!ok);
    std::cout << "test_too_short: PASS\n";
}

void test_max_payload() {
    std::vector<uint8_t> payload(MAX_PAYLOAD_SIZE, 0xAA);
    auto pkt = build_packet(PacketType::DATA, 0, 1, payload.data(), MAX_PAYLOAD_SIZE);
    PacketHeader hdr;
    const uint8_t* parsed_payload;
    uint16_t parsed_len;
    bool ok = parse_packet(pkt.data(), pkt.size(), hdr, parsed_payload, parsed_len);
    assert(ok);
    assert(parsed_len == MAX_PAYLOAD_SIZE);
    assert(std::memcmp(parsed_payload, payload.data(), MAX_PAYLOAD_SIZE) == 0);
    std::cout << "test_max_payload: PASS\n";
}

void test_zero_payload() {
    auto pkt = build_packet(PacketType::DATA, 0, 1, nullptr, 0);
    PacketHeader hdr;
    const uint8_t* parsed_payload;
    uint16_t parsed_len;
    bool ok = parse_packet(pkt.data(), pkt.size(), hdr, parsed_payload, parsed_len);
    assert(ok);
    assert(parsed_len == 0);
    std::cout << "test_zero_payload: PASS\n";
}

void test_compute_checksum() {
    uint8_t data[] = { 0x00, 0x01, 0x00, 0xf2 };
    uint16_t csum = compute_checksum(data, 4);
    assert(csum != 0); 
    std::cout << "test_compute_checksum: PASS\n";
}

void test_verify_checksum() {
    auto pkt = build_packet(PacketType::ACK, 0, 10, nullptr, 0);
    assert(verify_checksum(pkt.data(), pkt.size()));
    std::cout << "test_verify_checksum: PASS\n";
}

int main() {
    try {
        test_build_and_parse_data();
        test_build_and_parse_ack();
        test_packet_types();
        test_checksum_correct();
        test_corrupt_byte();
        test_too_short();
        test_max_payload();
        test_zero_payload();
        test_compute_checksum();
        test_verify_checksum();
        std::cout << "ALL TESTS PASSED.\n";
        return 0;
    } catch (...) {
        std::cout << "A TEST FAILED.\n";
        return 1;
    }
}
