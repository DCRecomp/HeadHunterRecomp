// SPDX-License-Identifier: MIT
// hashing streaming (SHA-256 e CRC32) para hh-extract.
// HH02 - implementado do zero conforme FIPS 180-4 e CRC-32 (IEEE 802.3).
#pragma once
#include <array>
#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace hh {

class Sha256 {
public:
    Sha256();
    void update(const uint8_t* data, size_t n);
    std::array<uint8_t, 32> digest();
    std::string hexdigest();

private:
    void transform(const uint8_t* block);
    std::array<uint32_t, 8> h_;
    std::array<uint8_t, 64> buf_;
    size_t buf_len_ = 0;
    uint64_t total_ = 0;
};

class Crc32 {
public:
    void update(const uint8_t* data, size_t n);
    uint32_t value() const; // com complemento final (CRC-32 IEEE)

private:
    uint32_t value_ = 0xFFFFFFFFu;
};

std::string hex32(uint32_t v);

} // namespace hh
