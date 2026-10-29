// SPDX-License-Identifier: MIT
#pragma once
// HH15.4 (docs/planos/HH15-PLANO.md; modelo: system/maple_bus do N8.4 do Blue Stinger): barramento de perifericos sem
// dispositivos. Com os controles e os saves nativos (o padrao do produto), o modelo do Maple (maple/maple.cpp) nao
// tinha nenhum dispositivo a responder: a camada per do SDK so sonda as portas a cada VBlank e o fim do DMA solta a
// passada dela (servidor bu, espera 0x8C118346). O barramento atende exatamente isso, com o mesmo efeito do modelo sem
// controle: registradores SB_MDSTAR/MDTSEL/MDEN/MDST e os demais guardados; o DMA segue a tabela ate o descritor LAST,
// grava 0xFFFFFFFF (sem dispositivo) em cada buffer de recepcao e agenda o fim em 100 ciclos de 200 MHz por bit
// (o mesmo do modelo); ISTNRM bit 12 no fim. Contadores iguais aos do modelo (prova de fechamento do HH10).
// Fora do contrato (padrao de transferencia diferente de 0, tabela sem LAST, DMA fora da RAM, disparo por VBlank
// MDTSEL=1) e erro explicito.
#include "cpu/sh4_context.h"
#include "maple/maple.h"

namespace hh {

void          maple_bus_reset();
bool          maple_bus_read(Sh4Context* c, uint32_t a, uint32_t& v);
bool          maple_bus_write(Sh4Context* c, uint32_t a, uint32_t v);
void          maple_bus_vblank_in();
void          maple_bus_dma_take();
uint64_t      maple_bus_dma_due();
MapleCounters maple_bus_counters();
void          maple_bus_counters_reset();

} // namespace hh
