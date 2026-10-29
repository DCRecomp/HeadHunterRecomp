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
// HH15.1: o servico do tempo logico (sys_logical) saiu do contrato: o modelo assume ICR/IPR com estes valores.
void sh4_intc_adopt(uint16_t icr, const uint16_t ipr[3]);

// Entrega a interrupcao pendente de maior prioridade, se SR permitir. c->pc deve ser
// o endereco de retomada (vira SPC).
void sh4_irq_check(Sh4Context* c);
// HH_IRQ_CHECK (depois de `ldc ...,sr`): o mesmo, com a origem SrCheck (diagnostico HH15.1).
void sh4_irq_check_sr(Sh4Context* c);

// Registradores do INTC (ICR, IPRA-C) e INTEVT; true se o acesso foi tratado.
bool sh4_intc_read(uint32_t a, unsigned size, uint32_t& v);
bool sh4_intc_write(uint32_t a, unsigned size, uint32_t v);

// Interrupcoes aceitas desde o inicio do processo (diagnostico N2b: epoca de eventos).
uint64_t sh4_irq_taken_count();
// Codigo da ultima interrupcao aceita (INTEVT), gravado pela entrega (servico do tempo logico, HH15.1).
uint32_t sh4_irq_intevt();

// HH15.1 (diagnostico, nao muda a execucao): de onde veio cada entrega. Quantum = atendimento pelo quantum de
// ciclos do poll; Forced = atendimento pedido (espera que andou o instante, fim de DMA: next_event = 0); Hardware =
// espera do hardware (hardware_wait); SrCheck = HH_IRQ_CHECK depois de `ldc ...,sr` (fora de um atendimento); Wakeup =
// atendimento pedido por sh4_irq_wakeup (cadencia B: nivel que ficou entregavel fora de um atendimento).
enum class IrqOrigin : uint8_t { Quantum, Forced, Hardware, SrCheck, Wakeup, Count };
// Entregas por origem desde o inicio do processo.
uint64_t sh4_irq_origin_count(IrqOrigin o);
// Origem dos atendimentos feitos enquanto o objeto existe (hardware_wait); aninhavel.
class IrqOriginScope {
public:
    explicit IrqOriginScope(IrqOrigin o);
    ~IrqOriginScope();
    IrqOriginScope(const IrqOriginScope&)            = delete;
    IrqOriginScope& operator=(const IrqOriginScope&) = delete;

private:
    IrqOrigin saved_;
    bool      savedSet_;
};
// Usado por sh4_service: a origem do atendimento corrente (o escopo, se houver; senao Quantum/Forced).
IrqOrigin sh4_irq_service_origin(bool forced);
IrqOrigin sh4_irq_set_origin(IrqOrigin o);  // origem das proximas entregas; devolve a anterior (sh4_service a restaura)

} // namespace hh
