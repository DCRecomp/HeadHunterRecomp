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

// Diagnostico/testes: chamado a cada escrita em STARTRENDER, antes de o valor ser
// guardado (n = numero da escrita, a partir de 1). Pode lancar (checkpoint do hh-diff).
// Fim de render pendente (ciclo em que ISTNRM 0/1/2 acendem) ou 0 se nenhum; o
// controlador do Holly consome com pvr_render_done_take.
uint64_t pvr_render_done_due();
void     pvr_render_done_take();

// HH13.6: STARTRENDER (render das listas atuais + agendamento do fim do render), para o provedor nativo do Kamui.
void pvr_start_render(Sh4Context* c);
// HH15.7b: o STARTRENDER com outro render no lugar do pvr_render (o da IR do ListSink nativo): a mesma contagem, o
// mesmo gancho e o mesmo prazo do fim de render, pelos bytes que o TA (ou o destino nativo) recebeu.
// `params`, `dumpParams` e `bank` servem o diagnostico HH_TA_DUMP (a contagem, as linhas dos parametros, por
// pvr_render_dump_param, e o banco de registradores impresso: o do estado nativo, ou nullptr para o deste banco).
void pvr_start_render_with(Sh4Context* c, uint32_t taBytes, size_t params, void (*render)(Sh4Context*),
                           void (*dumpParams)(), const uint32_t* bank);
void pvr_render_dump_param(uint32_t list, const uint32_t* w, uint32_t words);  // "[render]   L<lista> <palavras>"
// HH13.6: banco de registradores emprestado (0x005F8000-0x005F9FFF, 2048 palavras) que o render, a paleta e o video leem no
// lugar do banco do runtime; nullptr devolve o do runtime. So o provedor nativo do Kamui (o guest nao o ve).
void pvr_regs_set_override(const uint32_t* words);

using PvrStartRenderHook = void (*)(Sh4Context* c, int n);
void pvr_set_startrender_hook(PvrStartRenderHook hook);

} // namespace hh
