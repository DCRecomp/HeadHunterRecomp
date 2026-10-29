// SPDX-License-Identifier: MIT
// HH15.7d: o DMA do canal 2 como servico nativo (dma_bus.h). O efeito e o do modelo memory/sh4_dmac.cpp de antes (o
// canal 2 pelo SB_C2DST), que agora delega a ele o canal 2 e o DMAOR.
#include "system/dma_bus.h"

#include "cpu/sh4_mem.h"
#include "cpu/sh4_runtime.h"

namespace hh {
namespace {

constexpr uint32_t kSar2 = 0xFFA00020, kDar2 = 0xFFA00024, kDmatcr2 = 0xFFA00028, kChcr2 = 0xFFA0002C, kDmaor = 0xFFA00040;
constexpr uint32_t kDe = 1u << 0, kTe = 1u << 1, kDme = 1u << 0;

uint32_t g_sar2 = 0, g_dar2 = 0, g_dmatcr2 = 0, g_chcr2 = 0, g_dmaor = 0;
DmaSink  g_sink = nullptr;

uint32_t* reg(uint32_t a) {
    switch (a) {
    case kSar2: return &g_sar2;
    case kDar2: return &g_dar2;
    case kDmatcr2: return &g_dmatcr2;
    case kChcr2: return &g_chcr2;
    case kDmaor: return &g_dmaor;
    default: return nullptr;
    }
}

} // namespace

void dma_bus_reset() { g_sar2 = g_dar2 = g_dmatcr2 = g_chcr2 = g_dmaor = 0; }

bool dma_bus_read(uint32_t a, unsigned size, uint32_t& v) {
    uint32_t* r = size == 4 ? reg(a) : nullptr;
    if (!r) return false;
    v = *r;
    return true;
}

bool dma_bus_write(uint32_t a, unsigned size, uint32_t v) {
    uint32_t* r = size == 4 ? reg(a) : nullptr;
    if (!r) return false;
    *r = v;
    return true;
}

void dma_bus_set_sink(DmaSink sink) { g_sink = sink; }
bool dma_bus_has_sink() { return g_sink != nullptr; }

bool dma_bus_ch2_transfer(Sh4Context* c, uint32_t dest, uint32_t len) {
    if (!(g_dmaor & kDme) || !(g_chcr2 & kDe)) return false;
    if (!g_sink) sh4_fatal(c, "DMA do canal 2 sem o destino nativo das listas (provedor nativo do Kamui)", c->pc, dest);
    const bool fifo = (dest & 0x01800000) == 0;  // FIFO de poligonos: o endereco nao avanca
    uint32_t   src  = g_sar2;
    for (uint32_t off = 0; off < len; off += 32, src += 32) {
        uint32_t blk[8];
        for (int i = 0; i < 8; ++i) blk[i] = sh4_read32(c, src + 4 * i);
        if (!g_sink(c, fifo ? dest : dest + off, blk)) sh4_fatal(c, "DMA do canal 2 para destino fora da area 4", c->pc, dest);
    }
    g_sar2    = src;
    g_dmatcr2 = 0;
    g_chcr2 |= kTe;
    return true;
}

} // namespace hh
