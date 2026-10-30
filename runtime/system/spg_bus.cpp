// SPDX-License-Identifier: MIT
// HH15.7d3: a temporizacao do video como servico nativo (system/spg_bus.h). O efeito e o do modelo pvr/pvr_regs.cpp de
// antes (que agora delega a ele).
#include "system/spg_bus.h"

#include "cpu/time_hooks.h"
#include "debug/clock_log.h"
#include "pvr/holly_intc.h"
#include "render/kamui/spg_timing.h"

static_assert(hh::kSpgStatusOff == hh::native::render::kamui::SpgTiming::kStatus);
static_assert(hh::kSpgNever == hh::native::render::kamui::SpgTiming::kNever);

namespace hh {
namespace {

using native::render::kamui::SpgTiming;

SpgTiming g_spg(kCompatTicksPerField);  // campo NTSC 59.94 Hz na base de tempo de compatibilidade

} // namespace

void spg_bus_reset() {
    g_spg.reset();
    time_provider().spg_changed();
}

void spg_bus_load(const uint32_t* bank) {
    g_spg.load(bank);
    time_provider().spg_changed();
}

bool spg_bus_holds(uint32_t off) { return SpgTiming::holds(off); }

uint32_t spg_bus_reg(uint32_t off) { return SpgTiming::holds(off) ? g_spg.reg(off) : 0; }

uint32_t spg_bus_read(Sh4Context* c, uint32_t off) {
    if (off != SpgTiming::kStatus) return spg_bus_reg(off);
    const uint32_t v = g_spg.status(time_now(c));
    if (g_clock_log) clock_log_read(ClockRead::Spg, v);
    return v;
}

void spg_bus_write(Sh4Context* c, uint32_t off, uint32_t v) {
    if (!SpgTiming::holds(off)) return;
    const bool timing = off == SpgTiming::kLoad || off == SpgTiming::kVblankInt;
    if (timing) holly_intc_sync(time_now(c));  // os eventos vencidos com a configuracao anterior
    g_spg.set(off, v);
    if (timing) time_provider().spg_changed();  // nova temporizacao do SPG
}

bool     spg_crossed(uint64_t from, uint64_t to, uint32_t line) { return g_spg.crossed(from, to, line); }
uint64_t spg_next_crossing(uint64_t after, uint32_t line) { return g_spg.next_crossing(after, line); }
uint64_t spg_next_line_change(uint64_t after) { return g_spg.next_line_change(after); }
uint32_t spg_vblank_in_line() { return g_spg.vblank_in_line(); }
uint32_t spg_vblank_out_line() { return g_spg.vblank_out_line(); }

} // namespace hh
