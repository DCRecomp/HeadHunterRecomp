// SPDX-License-Identifier: MIT
// Derivado de recomp-BStinger runtime/memory/sh4_dmac.cpp (MIT, Copyright (c) 2026 Blue Stinger
// Recompiled contributors; ver third_party/licenses/recomp-BStinger-LICENSE.txt).
// DMAC (manual SH7750, cap. 14): por canal n, SARn 0xFFA00000+0x10n, DARn +4,
// DMATCRn +8, CHCRn +0xC; DMAOR 0xFFA00040. CHCR: bit 0 DE (habilita), bit 1 TE (fim).
// DMAOR bit 0 DME (habilita o controlador). O canal 2 em modo DDT e alimentado pelo
// Holly: o boot do jogo grava SAR2 (origem na RAM) e CHCR2, e dispara por SB_C2DST.
// HH15.7d: o modelo e so da verificacao; o canal 2 e o DMAOR vivem no servico nativo (system/dma_bus, o do produto),
// entao as referencias do hh-run exercitam o codigo do produto. O modelo guarda os canais 0, 1 e 3 e, sem o destino
// nativo das listas (HH_KAMUI=legado ou sombra), entrega o canal 2 ao TA do runtime.
#include "memory/sh4_dmac.h"

#include "cpu/sh4_mem.h"
#include "cpu/sh4_runtime.h"
#include "pvr/ta.h"
#include "system/dma_bus.h"

namespace hh {
namespace {

constexpr uint32_t kBase = 0xFFA00000;
constexpr uint32_t kSar2 = 0xFFA00020, kDmatcr2 = 0xFFA00028, kChcr2 = 0xFFA0002C, kDmaor = 0xFFA00040;
constexpr uint32_t kDe = 1u << 0, kTe = 1u << 1, kDme = 1u << 0;

uint32_t g_ch[4][4] = {};  // SAR, DAR, DMATCR, CHCR dos canais 0, 1 e 3 (o 2 e do servico)

bool chanReg(uint32_t a, uint32_t*& r) {
    if (a < kBase || a >= kBase + 0x40 || (a & 3)) return false;
    r = &g_ch[(a - kBase) >> 4][((a - kBase) & 0xF) >> 2];
    return true;
}

uint32_t busReg(uint32_t a) {
    uint32_t v = 0;
    dma_bus_read(a, 4, v);
    return v;
}

} // namespace

void sh4_dmac_reset() {
    for (auto& ch : g_ch)
        for (auto& r : ch) r = 0;
    dma_bus_reset();
}

bool sh4_dmac_read(Sh4Context*, uint32_t a, unsigned size, uint32_t& v) {
    if (dma_bus_read(a, size, v)) return true;
    if (size != 4) return false;
    uint32_t* r = nullptr;
    if (!chanReg(a, r)) return false;
    v = *r;
    return true;
}

bool sh4_dmac_write(Sh4Context*, uint32_t a, unsigned size, uint32_t v) {
    if (dma_bus_write(a, size, v)) return true;
    if (size != 4) return false;
    uint32_t* r = nullptr;
    if (!chanReg(a, r)) return false;
    *r = v;
    return true;
}

bool sh4_dmac_ch2_transfer(Sh4Context* c, uint32_t dest, uint32_t len) {
    if (dma_bus_has_sink()) return dma_bus_ch2_transfer(c, dest, len);  // o caminho do produto
    if (!(busReg(kDmaor) & kDme) || !(busReg(kChcr2) & kDe)) return false;
    const bool fifo = (dest & 0x01800000) == 0;  // FIFO de poligonos: o endereco nao avanca
    uint32_t   src  = busReg(kSar2);
    for (uint32_t off = 0; off < len; off += 32, src += 32) {
        uint32_t blk[8];
        for (int i = 0; i < 8; ++i) blk[i] = sh4_read32(c, src + 4 * i);
        ta_note_via("dma");
        const bool ok = ta_fifo_write(c, fifo ? dest : dest + off, blk);
        ta_note_via("cpu");
        if (!ok)
            sh4_fatal(c, "DMA do canal 2 para destino fora da area 4", c->pc, dest);
    }
    dma_bus_write(kSar2, 4, src);
    dma_bus_write(kDmatcr2, 4, 0);
    dma_bus_write(kChcr2, 4, busReg(kChcr2) | kTe);
    return true;
}

} // namespace hh
