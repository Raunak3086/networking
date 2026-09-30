#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif

#include <winsock2.h>
#include <ws2tcpip.h>
#pragma comment(lib, "ws2_32.lib")

using socket_t = SOCKET;
#define CLOSE_SOCKET closesocket
#define GET_SOCKET_ERR() std::to_string(WSAGetLastError())
#else
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <unistd.h>
#include <cerrno>
#include <cstring>

using socket_t = int;
constexpr socket_t INVALID_SOCKET = -1;
constexpr int SOCKET_ERROR = -1;
#define CLOSE_SOCKET close
#define GET_SOCKET_ERR() std::string(strerror(errno))
#endif

#include <chrono>
#include <cstdint>
#include <fstream>
#include <iostream>
#include <string>

int main(int argc, char* argv[]) {
    if (argc < 3) {
        std::cerr << "Usage: tcp_receiver <port> <output_filepath>\n";
        return 1;
    }

    uint16_t port = static_cast<uint16_t>(std::stoi(argv[1]));
    const char* output_filepath = argv[2];

    std::ofstream file(output_filepath, std::ios::binary | std::ios::trunc);
    if (!file.is_open()) {
        std::cerr << "Failed to open output file: " << output_filepath << "\n";
        return 1;
    }

#ifdef _WIN32
    WSADATA wsa;
    if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0) {
        std::cerr << "WSAStartup failed: " << WSAGetLastError() << "\n";
        return 1;
    }
#endif

    socket_t listen_sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (listen_sock == INVALID_SOCKET) {
        std::cerr << "Socket creation failed: " << GET_SOCKET_ERR() << "\n";
#ifdef _WIN32
        WSACleanup();
#endif
        return 1;
    }

    int opt = 1;
#ifdef _WIN32
    setsockopt(listen_sock, SOL_SOCKET, SO_REUSEADDR, (const char*)&opt, sizeof(opt));
#else
    setsockopt(listen_sock, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
#endif

    sockaddr_in server_addr = {};
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(port);

    if (bind(listen_sock, (sockaddr*)&server_addr, sizeof(server_addr)) == SOCKET_ERROR) {
        std::cerr << "Bind failed: " << GET_SOCKET_ERR() << "\n";
        CLOSE_SOCKET(listen_sock);
#ifdef _WIN32
        WSACleanup();
#endif
        return 1;
    }

    if (listen(listen_sock, 1) == SOCKET_ERROR) {
        std::cerr << "Listen failed: " << GET_SOCKET_ERR() << "\n";
        CLOSE_SOCKET(listen_sock);
#ifdef _WIN32
        WSACleanup();
#endif
        return 1;
    }

    socket_t client_sock = accept(listen_sock, nullptr, nullptr);
    if (client_sock == INVALID_SOCKET) {
        std::cerr << "Accept failed: " << GET_SOCKET_ERR() << "\n";
        CLOSE_SOCKET(listen_sock);
#ifdef _WIN32
        WSACleanup();
#endif
        return 1;
    }
    CLOSE_SOCKET(listen_sock);

    constexpr size_t BUFFER_SIZE = 64 * 1024;
    char buffer[BUFFER_SIZE];
    uint64_t total_received = 0;

    auto start_time = std::chrono::steady_clock::now();

    while (true) {
#ifdef _WIN32
        int bytes = recv(client_sock, buffer, static_cast<int>(sizeof(buffer)), 0);
#else
        ssize_t bytes = recv(client_sock, buffer, sizeof(buffer), 0);
#endif
        if (bytes > 0) {
            file.write(buffer, bytes);
            total_received += bytes;
        } else if (bytes == 0) {
            break;
        } else {
            std::cerr << "Recv failed: " << GET_SOCKET_ERR() << "\n";
            CLOSE_SOCKET(client_sock);
#ifdef _WIN32
            WSACleanup();
#endif
            return 1;
        }
    }

    file.close();
    CLOSE_SOCKET(client_sock);
#ifdef _WIN32
    WSACleanup();
#endif

    auto end_time = std::chrono::steady_clock::now();
    std::chrono::duration<double> elapsed = end_time - start_time;
    double elapsed_sec = elapsed.count();
    double throughput_kbps = (elapsed_sec > 0) ? (total_received * 8.0 / 1000.0) / elapsed_sec : 0.0;

    std::cerr << "Bytes received: " << total_received << "\n";
    std::cerr << "Elapsed time: " << elapsed_sec << " s\n";
    std::cerr << "Throughput: " << throughput_kbps << " kbps\n";

    return 0;
}
