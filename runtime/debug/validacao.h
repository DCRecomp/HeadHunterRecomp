// SPDX-License-Identifier: MIT
#pragma once
// HH15.2 (docs/planos/HH15-PLANO.md; modelo: N8.2 do Blue Stinger): diagnosticos exclusivos de validacao do runtime.
// O executavel de verificacao (hh-run, testes) liga validacao.cpp; o produto (HeadHunter.exe) liga validacao_stubs.cpp,
// em que estas funcoes nao tem efeito e as variaveis de teste sao recusadas na partida (saida 2), para que um teste
// nunca passe no vazio com o executavel errado. A sonda de hardware (debug/hw_probe, HH_HW_PROBE) segue a mesma
// divisao: o produto liga so o stub dela.
#include "cpu/sh4_context.h"

#include <cstdint>

namespace hh {

// true no executavel de validacao (diagnostico: o resumo e os testes da catraca)
bool validation_build();
// HH_CICLOS_RUIDO=<n>: soma ao contador de ciclos um valor pseudoaleatorio em [0, n) (prova de independencia dos ciclos
// do tempo logico; sh4_service e o tick de vivacidade)
void validation_cycle_noise(Sh4Context* c);
// HH_CADENCIA=quantum: o tempo logico mantem o atendimento pelo quantum de ciclos (referencia A/B da cadencia B)
bool validation_cadence_quantum();
// HH_TMU_MUTACAO=parado: o servico do TMU congela o TCNT0 (negativo das referencias)
bool validation_tmu_stopped();
// HH_IRQ_QUANTUM=1: lista as entregas feitas pelo quantum de ciclos (chamada pela ponte em cada uma)
void sh4_irq_quantum_note(Sh4Context* c, uint32_t intevt);

} // namespace hh
