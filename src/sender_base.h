#pragma once
#include <string>
#include <fstream>
#include <memory>
#include <vector>
#include "udp_socket.h"
#include "stats.h"
#include "packet.h"

class SenderBase {
public:
    SenderBase(UdpSocket& socket, uint32_t timeout_ms, uint32_t window_size);
    virtual ~SenderBase() = default;
    
    // Main entry point: transfer a file. Returns true on success.
    bool send_file(const std::string& filepath);
    
    // Get transfer statistics
    const TransferStats& get_stats() const { return stats_; }
    
protected:
    UdpSocket& socket_;
    uint32_t timeout_ms_;
    uint32_t window_size_;
    TransferStats stats_;
    
    // Derived classes implement the data transfer phase
    virtual bool send_data(std::ifstream& file, uint64_t file_size) = 0;
    
    // Get the mode flag byte for the SYN packet
    virtual uint8_t get_mode_flag() const = 0;
    
    // Helper to receive and parse an ACK/response packet with timeout
    // Returns true if a valid packet was received, fills header and payload
    bool recv_response(PacketHeader& header, std::vector<uint8_t>& recv_buf, int timeout_override_ms = -1);
    
private:
    // Handshake: send SYN with file info, wait for SYN_ACK
    bool perform_handshake(const std::string& filename, uint64_t file_size);
    
    // Teardown: send FIN, wait for FIN_ACK
    bool perform_teardown();
};
