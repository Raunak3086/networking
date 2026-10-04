#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <bcrypt.h>
#include <stdexcept>
#include <fstream>
#include <vector>
#include <iomanip>
#include <sstream>
#include "sha256.h"

#pragma comment(lib, "bcrypt.lib")

namespace {
    std::string to_hex(const std::vector<uint8_t>& data) {
        std::ostringstream oss;
        for (auto b : data) {
            oss << std::hex << std::setw(2) << std::setfill('0') << static_cast<int>(b);
        }
        return oss.str();
    }
    
    class BCryptHashContext {
    public:
        BCryptHashContext() {
            if (BCryptOpenAlgorithmProvider(&hAlg, BCRYPT_SHA256_ALGORITHM, NULL, 0) != 0) {
                throw std::runtime_error("Failed to open algorithm provider");
            }
            DWORD cbHashObject = 0, cbData = 0;
            BCryptGetProperty(hAlg, BCRYPT_OBJECT_LENGTH, (PBYTE)&cbHashObject, sizeof(DWORD), &cbData, 0);
            hashObj.resize(cbHashObject);
            DWORD hashLen = 0;
            BCryptGetProperty(hAlg, BCRYPT_HASH_LENGTH, (PBYTE)&hashLen, sizeof(DWORD), &cbData, 0);
            hash.resize(hashLen);
            
            if (BCryptCreateHash(hAlg, &hHash, hashObj.data(), (ULONG)hashObj.size(), NULL, 0, 0) != 0) {
                throw std::runtime_error("Failed to create hash");
            }
        }
        ~BCryptHashContext() {
            if (hHash) BCryptDestroyHash(hHash);
            if (hAlg) BCryptCloseAlgorithmProvider(hAlg, 0);
        }
        void update(const uint8_t* data, size_t length) {
            if (length > 0) {
                if (BCryptHashData(hHash, (PUCHAR)data, (ULONG)length, 0) != 0) {
                    throw std::runtime_error("Failed to hash data");
                }
            }
        }
        std::vector<uint8_t> finish() {
            if (BCryptFinishHash(hHash, hash.data(), (ULONG)hash.size(), 0) != 0) {
                throw std::runtime_error("Failed to finish hash");
            }
            return hash;
        }
    private:
        BCRYPT_ALG_HANDLE hAlg = NULL;
        BCRYPT_HASH_HANDLE hHash = NULL;
        std::vector<uint8_t> hashObj;
        std::vector<uint8_t> hash;
    };
}

std::string sha256_data(const uint8_t* data, size_t length) {
    BCryptHashContext ctx;
    ctx.update(data, length);
    return to_hex(ctx.finish());
}

std::string sha256_file(const std::string& filepath) {
    std::ifstream file(filepath, std::ios::binary);
    if (!file) {
        throw std::runtime_error("Failed to open file for hashing");
    }
    BCryptHashContext ctx;
    std::vector<char> buffer(65536);
    while (file.read(buffer.data(), buffer.size())) {
        ctx.update(reinterpret_cast<const uint8_t*>(buffer.data()), file.gcount());
    }
    if (file.gcount() > 0) {
        ctx.update(reinterpret_cast<const uint8_t*>(buffer.data()), file.gcount());
    }
    return to_hex(ctx.finish());
}
