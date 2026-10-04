#pragma once
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <winsock2.h>
#include <ws2tcpip.h>
#include <mstcpip.h>
#include <cstdint>
#include <vector>
#include <string>
#include <memory>
#include "fault_injection.h"

class UdpSocket {
public:
    UdpSocket();
    ~UdpSocket();
    
    // Non-copyable, movable
    UdpSocket(const UdpSocket&) = delete;
    UdpSocket& operator=(const UdpSocket&) = delete;
    UdpSocket(UdpSocket&& other) noexcept;
    UdpSocket& operator=(UdpSocket&& other) noexcept;
    
    // Bind to local port (for receiver)
    void bind(uint16_t port);
    
    // Set receive timeout in milliseconds
    void set_recv_timeout(uint32_t ms);
    
    // Set remote address (for sender convenience)
    void set_remote(const std::string& ip, uint16_t port);
    
    // Send packet to remote (uses set_remote address)
    // Goes through fault injector if set
    bool send_packet(const std::vector<uint8_t>& packet);
    
    // Send packet to specific address
    bool send_packet_to(const std::vector<uint8_t>& packet, const sockaddr_in& addr);
    
    // Receive packet, returns bytes received. -1 on error, 0 on timeout.
    int recv_packet(std::vector<uint8_t>& buffer, sockaddr_in& sender_addr);
    
    // Set fault injector
    void set_fault_injector(std::shared_ptr<FaultInjector> injector);
    
    // Get raw socket handle
    SOCKET handle() const;
    
private:
    SOCKET sock_ = INVALID_SOCKET;
    sockaddr_in remote_addr_ = {};
    bool remote_set_ = false;
    std::shared_ptr<FaultInjector> fault_injector_;
    
    static void init_wsa();
    void raw_send(const std::vector<uint8_t>& data, const sockaddr_in& addr);
};
