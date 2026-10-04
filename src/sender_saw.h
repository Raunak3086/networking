#pragma once
#include "sender_base.h"

class SenderSAW : public SenderBase {
public:
    using SenderBase::SenderBase;  // Inherit constructor
    
protected:
    bool send_data(std::ifstream& file, uint64_t file_size) override;
    uint8_t get_mode_flag() const override { return 0; } // SAW = 0
};
