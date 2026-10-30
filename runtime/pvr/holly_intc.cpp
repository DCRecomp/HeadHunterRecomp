// SPDX-License-Identifier: MIT
// Derivado de recomp-BStinger runtime/pvr/holly_intc.cpp (MIT, Copyright (c) 2026 Blue Stinger
// Recompiled contributors; ver third_party/licenses/recomp-BStinger-LICENSE.txt).
// Registradores (documentacao do KallistiOS, referencia de comportamento):
//   ISTNRM 0x005F6900: status normal; escrever 1 limpa o bit. Bit 3 VBlank-In,
//                      bit 4 VBlank-Out.
//   ISTEXT 0x005F6904, ISTERR 0x005F6908: status externo/erro (ainda sem fontes).
//   IML2NRM..IML6ERR 0x005F6910-0x005F6938: mascaras por nivel (so guardadas).
//
// Os eventos sao avaliados nos pontos de sempre (acesso, poll, nivel IRL): o provedor de tempo
// (cpu/time_hooks.h) diz o que venceu desde o ultimo ponto.
//
// HH15.7d2 (papel intc, so a verificacao): este controlador e a referencia do banco de status nativo do produto
// (native/render/kamui/event_status, servido por system/event_bus.cpp), que roda aqui em sombra, recebe os mesmos
// eventos e escritas e e comparado depois de cada operacao (os 16 registradores, o valor lido do ISTNRM, o nivel IRL e
// as contagens). Negativo (HH_EVENT_STATUS_MUTACAO=limpa): a sombra ignora a limpeza do bit 19 (fim do DMA do canal 2).
#include "pvr/holly_intc.h"
#include "cpu/sh4_runtime.h"
#include "cpu/time_hooks.h"
#include "debug/clock_log.h"
#include "debug/event_trace.h"

#include "pvr/pvr_regs.h"
#include "maple/maple.h"
#include "pvr/pvr_video.h"
#include "system/render_bus.h"
#include "system/video_bus.h"
#include "render/kamui/event_status.h"

#include <array>
#include <cstdio>
#include <cstdlib>
#include <string>

