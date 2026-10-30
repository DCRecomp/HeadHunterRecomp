// SPDX-License-Identifier: MIT
#pragma once
// HH15.7d4a: o pedido de render (STARTRENDER) como servico nativo, com a conta e a ordem do modelo anterior
// (pvr/pvr_regs.cpp ate o HH15.7d3): a contagem, o gancho e a parada pedida (HH_STOP_ON_STARTRENDER), o diagnostico
// HH_TA_DUMP, o render inteiro na hora e o prazo do fim de render, min(450000 + bytes do TA * 100, 1500000) (a mesma
// estimativa do Flycast, spg.cpp scheduleRenderDone; aproximacao), avisado ao provedor de tempo. O fim pendente e
// consumido pelo banco de status quando ISTNRM 0/1/2 acendem. Usado pelo provedor nativo do Kamui (o render pela IR do
// ListSink) e pelo modelo dos registradores do PVR (o caminho do TA, so na verificacao).
#include "cpu/sh4_context.h"

#include <cstddef>
#include <cstdint>

namespace hh {

void render_bus_reset();  // nenhum fim de render pendente (a contagem e do processo)

// STARTRENDER com `render` desenhando as listas atuais; `taBytes` define o prazo do fim. `params` e `dumpParams`
// servem o HH_TA_DUMP (a contagem e as linhas dos parametros, por render_bus_dump_param); `bank` e o banco de
// registradores impresso (0x005F8000-0x005F9FFF, 2048 palavras).
void render_bus_start(Sh4Context* c, uint32_t taBytes, size_t params, void (*render)(Sh4Context*),
                      void (*dumpParams)(), const uint32_t* bank);
void render_bus_dump_param(uint32_t list, const uint32_t* w, uint32_t words);  // "[render]   L<lista> <palavras>"
int  render_bus_count();                                                       // STARTRENDERs desde o inicio

// fim de render pendente (instante em que ISTNRM 0/1/2 acendem) ou 0 se nenhum; o banco de status consome
uint64_t render_bus_done_due();
void     render_bus_done_take();

// diagnostico/testes: chamado a cada STARTRENDER, antes do render (n = numero do pedido, a partir de 1); pode lancar
// (checkpoint do hh-diff)
using RenderStartHook = void (*)(Sh4Context* c, int n);
void render_bus_set_start_hook(RenderStartHook hook);

} // namespace hh
