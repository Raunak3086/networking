#include "sender_saw.h"
#include <chrono>

bool SenderSAW::send_data(std::ifstream& file, uint64_t file_size) {
    stats_.mode = "SAW";
    uint32_t seq_num = 0;
    std::vector<uint8_t> payload_buf(MAX_PAYLOAD_SIZE);
    
    while (file.read(reinterpret_cast<char*>(payload_buf.data()), MAX_PAYLOAD_SIZE) || file.gcount() > 0) {
        uint16_t bytes_read = static_cast<uint16_t>(file.gcount());
        auto packet = build_packet(PacketType::DATA, get_mode_flag(), seq_num, payload_buf.data(), bytes_read);
        
        bool acked = false;
        for (int retry = 0; retry < 20; ++retry) {
            auto send_time = std::chrono::steady_clock::now();
            socket_.send_packet(packet);
            
            if (retry == 0) {
                stats_.packets_sent++;
                stats_.bytes_sent += bytes_read;
            } else {
                stats_.retransmissions++;
            }
            
            PacketHeader header;
            std::vector<uint8_t> recv_buf;
            if (recv_response(header, recv_buf)) {
                if (header.type == PacketType::ACK && header.sequence_number == seq_num + 1) {
                    auto recv_time = std::chrono::steady_clock::now();
                    double rtt = std::chrono::duration<double, std::milli>(recv_time - send_time).count();
                    stats_.rtt_samples_ms.push_back(rtt);
                    acked = true;
                    seq_num++;
                    break;
                }
            } else {
                stats_.timeouts++;
            }
        }
        
        if (!acked) return false;
    }
    
    return true;
}
