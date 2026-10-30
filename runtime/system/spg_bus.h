// SPDX-License-Identifier: MIT
#pragma once
// HH15.7d3: a temporizacao do video (SPG) como servico nativo (native/render/kamui/spg_timing, as contas do modelo
// anterior), unica fonte do SPG para o tempo (cpu/time_legacy, transitional/time_adapter), para o provedor nativo do
// Kamui (escritas pelo funil e o SPG_STATUS lido) e para o modelo dos registradores do PVR (pvr/pvr_regs.cpp, que
// delega a ele os registradores 0x0C8-0x0E0 e o SPG_STATUS). Contrato do guest medido na rota nativa (HH15.7, passo 0):
// leituras de SPG_STATUS (0x10C) e SPG_VBLANK (0x0DC); escritas 0x0C8-0x0E0 (pelo funil e por 0x8C169FE0).
// Deslocamentos no banco grafico (0x0000-0x1FFF).
#include "cpu/sh4_context.h"

#include <cstdint>

namespace hh {

constexpr uint64_t kSpgNever     = ~0ull;
constexpr uint32_t kSpgStatusOff = 0x10C;  // SPG_STATUS (so leitura, calculado no instante)

void spg_bus_reset();
// o estado deixado pelo boot: `bank` e o banco grafico inteiro (2048 palavras); fixa a fase do feixe pelo SPG_STATUS
void spg_bus_load(const uint32_t* bank);
bool spg_bus_holds(uint32_t off);  // registrador guardado pelo servico (0x0C8-0x0E0)
// leitura do guest ou do hardware (SPG_STATUS no instante atual; os outros, o valor guardado)
uint32_t spg_bus_read(Sh4Context* c, uint32_t off);  // off: 0x0C8-0x0E0 ou kSpgStatusOff
uint32_t spg_bus_reg(uint32_t off);  // o valor guardado (0x0C8-0x0E0), sem instante
// escrita (0x0C8-0x0E0): SPG_LOAD e SPG_VBLANK_INT entregam os eventos vencidos com a configuracao anterior antes do
// valor novo e avisam o provedor de tempo depois (nova temporizacao), como no modelo anterior
void spg_bus_write(Sh4Context* c, uint32_t off, uint32_t v);

// as contas do tempo (instantes do provedor de tempo)
bool     spg_crossed(uint64_t from, uint64_t to, uint32_t line);
uint64_t spg_next_crossing(uint64_t after, uint32_t line);  // kSpgNever se o campo nunca chega a linha
uint64_t spg_next_line_change(uint64_t after);
uint32_t spg_vblank_in_line();
uint32_t spg_vblank_out_line();

} // namespace hh