namespace hh {
namespace {

constexpr uint32_t kBase   = 0x005F6900;
constexpr uint32_t kSize   = 0x40;
constexpr uint32_t kIstNrm = 0x005F6900;
constexpr uint32_t kIstExt = 0x005F6904;
constexpr uint32_t kIstErr = 0x005F6908;

std::array<uint32_t, kSize / 4> g_regs{};
uint64_t                        g_last = 0;

uint32_t& reg(uint32_t a) { return g_regs[(a - kBase) / 4]; }
uint32_t  canon(uint32_t a) { return a < 0xE0000000 ? (a & 0x1FFFFFFF) : 0; }

uint64_t g_vblanks = 0;  // contagem de VBlank-In (trace de ordem)
uint64_t g_vblankAt = 0;  // instante do ultimo VBlank-In entregue (diagnostico HH_TMU_DIAG)
uint64_t g_events = 0;  // eventos do Holly entregues (diagnostico N2b: epoca, mesmo mascarados)
uint64_t g_vblankFused = 0;  // VBlank-In com o bit ainda aceso: um atendimento para dois (N2b, B4)

native::render::kamui::EventStatus g_shadow;
uint64_t                           g_checks = 0, g_differ = 0;
std::string                        g_first;

bool mutation_noclear() {
    static const bool m = [] {
        const char* v = std::getenv("HH_EVENT_STATUS_MUTACAO");
        return v && std::string(v) == "limpa";
    }();
    return m;
}

uint32_t level_now() {
    const uint32_t nrm = reg(kIstNrm), ext = reg(kIstExt), err = reg(kIstErr);
    // IML6NRM 0x005F6930, IML4NRM 0x005F6920, IML2NRM 0x005F6910; EXT +4, ERR +8
    for (uint32_t level : {6u, 4u, 2u}) {
        const uint32_t m = 0x005F6910 + (level / 2 - 1) * 0x10;
        if ((nrm & reg(m)) || (ext & reg(m + 4)) || (err & reg(m + 8))) return level;
    }
    return 0;
}

uint32_t read_now(uint32_t p) {
    uint32_t v = reg(p);
    if (p == kIstNrm && reg(kIstExt)) v |= 1u << 30;  // HH14: resumo do ISTEXT (so com a fonte externa da AICA)
    return v;
}

void shadow_check(const char* what) {
    ++g_checks;
    bool same = g_shadow.vblanks() == g_vblanks && g_shadow.fused() == g_vblankFused && g_shadow.events() == g_events;
    for (uint32_t i = 0; i < kSize / 4 && same; ++i) same = g_shadow.reg(4 * i) == g_regs[i];
    same = same && g_shadow.read(0) == read_now(kIstNrm) && g_shadow.irl_level() == level_now();
    if (same) return;
    ++g_differ;
    if (g_first.empty()) {
        char b[160];
        std::snprintf(b, sizeof b, "%s (operacao %llu): ISTNRM legado %08X, nativo %08X; VBlanks %llu/%llu", what,
                      static_cast<unsigned long long>(g_checks), g_regs[0], g_shadow.reg(0),
                      static_cast<unsigned long long>(g_vblanks), static_cast<unsigned long long>(g_shadow.vblanks()));
        g_first = b;
    }
}

void update(uint64_t now) {
    // no tempo experimental (N2b-2) o instante fica parado entre dois atendimentos: um evento
    // vencido no mesmo instante ainda e entregue
    if (now < g_last || (now == g_last && !time_provider().holly_pending(now))) return;
    // N2a: os eventos agendados vencidos vem do provedor de tempo (legado: calculo preguicoso
    // por ciclos; nativo: fila de eventos), entregues aqui como antes
    const uint32_t due      = time_provider().holly_due(now);
    const bool     vblankIn = due & kDueVblankIn;
    if (due) ++g_events;
    if (vblankIn && (reg(kIstNrm) & kIstVblankIn)) ++g_vblankFused;
    if (vblankIn) reg(kIstNrm) |= kIstVblankIn;
    if (vblankIn) ++g_vblanks, g_vblankAt = now;
    // HH10: arma a transicao injetada dos roteiros "indice transicao" (e o negativo amostra_vblank_in);
    // sem esta chamada o indice ficava em 0 no HH (medido: 1 543 leituras, transicao 0)
    if (vblankIn) maple_vblank_in();
    if (vblankIn && g_event_trace) event_trace(TraceEvent::VBlank, now, uint32_t(g_vblanks));
    if (vblankIn && g_clock_log) clock_log_vblank(g_vblanks);
    if (due & kDueVblankOut) reg(kIstNrm) |= kIstVblankOut;
    // fim de render agendado pelo STARTRENDER: bits 0 (video), 1 (ISP), 2 (TSP)
    if (due & kDueRender) {
        reg(kIstNrm) |= 0x7;
        render_bus_done_take();
    }
    // fim do DMA do Maple: bit 12
    if (due & kDueMaple) {
        reg(kIstNrm) |= kIstMapleDma;
        maple_dma_take();
    }
    g_last = now;
    if (due) {
        native::render::kamui::EventStatus::Due d;
        d.vblank_in = vblankIn, d.vblank_out = due & kDueVblankOut, d.render = due & kDueRender;
        d.maple     = due & kDueMaple;
        g_shadow.deliver(d);
        shadow_check("eventos vencidos");
    }
    // host (apresentacao de quadros, ritmo): depois de atualizar o estado, porque o
    // gancho pode interromper a execucao (encerramento)
    if (vblankIn) video_bus_vblank();
}

} // namespace

void holly_intc_sync(uint64_t cycles) { update(cycles); }

uint64_t holly_vblank_count() { return g_vblanks; }
uint64_t holly_last_vblank_instant() { return g_vblankAt; }
uint64_t holly_vblank_fused() { return g_vblankFused; }

void holly_intc_raise(uint32_t bits) {
    reg(kIstNrm) |= bits;
    ++g_events;
    g_shadow.raise(bits);
    shadow_check("evento levantado");
    sh4_irq_wakeup();  // HH15.1: cadencia B (so no tempo logico)
}
uint64_t holly_event_count() { return g_events; }

std::string holly_event_status_summary() {
    return "[event-status] operacoes comparadas " + std::to_string(g_checks) + ", diferentes " +
           std::to_string(g_differ) + (g_first.empty() ? std::string() : "; primeira: " + g_first) + "\n";
}

uint32_t holly_irl_level(uint64_t cycles) {
    update(cycles);
    return holly_irl_level_now();
}

uint32_t holly_irl_level_now() { return level_now(); }

void holly_intc_set_external(uint32_t bits, bool on) {
    if (on) reg(kIstExt) |= bits;
    else reg(kIstExt) &= ~bits;
    g_shadow.set_external(bits, on);
    shadow_check("fonte externa");
    if (on) sh4_irq_wakeup();  // HH15.1: cadencia B (so no tempo logico)
}

void holly_intc_reset(uint64_t cycles) {
    g_regs.fill(0);
    g_last    = cycles;
    g_vblanks = 0;
    g_vblankAt = cycles;
    g_vblankFused = 0;
    g_shadow.reset();
    time_provider().reset(cycles);
}

bool holly_intc_read(Sh4Context* c, uint32_t a, unsigned size, uint32_t& v) {
    const uint32_t p = canon(a);
    if (p < kBase || p >= kBase + kSize || size != 4) return false;
    update(time_now(c));
    v = read_now(p);
    if (g_clock_log && (p == kIstNrm || p == kIstExt || p == kIstErr)) clock_log_read(ClockRead::Ist, v);
    return true;
}

bool holly_intc_write(Sh4Context* c, uint32_t a, unsigned size, uint32_t v) {
    const uint32_t p = canon(a);
    if (p < kBase || p >= kBase + kSize || size != 4) return false;
    update(time_now(c));
    if (p == kIstExt) {
        // nivel das fontes externas: so a fonte o apaga (HH14)
        g_shadow.write(p - kBase, v);
        shadow_check("escrita do ISTEXT");
    } else if (p == kIstNrm || p == kIstErr) {
        reg(p) &= ~v;  // escrever 1 limpa
        g_shadow.write(p - kBase, mutation_noclear() && p == kIstNrm ? v & ~(1u << 19) : v);
        shadow_check("limpeza");
    } else {
        // Diagnostico: HH_PVR_DEBUG=1 registra mudancas das mascaras IML*
        static const bool debug = std::getenv("HH_PVR_DEBUG") != nullptr;
        if (debug && reg(p) != v) std::fprintf(stderr, "[intc] %08X = %08X\n", p, v);
        reg(p) = v;
        g_shadow.write(p - kBase, v);
        shadow_check("mascara");
        sh4_irq_wakeup();  // HH15.1: mascara IML* nova pode deixar um nivel entregavel (cadencia B)
    }
    return true;
}

} // namespace hh
