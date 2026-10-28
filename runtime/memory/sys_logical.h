// SPDX-License-Identifier: MIT
#pragma once
// HH15.1 (docs/planos/HH15-PLANO.md; modelo: tmu_logical/sys_logical do N8.1 do Blue Stinger): servicos estreitos do
// tempo logico para os perifericos do SH-4 que o jogo ainda le pelo codigo gerado. So o contrato medido (sonda
// HH_HW_PROBE na rota nativa e disassembly):
//  - TMU canal 0 (0x8C15E830, duas vezes: TOCR = 0, TSTR &= ~1, TCR0 = 2 (P/64), TCOR0 = TCNT0 = 0xFFFFFFFF,
//    TSTR |= 1) e as leituras de TCNT0 (leitor 0x8C15E856, esperas do boot e timeouts do Kamui), pelo instante logico,
//    com a semantica do modelo (base e rebase em TSTR/TCR/TCNT, P/64 = 256 ciclos de 200 MHz por tick);
//  - CCN nao grafico: CCR (ICI/OCI leem 0) e escritas no array de enderecos do cache de operandos (sem efeito no host;
//    o valor lido so formaria o endereco de um ocbi);
//  - porta do cabo: PDTRA (pinos 8-9 = tipo de cabo, PCTRA em 0);
//  - INTC: ICR e IPRA-IPRC escritos uma vez no boot (0x8C15FAC8) e nunca lidos (a entrega so usa o nivel IRL do Holly).
// Fora do contrato (outro canal, registrador, tamanho ou valor; leitura de ICR/IPR) o acesso volta ao modelo com o
// estado transferido (*_adopt) e e contado: na HH15.2 isso vira erro explicito no produto. So ativo no tempo logico
// (sh4_logical_cadence); no tempo por ciclos os modelos atendem tudo, como antes.
#include "cpu/sh4_context.h"

#include <cstdint>
#include <string>

namespace hh {

// true se o acesso foi atendido pelo servico (dentro do contrato).
bool sys_logical_read(Sh4Context* c, uint32_t a, unsigned size, uint32_t& v);
bool sys_logical_write(Sh4Context* c, uint32_t a, unsigned size, uint32_t v);
void sys_logical_reset();

struct SysLogicalStats {
    uint64_t tmu = 0, ccn = 0, port = 0, intc = 0;  // acessos atendidos
    uint64_t outside = 0;                            // fora do contrato (foram ao modelo)
    std::string firstOutside;                        // o primeiro, para a mensagem
};
SysLogicalStats sys_logical_stats();
std::string     sys_logical_summary();  // vazio se nada foi atendido

} // namespace hh
