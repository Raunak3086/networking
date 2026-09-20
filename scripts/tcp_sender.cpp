#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif

#include <winsock2.h>
#include <ws2tcpip.h>
#include <chrono>
#include <cstdint>
#include <fstream>
#include <iostream>
#include <string>

#pragma comment(lib, "ws2_32.lib")

int main(int argc, char* argv[]) {
    if (argc < 4) {
        std::cerr << "Usage: tcp_sender <host> <port> <filepath>\n";
        return 1;
    }

    const char* host = argv[1];
    const char* port = argv[2];
    const char* filepath = argv[3];

    std::ifstream file(filepath, std::ios::binary | std::ios::ate);
    if (!file.is_open()) {
        std::cerr << "Failed to open file: " << filepath << "\n";
        return 1;
    }
    uint64_t file_size = static_cast<uint64_t>(file.tellg());
    file.seekg(0, std::ios::beg);

    WSADATA wsa;
    if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0) {
        std::cerr << "WSAStartup failed: " << WSAGetLastError() << "\n";
        return 1;
    }

    struct addrinfo hints = {}, *res = nullptr;
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_protocol = IPPROTO_TCP;

    if (getaddrinfo(host, port, &hints, &res) != 0) {
        std::cerr << "getaddrinfo failed: " << WSAGetLastError() << "\n";
        WSACleanup();
        return 1;
    }

    SOCKET sock = socket(res->ai_family, res->ai_socktype, res->ai_protocol);
    if (sock == INVALID_SOCKET) {
        std::cerr << "Socket creation failed: " << WSAGetLastError() << "\n";
        freeaddrinfo(res);
        WSACleanup();
        return 1;
    }

    if (connect(sock, res->ai_addr, static_cast<int>(res->ai_addrlen)) == SOCKET_ERROR) {
        std::cerr << "Connect failed: " << WSAGetLastError() << "\n";
        freeaddrinfo(res);
        closesocket(sock);
        WSACleanup();
        return 1;
    }
    freeaddrinfo(res);

    constexpr size_t CHUNK_SIZE = 64 * 1024;
    char buffer[CHUNK_SIZE];
    uint64_t total_sent = 0;

    auto start_time = std::chrono::steady_clock::now();

    while (file) {
        file.read(buffer, sizeof(buffer));
        std::streamsize bytes_read = file.gcount();
        if (bytes_read <= 0) break;

        const char* ptr = buffer;
        std::streamsize remaining = bytes_read;
        while (remaining > 0) {
            int sent = send(sock, ptr, static_cast<int>(remaining), 0);
            if (sent == SOCKET_ERROR) {
                std::cerr << "Send failed: " << WSAGetLastError() << "\n";
                closesocket(sock);
                WSACleanup();
                return 1;
            }
            ptr += sent;
            remaining -= sent;
            total_sent += sent;
        }
    }

    shutdown(sock, SD_SEND);
    closesocket(sock);
    WSACleanup();

    auto end_time = std::chrono::steady_clock::now();
    std::chrono::duration<double> elapsed = end_time - start_time;
    double elapsed_sec = elapsed.count();
    double throughput_kbps = (elapsed_sec > 0) ? (total_sent * 8.0 / 1000.0) / elapsed_sec : 0.0;

    std::cerr << "File size: " << file_size << " bytes\n";
    std::cerr << "Elapsed time: " << elapsed_sec << " s\n";
    std::cerr << "Throughput: " << throughput_kbps << " kbps\n";

    return 0;
}
