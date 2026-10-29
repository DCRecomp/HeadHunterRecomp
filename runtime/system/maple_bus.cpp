// SPDX-License-Identifier: MIT
// HH15.4: barramento de perifericos sem dispositivos (ver maple_bus.h). O efeito e o do modelo maple/maple.cpp sem
// controle (o caso do --controles nativo), conferido pelas referencias da HH15 e pelos portoes do HH10/HH11.
#include "system/maple_bus.h"

#include "cpu/sh4_runtime.h"
#include "cpu/time_hooks.h"
#include "debug/clock_log.h"
#include "input/input.h"
#include "pvr/holly_intc.h"

#include <array>
#include <cstring>

namespace hh {
namespace {

constexpr uint32_t kMdstar = 0x005F6C04, kMdtsel = 0x005F6C10, kMden = 0x005F6C14, kMdst = 0x005F6C18;
constexpr uint32_t kBase = 0x005F6C00, kSize = 0x100;
constexpr uint8_t  kCmdDevInfo = 0x01, kCmdGetCond = 0x09;

std::array<uint32_t, kSize / 4> g_regs{};
uint64_t      g_due = 0;
MapleCounters g_counters;
bool          g_transitionArmed = false;  // o primeiro acesso ao MDST depois do VBlank-In avanca a transicao do roteiro

uint32_t& reg(uint32_t p) { return g_regs[(p - kBase) / 4]; }

uint8_t* ramAt(Sh4Context* c, uint32_t phys, uint32_t bytes) {
    phys &= 0x1FFFFFFF;
    if ((phys >> 26) != 3 || (phys & 0x00FFFFFF) + bytes > (16u << 20))
        sh4_fatal(c, "barramento de perifericos: DMA fora da RAM", c->pc, phys);
    return c->ram + (phys & 0x00FFFFFF);
}

uint32_t rd(Sh4Context* c, uint32_t phys) {
    uint32_t v;
    std::memcpy(&v, ramAt(c, phys, 4), 4);
    return v;
}

void runDma(Sh4Context* c) {
    uint32_t addr = reg(kMdstar) & 0x1FFFFFE0;
    uint64_t bits = 0;
    ++g_counters.dmas;
    for (int n = 0;; ++n) {
        if (n == 1024) sh4_fatal(c, "barramento de perifericos: tabela sem descritor final (LAST)", c->pc, reg(kMdstar));
        const uint32_t instr = rd(c, addr), recv = rd(c, addr + 4);
        const uint32_t port = (instr >> 16) & 3, pattern = (instr >> 8) & 7, len = instr & 0xFF;
        if (pattern != 0) sh4_fatal(c, "barramento de perifericos: padrao de transferencia fora do contrato", c->pc, instr);
        const uint8_t cmd = rd(c, addr + 8) & 0xFF;
        (cmd == kCmdDevInfo ? g_counters.devInfo : cmd == kCmdGetCond ? g_counters.getCond : g_counters.other)[port]++;
        std::memset(ramAt(c, recv, 4), 0xFF, 4);  // nenhum dispositivo responde
        bits += (1 + len + 1) * 32ull;
        addr += 12 + 4 * len;
        if (instr & 0x80000000) break;
    }
    g_due = time_now(c) + bits * 100;
    if (g_due == 0) g_due = 1;
    time_provider().maple_scheduled(g_due);
}

uint32_t canon(uint32_t a) { return a < 0xE0000000 ? (a & 0x1FFFFFFF) : 0; }

} // namespace

void maple_bus_reset() {
    g_regs.fill(0);
    g_due = 0;
    time_provider().maple_cancel();
    g_counters        = {};
    g_transitionArmed = false;
}

bool maple_bus_read(Sh4Context* c, uint32_t a, uint32_t& v) {
    const uint32_t p = canon(a);
    if (p < kBase || p >= kBase + kSize) return false;
    v = reg(p);
    if (p == kMdst) {
        if (g_transitionArmed) g_transitionArmed = false, input_transition_tick();
        v = (g_due && time_now(c) < g_due) ? 1 : 0;
        if (g_clock_log) clock_log_read(ClockRead::Mdst, v);
    }
    return true;
}

bool maple_bus_write(Sh4Context* c, uint32_t a, uint32_t v) {
    const uint32_t p = canon(a);
    if (p < kBase || p >= kBase + kSize) return false;
    if (p == kMdtsel && (v & 1)) sh4_fatal(c, "barramento de perifericos: disparo por VBlank (MDTSEL=1) fora do contrato", c->pc, v);
    if (p == kMdst) {
        if (g_transitionArmed) g_transitionArmed = false, input_transition_tick();
        if (g_due && time_now(c) >= g_due) {  // fim anterior ainda nao visto pelo controlador
            holly_intc_raise(kIstMapleDma);
            g_due = 0;
            time_provider().maple_cancel();
        }
        if ((v & 1) && (reg(kMden) & 1) && !g_due) runDma(c);
        return true;
    }
    reg(p) = v;
    return true;
}

void          maple_bus_vblank_in() { g_transitionArmed = true; }
void          maple_bus_dma_take() { g_due = 0; }
uint64_t      maple_bus_dma_due() { return g_due; }
MapleCounters maple_bus_counters() { return g_counters; }
void          maple_bus_counters_reset() { g_counters = {}; }

} // namespace hh
