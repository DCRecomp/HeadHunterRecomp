// SPDX-License-Identifier: MIT
// Derivado de recomp-BStinger transitional/pad_contract.cpp (MIT, Copyright (c) 2026 Blue Stinger
// Recompiled contributors; ver third_party/licenses/recomp-BStinger-LICENSE.txt).
#include "pad_contract.h"

#include <cstring>

namespace hh::transitional::pad {
namespace {

template <typename T>
T get(const uint8_t* p, int off) {
    T v;
    std::memcpy(&v, p + off, sizeof v);
    return v;
}
template <typename T>
void put(uint8_t* p, int off, T v) {
    std::memcpy(p + off, &v, sizeof v);
}

} // namespace

Pds pds_load(const uint8_t* p) {
    Pds s;
    s.id      = get<uint32_t>(p, 0x00);
    s.support = get<uint32_t>(p, 0x04);
    s.on      = get<uint32_t>(p, 0x08);
    s.off     = get<uint32_t>(p, 0x0C);
    s.press   = get<uint32_t>(p, 0x10);
    s.release = get<uint32_t>(p, 0x14);
    s.r       = get<uint16_t>(p, 0x18);
    s.l       = get<uint16_t>(p, 0x1A);
    s.x1      = get<int16_t>(p, 0x1C);
    s.y1      = get<int16_t>(p, 0x1E);
    s.x2      = get<int16_t>(p, 0x20);
    s.y2      = get<int16_t>(p, 0x22);
    s.name    = get<uint32_t>(p, 0x24);
    s.extend  = get<uint32_t>(p, 0x28);
    s.old     = get<uint32_t>(p, 0x2C);
    s.info    = get<uint32_t>(p, 0x30);
    return s;
}

void pds_store(uint8_t* p, const Pds& s) {
    put(p, 0x00, s.id);
    put(p, 0x04, s.support);
    put(p, 0x08, s.on);
    put(p, 0x0C, s.off);
    put(p, 0x10, s.press);
    put(p, 0x14, s.release);
    put(p, 0x18, s.r);
    put(p, 0x1A, s.l);
    put(p, 0x1C, s.x1);
    put(p, 0x1E, s.y1);
    put(p, 0x20, s.x2);
    put(p, 0x22, s.y2);
    put(p, 0x24, s.name);
    put(p, 0x28, s.extend);
    put(p, 0x2C, s.old);
    put(p, 0x30, s.info);
}

// 8C15DBC2..DBD8: subunidade pelo resto de i por 6 (0x8C123C94); controle so com (flags & 0x21),
// erro 0 e resposta 8. O 1.50 nao tem o ramo do teclado do 1.07a.
Kind classify(int unit, const Unit& u) {
    if (unit % 6 != 0) return Kind::Subunit;
    if ((u.infoFlags & 0x21) && u.error == 0 && u.response == 8) return Kind::Controller;
    return Kind::Absent;
}

// 8C15DB06: 0xFC -> 0xFF0000FC; 0xFF, 8, 5, 6, 0 -> 0; demais -> 0xFF000000.
uint32_t status_code(uint8_t response) {
    switch (response) {
    case 0: case 5: case 6: case 8: case 0xFF: return 0;
    case 0xFC: return 0xFF0000FCu;
    default: return 0xFF000000u;
    }
}

static void decodeImpl(Pds& p, const uint8_t cond[8], uint32_t mode, Triggers& t, uint32_t mutations) {
    uint32_t on = 0xFFFF0000u | uint32_t(cond[0]) | uint32_t(cond[1]) << 8;  // ativo em 0
    const uint8_t high = (mutations & kMutThresholdOff1) ? uint8_t(t.high - 1) : t.high;
    // 8C15D924..D998 (L, byte 3) e 8C15D9A2..D9D0 (R, byte 2): histerese entre os limiares
    auto trigger = [&](uint8_t v, uint8_t& latch, uint32_t bit) {
        if (v >= high) {
            on &= ~bit;
            latch = 1;
        } else if (v <= t.low) {
            on |= bit;
            latch = 0;
        } else if (latch) {
            on &= ~bit;
        }
    };
    trigger(cond[3], t.latchL, 0x20000u);
    trigger(cond[2], t.latchR, 0x10000u);
    if (mode == 0) on = ~on;  // 8C15D9D2..D9DC
    if (mutations & kMutSwapAB) {
        const uint32_t a = on & 0x4, b = on & 0x2;
        on                = (on & ~0x6u) | (a >> 1) | (b << 1);
    }
    p.on = on;
    p.r  = (p.support & 0x10000) ? cond[2] : 0;
    p.l  = (p.support & 0x20000) ? cond[3] : 0;
    p.x1 = (p.support & 0x40000) ? int16_t(int(cond[4]) - 128) : 0;
    p.y1 = (p.support & 0x80000) ? int16_t(int(cond[5]) - 128) : 0;
    p.x2 = (p.support & 0x100000) ? int16_t(int(cond[6]) - 128) : 0;
    p.y2 = (p.support & 0x200000) ? int16_t(int(cond[7]) - 128) : 0;
}

void decode_controller(Pds& p, const uint8_t cond[8], uint32_t mode, Triggers& t) {
    decodeImpl(p, cond, mode, t, kMutNone);
}

void edges(Pds& p, uint32_t mode) {
    p.off            = ~p.on;
    const uint32_t t = p.on ^ p.old;
    if (mode == 0) {
        p.press   = t & p.on;
        p.release = t & p.off;
    } else {
        p.press   = ~t | p.on;
        p.release = ~t | p.off;
    }
}

void clear_absent(Pds& p, uint32_t mode) {
    p.id = p.support = 0;
    if (mode == 0) p.on = 0, p.off = ~0u, p.press = 0, p.release = 0;
    else p.on = ~0u, p.off = 0, p.press = ~0u, p.release = ~0u;
    p.r = p.l = 0;
    p.x1 = p.y1 = p.x2 = p.y2 = 0;
    p.extend = 0;
    p.name   = kNoDeviceName;
}

Pds server_unit(int unit, const Pds& before, const Unit& u, uint32_t mode, Triggers& t, uint32_t mutations,
                uint32_t nativeOld) {
    Pds p  = before;
    p.info = u.info;
    p.old  = (mutations & kMutOldNative) ? nativeOld : before.on;
    switch (classify(unit, u)) {
    case Kind::Subunit:
        p.name = u.info + 20;
        return p;
    case Kind::Controller:
        p.support = u.support;
        p.id      = u.support;
        p.name    = u.info + 20;
        decodeImpl(p, u.cond, mode, t, mutations);
        break;
    case Kind::Absent:
        clear_absent(p, mode);
        break;
    }
    edges(p, mode);
    return p;
}

void controller_info(uint8_t out[kInfoBytes], uint32_t functionData, uint32_t mutations) {
    std::memset(out, 0, kInfoBytes);
    out[0] = (mutations & kMutInfoNoConnect) ? 0 : 1;
    std::memcpy(out + 4, &functionData, 4);
    out[0x10] = 1;
    out[0x11] = 0xFF;
    auto padded = [&](int at, const char* text, int width) {
        std::memset(out + at, ' ', size_t(width));
        std::memcpy(out + at, text, std::strlen(text));
    };
    padded(0x14, "Dreamcast Controller", 30);
    padded(0x34, "Produced By or Under License From SEGA ENTERPRISES,LTD.", 60);
    const uint16_t standby = 0x01AE, maxPower = 0x01F4;
    std::memcpy(out + 0x74, &standby, 2);
    std::memcpy(out + 0x76, &maxPower, 2);
}

} // namespace hh::transitional::pad
