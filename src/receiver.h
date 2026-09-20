#pragma once

#include "udp_socket.h"
#include "stats.h"
#include "packet.h"
#include <string>
#include <fstream>
#include <map>
#include <cstdint>
#include <vector>


class Receiver {
public:
    Receiver(UdpSocket& socket, const std::string& output_dir);
    
    // Wait for a connection and receive a file. Returns true on success.
    // Can be called in a loop for multiple transfers.
    bool receive_file();
    
    const TransferStats& get_stats() const { return stats_; }
    
private:
    UdpSocket& socket_;
    std::string output_dir_;
    TransferStats stats_;
    
    // Connection state
    sockaddr_in sender_addr_ = {};
    std::string filename_;
    uint64_t file_size_ = 0;
    uint32_t window_size_ = 1;
    uint8_t mode_ = 0;  // 0=SAW, 1=GBN, 2=SR
    
    // Receive state for GBN/SAW
    uint32_t expected_seq_ = 0;
    uint64_t bytes_written_ = 0;
    
    // Receive state for SR
    uint32_t rcv_base_ = 0;
    std::map<uint32_t, std::vector<uint8_t>> sr_buffer_;  // Out-of-order buffer
    
    bool handle_syn(const PacketHeader& header, const uint8_t* payload, uint16_t payload_len, const sockaddr_in& addr);
    bool handle_data_saw(const PacketHeader& header, const uint8_t* payload, uint16_t payload_len, std::ofstream& file);
    bool handle_data_gbn(const PacketHeader& header, const uint8_t* payload, uint16_t payload_len, std::ofstream& file);
    bool handle_data_sr(const PacketHeader& header, const uint8_t* payload, uint16_t payload_len, std::ofstream& file);
    bool handle_fin(const PacketHeader& header);
    
    void send_ack(PacketType type, uint32_t seq_num);
    void flush_sr_buffer(std::ofstream& file);  // Write contiguous packets from SR buffer
};
