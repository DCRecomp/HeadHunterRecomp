// SPDX-License-Identifier: MIT
// Derivado de recomp-BStinger runtime/pvr/pvr_regs.h (MIT, Copyright (c) 2026 Blue Stinger
// Recompiled contributors; ver third_party/licenses/recomp-BStinger-LICENSE.txt).
#pragma once
// Registradores do PowerVR (Holly), area 0: 0x005F8000-0x005F9FFF.
// M4: guardam o valor escrito; SPG_STATUS e derivado do contador de ciclos.
// HH15.7d3: a temporizacao do video (0x0C8-0x0E0 e SPG_STATUS) e do servico nativo system/spg_bus.h, a quem o modelo
// delega as leituras, as escritas, o reset e o estado do boot.
#include "cpu/sh4_context.h"

namespace hh {

void pvr_regs_reset();

// true se `a` e um registrador do PVR (o acesso foi tratado).
bool pvr_reg_read(Sh4Context* c, uint32_t a, unsigned size, uint32_t& v);
bool pvr_reg_write(Sh4Context* c, uint32_t a, unsigned size, uint32_t v);

// Banco de registradores (0x005F8000-0x005F9FFF, 2048 palavras) como o guest le,
// para comparacao com o oraculo; SPG_STATUS depende de `cycles`.
void pvr_regs_dump(Sh4Context* c, uint32_t* out);

// Leitura de um registrador pelo hardware (rasterizador), sem o diagnostico de
// "leitura sem modelo". `a` = 0x005F8000..0x005F9FFF.
uint32_t pvr_reg(uint32_t a);
// Entrada da paleta (0..1023) em ARGB8888, conforme PAL_RAM_CTRL.
uint32_t pvr_pal_entry(uint32_t index);
// Carrega o banco inteiro (estado deixado pelo BIOS; ID, REVISION e SPG_STATUS sao
// calculados e ignorados).
void pvr_regs_load(const uint32_t* words);

// HH13.6: STARTRENDER pelo caminho do TA (render das listas atuais pelo pvr_render), para o provedor nativo do Kamui nos
// modos de verificacao. HH15.7d4a: a contagem, o gancho, o HH_TA_DUMP e o prazo do fim de render sao do servico nativo
// system/render_bus.h, como os do render pela IR do ListSink.
void pvr_start_render(Sh4Context* c);
// HH13.6: banco de registradores emprestado (0x005F8000-0x005F9FFF, 2048 palavras) que o render, a paleta e o video leem no
// lugar do banco do runtime; nullptr devolve o do runtime. So o provedor nativo do Kamui (o guest nao o ve).
void pvr_regs_set_override(const uint32_t* words);

} // namespace hh
