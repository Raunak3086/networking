#pragma once

#include <string>
#include <cstdint>
#include <cstddef>

std::string sha256_file(const std::string& filepath);

std::string sha256_data(const uint8_t* data, size_t length);
