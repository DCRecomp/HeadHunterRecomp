// SPDX-License-Identifier: MIT
// Derivado de recomp-BStinger runtime/cpu/sh4_runtime.cpp (MIT, Copyright (c) 2026 Blue Stinger
// Recompiled contributors; ver third_party/licenses/recomp-BStinger-LICENSE.txt).
// Pontos de contato do codigo gerado com o hardware temporario (HH05): poll de eventos,
// esperas e sleep. A versao sem hardware do HH04 fica em sh4_hw_m3.cpp (so testes).
#include "cpu/sh4_intc.h"
#include "cpu/sh4_runtime.h"
#include "cpu/sh4_threads.h"
#include "cpu/time_hooks.h"
#include "gdrom/gdrom.h"
#include "pvr/holly_intc.h"

#include <cstdlib>
#include <string>

namespace hh {

void sh4_sleep(Sh4Context* c) { sh4_service(c); }

// Poll periodico (HH_POLL nos back-edges): reavalia interrupcoes a cada quantum de
// ciclos. c->pc ja e o endereco de retomada.
void sh4_wait_point(Sh4Context* c, uint32_t pc) {
    // N2b-2: se o instante logico andou, o poll que segue o ponto (HH_WAIT) atende os eventos
    // (next_event = 0 forca o atendimento, como no fim do DMA do canal 2)
    if (time_provider().wait(c, pc, uint32_t(sh4_current_thread()))) c->next_event = 0;
}

void sh4_wait_end(Sh4Context* c, uint32_t pc) {
    time_provider().wait_end(pc, uint32_t(sh4_current_thread()), c->polls);
}

namespace {
Sh4ServiceHook g_serviceHook = nullptr;
bool           g_logical     = false;    // HH15.1: cadencia B (tempo logico)
Sh4Context*    g_session     = nullptr;  // contexto da sessao (despertar pedido fora do codigo gerado)
bool           g_woken       = false;    // o proximo atendimento foi pedido por sh4_irq_wakeup (diagnostico)
// HH15.1 (prova de independencia dos ciclos): HH_CICLOS_RUIDO=<n> soma ao contador de ciclos um valor pseudoaleatorio
// em [0, n) a cada atendimento e a cada tick. No tempo logico nada pode mudar; no tempo por ciclos (controle) muda.
const uint64_t g_noise = [] { const char* v = std::getenv("HH_CICLOS_RUIDO"); return v ? std::strtoull(v, nullptr, 10) : 0ull; }();
uint64_t       g_noiseState = 0x9E3779B97F4A7C15ull;
void addNoise(Sh4Context* c) {
    if (!g_noise) return;
    g_noiseState = g_noiseState * 6364136223846793005ull + 1442695040888963407ull;
    c->cycles += (g_noiseState >> 33) % g_noise;
}
} // namespace

void sh4_set_service_hook(Sh4ServiceHook fn) { g_serviceHook = fn; }

void sh4_set_logical_cadence(bool on) {
    // diagnostico (HH15.1, referencia A/B): HH_CADENCIA=quantum mantem o atendimento pelo quantum de ciclos no tempo logico
    static const bool quantum = [] { const char* v = std::getenv("HH_CADENCIA"); return v && std::string(v) == "quantum"; }();
    g_logical = on && !quantum;
    on        = g_logical;
    if (g_session) g_session->next_poll = on ? g_session->polls + kProgressTick : UINT64_MAX;
}
bool sh4_logical_cadence() { return g_logical; }
void sh4_set_session_context(Sh4Context* c) {
    g_session = c;
    if (c) c->next_poll = g_logical ? c->polls + kProgressTick : UINT64_MAX;
}

void sh4_irq_wakeup() {
    Sh4Context* c = g_session;
    if (!g_logical || !c || c->next_event == 0) return;
    const uint32_t sr = sh4_get_sr(*c);
    if (sr & kSrBL) return;  // dentro de um tratador: o laco de sh4_irq_check reavalia depois do rte
    if (holly_irl_level_now() <= ((sr & kSrImask) >> 4)) return;  // mascarada: HH_IRQ_CHECK entrega no ldc sr
    c->next_event = 0;
    g_woken       = true;
}

void sh4_poll_slow(Sh4Context* c) {
    if (c->cycles >= c->next_event) return sh4_service(c);
    if (!g_logical) {  // contexto zerado (sh4_reset): no tempo por ciclos o tick nao existe
        c->next_poll = UINT64_MAX;
        return;
    }
    // tick de vivacidade (D8) e parada pedida pelo host, sem atendimento: nada muda no guest
    c->next_poll = c->polls + kProgressTick;
    addNoise(c);
    if (g_serviceHook) g_serviceHook(c);
    time_provider().progress(uint32_t(sh4_current_thread()), c->polls, c->pc);
}

void sh4_service(Sh4Context* c) {
    // HH15.1 (diagnostico): origem das entregas deste atendimento (o tratador aninhado pode atender de novo: a origem
    // e restaurada na saida)
    const IrqOrigin outer =
        sh4_irq_set_origin(g_woken && c->next_event == 0 ? IrqOrigin::Wakeup : sh4_irq_service_origin(c->next_event == 0));
    g_woken = false;
    addNoise(c);
    struct Restore {
        IrqOrigin o;
        ~Restore() { sh4_irq_set_origin(o); }
    } restore{outer};
    // HH15.1: no tempo logico o proximo atendimento so vem de um pedido (o quantum de ciclos nao conta)
    c->next_event = g_logical ? UINT64_MAX : c->cycles + kServiceQuantum;
    if (g_logical) c->next_poll = c->polls + kProgressTick;
    if (g_serviceHook) g_serviceHook(c);  // pode lancar (parada pedida pelo host)
    time_provider().progress(uint32_t(sh4_current_thread()), c->polls, c->pc);
    // transferencia do GD-ROM em andamento (perfil de tempo reios): avanca com o tempo,
    // sem depender de nova syscall (revisao Codex 2026-10-14)
    gdrom_poll(c);
    // eventos do Holly/SPG (VBlank: video e ritmo do host) avancam com o tempo mesmo com
    // interrupcoes bloqueadas (SR.BL); a entrega para a CPU continua em sh4_irq_check
    holly_intc_sync(time_now(c));
    sh4_irq_check(c);
}

} // namespace hh
