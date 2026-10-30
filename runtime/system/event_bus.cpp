// SPDX-License-Identifier: MIT
// Derivado de recomp-BStinger runtime/system/event_bus.cpp (MIT, Copyright (c) 2026 Blue Stinger
// Recompiled contributors; ver third_party/licenses/recomp-BStinger-LICENSE.txt).
// HH15.7d2 (docs/planos/HH15-PLANO.md): no produto, o controlador de interrupcoes do Holly (pvr/holly_intc.h) e o banco
// minimo de status nativo (native/render/kamui/event_status), alimentado pelos eventos que o provedor de tempo entrega
// nos pontos de sempre (acesso, poll, nivel IRL). O tratador original do Kamui le e limpa o banco como antes. O
// controlador anterior (pvr/holly_intc.cpp, papel intc) fica na verificacao como referencia, com este banco em sombra.
// Compilado so no produto (app/CMakeLists.txt), como os stubs: define a mesma API do controlador anterior.
#include "pvr/holly_intc.h"

#include "cpu/sh4_runtime.h"
#include "cpu/time_hooks.h"
#include "debug/clock_log.h"
#include "debug/event_trace.h"
#include "maple/maple.h"
#include "pvr/pvr_regs.h"
#include "pvr/pvr_video.h"
#include "render/kamui/event_status.h"

#include <cstdio>
#include <cstdlib>

namespace hh {
namespace {

using native::render::kamui::EventStatus;

constexpr uint32_t kBase = 0x005F6900;

EventStatus g_bank;
uint64_t    g_last     = 0;
uint64_t    g_vblankAt = 0;  // instante do ultimo VBlank-In entregue (diagnostico HH_TMU_DIAG)

uint32_t canon(uint32_t a) { return a < 0xE0000000 ? (a & 0x1FFFFFFF) : 0; }

void update(uint64_t now) {
    // no tempo experimental (N2b-2) o instante fica parado entre dois atendimentos: um evento vencido no mesmo instante
    // ainda e entregue
    if (now < g_last || (now == g_last && !time_provider().holly_pending(now))) return;
    const uint32_t   due = time_provider().holly_due(now);
    EventStatus::Due d;
    d.vblank_in = due & kDueVblankIn, d.vblank_out = due & kDueVblankOut, d.render = due & kDueRender;
    d.maple     = due & kDueMaple;
    g_bank.deliver(d);
    if (d.vblank_in) {
        g_vblankAt = now;
        maple_vblank_in();  // HH10: arma a transicao injetada dos roteiros (como no controlador anterior)
        if (g_event_trace) event_trace(TraceEvent::VBlank, now, uint32_t(g_bank.vblanks()));
        if (g_clock_log) clock_log_vblank(g_bank.vblanks());
    }
    if (d.render) pvr_render_done_take();
    if (d.maple) maple_dma_take();
    g_last = now;
    // host (apresentacao de quadros, ritmo): depois de atualizar o estado, porque o gancho pode interromper a execucao
    if (d.vblank_in) pvr_video_vblank();
}

} // namespace

void holly_intc_sync(uint64_t cycles) { update(cycles); }

uint64_t    holly_vblank_count() { return g_bank.vblanks(); }
uint64_t    holly_last_vblank_instant() { return g_vblankAt; }
uint64_t    holly_vblank_fused() { return g_bank.fused(); }
uint64_t    holly_event_count() { return g_bank.events(); }
std::string holly_event_status_summary() { return {}; }  // a comparacao com o controlador anterior so na verificacao

void holly_intc_raise(uint32_t bits) {
    g_bank.raise(bits);
    sh4_irq_wakeup();  // HH15.1: cadencia B (so no tempo logico)
}

void holly_intc_set_external(uint32_t bits, bool on) {
    g_bank.set_external(bits, on);
    if (on) sh4_irq_wakeup();  // HH15.1: cadencia B (so no tempo logico)
}

uint32_t holly_irl_level(uint64_t cycles) {
    update(cycles);
    return holly_irl_level_now();
}

uint32_t holly_irl_level_now() { return g_bank.irl_level(); }

void holly_intc_reset(uint64_t cycles) {
    g_bank.reset();
    g_last     = cycles;
    g_vblankAt = cycles;
    time_provider().reset(cycles);
}

bool holly_intc_read(Sh4Context* c, uint32_t a, unsigned size, uint32_t& v) {
    const uint32_t p = canon(a);
    if (p < kBase || !EventStatus::contains(p - kBase) || size != 4) return false;
    update(time_now(c));
    v = g_bank.read(p - kBase);
    const uint32_t off = p - kBase;
    if (g_clock_log && (off == EventStatus::kIstNrm || off == EventStatus::kIstExt || off == EventStatus::kIstErr))
        clock_log_read(ClockRead::Ist, v);
    return true;
}

bool holly_intc_write(Sh4Context* c, uint32_t a, unsigned size, uint32_t v) {
    const uint32_t p = canon(a);
    if (p < kBase || !EventStatus::contains(p - kBase) || size != 4) return false;
    update(time_now(c));
    const uint32_t off = p - kBase;
    if (off == EventStatus::kIstNrm || off == EventStatus::kIstExt || off == EventStatus::kIstErr) {
        g_bank.write(off, v);  // ISTNRM/ISTERR: escrever 1 limpa; ISTEXT: so a fonte apaga
    } else {
        // Diagnostico: HH_PVR_DEBUG=1 registra mudancas das mascaras IML*
        static const bool debug = std::getenv("HH_PVR_DEBUG") != nullptr;
        if (debug && g_bank.reg(off) != v) std::fprintf(stderr, "[intc] %08X = %08X\n", p, v);
        g_bank.write(off, v);
        sh4_irq_wakeup();  // HH15.1: mascara IML* nova pode deixar um nivel entregavel (cadencia B)
    }
    return true;
}

} // namespace hh
