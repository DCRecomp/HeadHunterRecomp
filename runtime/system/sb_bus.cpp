// SPDX-License-Identifier: MIT
// HH15.7d: o system block do Holly como servico nativo (sb_bus.h). O efeito e o do modelo pvr/holly_sb.cpp de antes no
// contrato (que agora delega a ele).
#include "system/sb_bus.h"

#include "memory/sh4_dmac.h"
#include "pvr/holly_intc.h"

#include <array>

namespace hh {
namespace {

constexpr uint32_t kBase = 0x005F6800, kSize = 0x1800;
constexpr uint32_t kC2Dstat = 0x005F6800, kC2Dlen = 0x005F6804, kC2Dst = 0x005F6808;
constexpr uint32_t kFfst = 0x005F688C, kSbRev = 0x005F689C;
constexpr uint32_t kIstPvrDma = 1u << 19;

std::array<uint32_t, kSize / 4> g_regs{};

uint32_t  canon(uint32_t a) { return a < 0xE0000000 ? (a & 0x1FFFFFFF) : 0; }
uint32_t& reg(uint32_t p) { return g_regs[(p - kBase) / 4]; }

bool writable(uint32_t p) {
    return (p >= 0x005F6800 && p < 0x005F6900) || (p >= 0x005F6940 && p < 0x005F6A00) ||
           (p >= 0x005F7400 && p < 0x005F7500) ||
           (p >= 0x005F7800 && p < 0x005F7900) || (p >= 0x005F7C00 && p < 0x005F7D00);
}

} // namespace

void sb_bus_reset() { g_regs.fill(0); }

bool sb_bus_contains(uint32_t a) {
    const uint32_t p = canon(a);
    return p >= kBase && p < kBase + kSize && !(p >= 0x005F6900 && p < 0x005F6940);
}

bool sb_bus_read(Sh4Context*, uint32_t a, unsigned size, uint32_t& v) {
    const uint32_t p = canon(a);
    if (size != 4) return false;
    if (p == kSbRev) v = 0x0000000B;
    else if (p == kC2Dstat || p == kC2Dlen || p == kC2Dst || p == kFfst) v = reg(p);
    else return false;
    return true;
}

bool sb_bus_write(Sh4Context* c, uint32_t a, unsigned size, uint32_t v) {
    const uint32_t p = canon(a);
    if (size != 4 || !writable(p) || p == kSbRev) return false;
    reg(p) = v;
    // SB_C2DST = 1: DMA do canal 2 (SH-4 DMAC) de SB_C2DLEN bytes para SB_C2DSTAT. No fim, C2DSTAT avanca, C2DLEN e C2DST
    // zeram e ISTNRM bit 19 (fim do DMA do PVR) acende; a interrupcao e avaliada no proximo poll (next_event = 0).
    if (p == kC2Dst && (v & 1)) {
        uint32_t& dest = reg(kC2Dstat);
        uint32_t& len  = reg(kC2Dlen);
        if (sh4_dmac_ch2_transfer(c, dest, len)) {
            dest += len;
            len       = 0;
            reg(kC2Dst) = 0;
            holly_intc_raise(kIstPvrDma);
            c->next_event = 0;
        }
    }
    return true;
}

} // namespace hh
