// SPDX-License-Identifier: MIT
#include "hh_extract/ip_bin.hpp"

#include "hh_extract/cdi.hpp"

#include <cctype>

namespace hh {

namespace {

std::string field(const uint8_t* p, size_t off, size_t len) {
    std::string s;
    s.reserve(len);
    for (size_t i = 0; i < len; ++i) {
        const uint8_t c = p[off + i];
        s += (c >= 0x20 && c < 0x7F) ? char(c) : '?';
    }
    while (!s.empty() && s.back() == ' ') s.pop_back();
    return s;
}

} // namespace

uint32_t IpBin::peripheralsMask() const {
    uint32_t v = 0;
    for (char c : peripherals) {
        const int d = std::isxdigit(static_cast<unsigned char>(c)) ? std::tolower(c) - '0'
                                                                   : -1;
        if (d < 0) return 0;
        v = v * 16 + uint32_t(d > 9 ? d - ('a' - '0' - 10) : d);
    }
    return v;
}

bool IpBin::usesWindowsCE() const {
    return (peripheralsMask() & 1) != 0;
}

IpBin parseIpBin(const uint8_t* data, size_t size) {
    if (size < 0xC00) {
        throw DiscError("IP.BIN truncado (" + std::to_string(size) + " bytes; esperado >= 3072)");
    }
    IpBin ip;
    ip.hardware_id = field(data, 0x00, 16);
    ip.maker_id = field(data, 0x10, 16);
    ip.device_info = field(data, 0x20, 16);
    ip.area = field(data, 0x30, 8);
    ip.peripherals = field(data, 0x38, 8);
    ip.product = field(data, 0x40, 10);
    ip.version = field(data, 0x4A, 6);
    ip.release_date = field(data, 0x50, 16);
    ip.boot_file = field(data, 0x60, 16);
    ip.company = field(data, 0x70, 16);
    ip.title = field(data, 0x80, 128);
    if (ip.hardware_id != "SEGA SEGAKATANA") {
        throw DiscError("IP.BIN invalido: hardware_id '" + ip.hardware_id + "'");
    }
    for (char c : ip.peripherals) {
        if (!std::isxdigit(static_cast<unsigned char>(c))) {
            throw DiscError("perifericos IP.BIN invalidos: '" + ip.peripherals + "'");
        }
    }
    return ip;
}

} // namespace hh
