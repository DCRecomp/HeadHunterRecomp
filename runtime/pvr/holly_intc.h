// SPDX-License-Identifier: MIT
// Derivado de recomp-BStinger runtime/pvr/holly_intc.h (MIT, Copyright (c) 2026 Blue Stinger
// Recompiled contributors; ver third_party/licenses/recomp-BStinger-LICENSE.txt).
#pragma once
// Controlador de interrupcoes do Holly (system block, 0x005F6900-0x005F693F).
// ISTNRM com os eventos de VBlank derivados do contador de ciclos e os eventos do PVR
// (fim de lista do TA, fim de render); as mascaras IML2/4/6 definem o nivel IRL.
#include "cpu/sh4_context.h"

namespace hh {

constexpr uint32_t kIstVblankIn  = 1u << 3;
constexpr uint32_t kIstVblankOut = 1u << 4;

// Acende bits de ISTNRM (eventos do PVR: fim de lista do TA, fim de render).
void holly_intc_raise(uint32_t bits);

void holly_intc_reset(uint64_t cycles = 0);
// HH14: nivel de uma fonte externa (ISTEXT; bit 1 = AICA) a partir do pedido da AICA (MCIPD & MCIEB) do provedor nativo
// de audio. Nivel, nao pulso: o reconhecimento e na fonte (MCIRE); ISTNRM bit 30 resume o ISTEXT. Sem fonte, nada muda.
void holly_intc_set_external(uint32_t bits, bool on);
constexpr uint32_t kIstExtAica = 1u << 1;

// Acende os eventos ate `cycles` com a configuracao de SPG atual. Chamado antes de
// qualquer escrita que mude a temporizacao, para nao aplicar a nova a tempo passado.
void holly_intc_sync(uint64_t cycles);
// VBlank-In desde o reset (diagnostico: trace de ordem, registro de leituras do N1).
uint64_t holly_vblank_count();
// Instante (provedor de tempo) em que o ultimo VBlank-In foi entregue, no ponto de poll (nao o
// cruzamento exato da linha). Diagnostico N2b: HH_TMU_DIAG.
uint64_t holly_last_vblank_instant();
// VBlank-In que chegaram com o bit de VBlank-In ainda aceso (nao reconhecido): o tratador ve um
// so atendimento para dois campos (N2b, criterio B4; causa da diferenca de 2 VBlanks do N1).
uint64_t holly_vblank_fused();
// Eventos do Holly entregues (vencidos ou levantados), mesmo com a interrupcao mascarada:
// diagnostico do N2b (epoca de eventos do HH_CRI_DIAG).
uint64_t holly_event_count();

// Nivel IRL pedido ao SH-4 (6, 4, 2 ou 0): maior nivel cujas mascaras IMLn
// (NRM/EXT/ERR) tem algum bit pendente em ISTNRM/ISTEXT/ISTERR.
uint32_t holly_irl_level(uint64_t cycles);
// O mesmo nivel, so com o estado atual dos registradores (sem avaliar eventos vencidos): despertar da cadencia B.
uint32_t holly_irl_level_now();

// true se `a` e um registrador do controlador (o acesso foi tratado).
bool holly_intc_read(Sh4Context* c, uint32_t a, unsigned size, uint32_t& v);
bool holly_intc_write(Sh4Context* c, uint32_t a, unsigned size, uint32_t v);

} // namespace hh
