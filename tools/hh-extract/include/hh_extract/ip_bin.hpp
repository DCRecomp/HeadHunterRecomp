// SPDX-License-Identifier: MIT
// IP.BIN: area de sistema dos primeiros 16 setores de usuario da faixa de
// boot (32 768 bytes). Campos de offsets fixos conforme a especificacao Katana.
#pragma once
#include <cstddef>
#include <cstdint>
#include <string>

namespace hh {

constexpr size_t kIpBinSize = 32768;
constexpr size_t kUserDataSize = 2048;

struct IpBin {
    std::string hardware_id;  // 0x00, 16
    std::string maker_id;     // 0x10, 16
    std::string device_info;  // 0x20, 16
    std::string area;         // 0x30, 8
    std::string peripherals;  // 0x38, 8 (hex)
    std::string product;      // 0x40, 10
    std::string version;      // 0x4A, 6
    std::string release_date; // 0x50, 16
    std::string boot_file;    // 0x60, 16
    std::string company;      // 0x70, 16
    std::string title;        // 0x80, 128

    bool usesWindowsCE() const; // bit 0 do mask de perifericos
    uint32_t peripheralsMask() const;
};

// Valida hardware_id ("SEGA SEGAKATANA") e hex dos perifericos.
// Lanca DiscError em IP.BIN invalido ou Windows CE.
IpBin parseIpBin(const uint8_t* data, size_t size);

} // namespace hh
