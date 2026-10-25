// SPDX-License-Identifier: MIT
// Derivado de recomp-BStinger runtime/cpu/sh4_intc.h (MIT, Copyright (c) 2026 Blue Stinger
// Recompiled contributors; ver third_party/licenses/recomp-BStinger-LICENSE.txt).
#pragma once
// INTC do SH-4 e entrega de interrupcoes ao codigo recompilado (R1 do ARCHITECTURE.md).
// A interrupcao e entregue como chamada aninhada ao tratador em VBR+0x600, nos pontos
// de poll do codigo gerado: back-edges (HH_POLL) e logo depois de `ldc ...,sr`, onde
// uma interrupcao pendente pode ser desmascarada.
#include "cpu/sh4_context.h"

namespace hh {

void sh4_intc_reset();

// Entrega a interrupcao pendente de maior prioridade, se SR permitir. c->pc deve ser
// o endereco de retomada (vira SPC).
void sh4_irq_check(Sh4Context* c);

// Registradores do INTC (ICR, IPRA-C) e INTEVT; true se o acesso foi tratado.
bool sh4_intc_read(uint32_t a, unsigned size, uint32_t& v);
bool sh4_intc_write(uint32_t a, unsigned size, uint32_t v);

// Interrupcoes aceitas desde o inicio do processo (diagnostico N2b: epoca de eventos).
uint64_t sh4_irq_taken_count();

} // namespace hh
