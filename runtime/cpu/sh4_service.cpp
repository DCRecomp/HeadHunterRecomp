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
} // namespace

void sh4_set_service_hook(Sh4ServiceHook fn) { g_serviceHook = fn; }

void sh4_service(Sh4Context* c) {
    c->next_event = c->cycles + kServiceQuantum;
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
