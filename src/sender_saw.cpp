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
        bool was_retransmitted = false;
        for (int retry = 0; retry < 20; ++retry) {
            auto send_time = std::chrono::steady_clock::now();
            socket_.send_packet(packet);
            
            if (retry == 0) {
                stats_.packets_sent++;
                stats_.bytes_sent += bytes_read;
            } else {
                stats_.retransmissions++;
                was_retransmitted = true;
            }
            
            PacketHeader header;
            std::vector<uint8_t> recv_buf;
            if (recv_response(header, recv_buf)) {
                if (header.type == PacketType::ACK && header.sequence_number == seq_num + 1) {
                    auto recv_time = std::chrono::steady_clock::now();
                    double rtt = std::chrono::duration<double, std::milli>(recv_time - send_time).count();
                    
                    // Karn's algorithm: only sample non-retransmitted packets
                    if (!was_retransmitted) {
                        record_rtt_sample(rtt);
                    }
                    
                    // Congestion control
                    if (congestion_ctrl_.is_enabled()) {
                        double elapsed = std::chrono::duration<double, std::milli>(
                            recv_time - stats_.start_time).count();
                        congestion_ctrl_.on_ack(elapsed);
                    }
                    
                    acked = true;
                    seq_num++;
                    break;
                }
            } else {
                stats_.timeouts++;
                // Adaptive RTO: exponential backoff on timeout
                if (adaptive_rto_) {
                    rtt_estimator_.on_timeout();
                }
                // Congestion control: timeout
                if (congestion_ctrl_.is_enabled()) {
                    double elapsed = std::chrono::duration<double, std::milli>(
                        std::chrono::steady_clock::now() - stats_.start_time).count();
                    congestion_ctrl_.on_timeout(elapsed);
                }
            }
        }
        
        if (!acked) return false;
    }
    
    return true;
}
