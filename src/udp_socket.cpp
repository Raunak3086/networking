#include "udp_socket.h"
#include "packet.h"
#include "logger.h"
#include <iostream>

#ifdef _WIN32
#include <mutex>

// SIO_UDP_CONNRESET may not be defined in all versions of mstcpip.h
#ifndef SIO_UDP_CONNRESET
#define SIO_UDP_CONNRESET _WSAIOW(IOC_VENDOR, 12)
#endif

static std::once_flag wsa_init_flag;

void UdpSocket::init_wsa() {
    std::call_once(wsa_init_flag, []() {
        WSADATA wsaData;
        int result = WSAStartup(MAKEWORD(2, 2), &wsaData);
        if (result != 0) {
            std::cerr << "WSAStartup failed: " << result << std::endl;
            exit(1);
        }
        std::atexit([]() { WSACleanup(); });
    });
}
#endif

UdpSocket::UdpSocket() {
#ifdef _WIN32
    init_wsa();
#endif
    sock_ = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (sock_ == INVALID_SOCKET) {
        return;
    }

#ifdef _WIN32
    // Apply SIO_UDP_CONNRESET fix to prevent WSAECONNRESET
    DWORD bytesReturned = 0;
    BOOL bNewBehavior = FALSE;
    WSAIoctl(sock_, SIO_UDP_CONNRESET, &bNewBehavior, sizeof(bNewBehavior),
             NULL, 0, &bytesReturned, NULL, NULL);
#endif

    // Set 2MB buffers for sliding window
    int rcvbuf = 2 * 1024 * 1024;
#ifdef _WIN32
    setsockopt(sock_, SOL_SOCKET, SO_RCVBUF, (const char*)&rcvbuf, sizeof(rcvbuf));
    setsockopt(sock_, SOL_SOCKET, SO_SNDBUF, (const char*)&rcvbuf, sizeof(rcvbuf));
#else
    setsockopt(sock_, SOL_SOCKET, SO_RCVBUF, &rcvbuf, sizeof(rcvbuf));
    setsockopt(sock_, SOL_SOCKET, SO_SNDBUF, &rcvbuf, sizeof(rcvbuf));
#endif
}

UdpSocket::~UdpSocket() {
    if (sock_ != INVALID_SOCKET) {
        CLOSE_SOCKET(sock_);
    }
}

UdpSocket::UdpSocket(UdpSocket&& other) noexcept 
    : sock_(other.sock_), remote_addr_(other.remote_addr_), 
      remote_set_(other.remote_set_), fault_injector_(std::move(other.fault_injector_)) {
    other.sock_ = INVALID_SOCKET;
}

UdpSocket& UdpSocket::operator=(UdpSocket&& other) noexcept {
    if (this != &other) {
        if (sock_ != INVALID_SOCKET) {
            CLOSE_SOCKET(sock_);
        }
        sock_ = other.sock_;
        remote_addr_ = other.remote_addr_;
        remote_set_ = other.remote_set_;
        fault_injector_ = std::move(other.fault_injector_);
        other.sock_ = INVALID_SOCKET;
    }
    return *this;
}

void UdpSocket::bind(uint16_t port) {
    int opt = 1;
#ifdef _WIN32
    setsockopt(sock_, SOL_SOCKET, SO_REUSEADDR, (const char*)&opt, sizeof(opt));
#else
    setsockopt(sock_, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
#endif

    sockaddr_in local_addr = {};
    local_addr.sin_family = AF_INET;
    local_addr.sin_addr.s_addr = INADDR_ANY;
    local_addr.sin_port = htons(port);

    ::bind(sock_, (sockaddr*)&local_addr, sizeof(local_addr));
}

void UdpSocket::set_recv_timeout(uint32_t ms) {
#ifdef _WIN32
    DWORD timeout = ms;
    setsockopt(sock_, SOL_SOCKET, SO_RCVTIMEO, (const char*)&timeout, sizeof(timeout));
#else
    struct timeval tv;
    tv.tv_sec = ms / 1000;
    tv.tv_usec = (ms % 1000) * 1000;
    setsockopt(sock_, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
#endif
}

void UdpSocket::set_remote(const std::string& ip, uint16_t port) {
    remote_addr_ = {};
    remote_addr_.sin_family = AF_INET;
    remote_addr_.sin_port = htons(port);
    inet_pton(AF_INET, ip.c_str(), &remote_addr_.sin_addr);
    remote_set_ = true;
}

void UdpSocket::raw_send(const std::vector<uint8_t>& data, const sockaddr_in& addr) {
#ifdef _WIN32
    sendto(sock_, (const char*)data.data(), (int)data.size(), 0, (const sockaddr*)&addr, sizeof(addr));
#else
    sendto(sock_, data.data(), data.size(), 0, (const sockaddr*)&addr, sizeof(addr));
#endif
}

bool UdpSocket::send_packet(const std::vector<uint8_t>& packet) {
    if (!remote_set_) return false;
    return send_packet_to(packet, remote_addr_);
}

bool UdpSocket::send_packet_to(const std::vector<uint8_t>& packet, const sockaddr_in& addr) {
    if (fault_injector_ && fault_injector_->is_active()) {
        auto packets = fault_injector_->process(packet);
        for (const auto& p : packets) {
            raw_send(p, addr);
        }
        return true;
    } else {
        raw_send(packet, addr);
        return true;
    }
}

int UdpSocket::recv_packet(std::vector<uint8_t>& buffer, sockaddr_in& sender_addr) {
    buffer.resize(MAX_PACKET_SIZE);
#ifdef _WIN32
    int sender_len = sizeof(sender_addr);
    int bytes = recvfrom(sock_, (char*)buffer.data(), (int)buffer.size(), 0, (sockaddr*)&sender_addr, &sender_len);

    if (bytes == SOCKET_ERROR) {
        int err = WSAGetLastError();
        if (err == WSAETIMEDOUT) {
            return 0;
        }
        return -1;
    }
#else
    socklen_t sender_len = sizeof(sender_addr);
    int bytes = recvfrom(sock_, (char*)buffer.data(), buffer.size(), 0, (sockaddr*)&sender_addr, &sender_len);

    if (bytes == SOCKET_ERROR) {
        if (errno == EAGAIN || errno == EWOULDBLOCK) {
            return 0;
        }
        return -1;
    }
#endif

    buffer.resize(bytes);
    return bytes;
}

void UdpSocket::set_fault_injector(std::shared_ptr<FaultInjector> injector) {
    fault_injector_ = std::move(injector);
}

socket_t UdpSocket::handle() const {
    return sock_;
}
