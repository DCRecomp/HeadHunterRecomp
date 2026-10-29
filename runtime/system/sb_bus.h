// SPDX-License-Identifier: MIT
#pragma once
// HH15.7d: o system block do Holly (0x005F6800-0x005F7FFF, fora o controlador de interrupcoes e o Maple) como servico
// nativo, com o contrato medido na rota nativa (docs/planos/HH15-PLANO.md, medicao da HH15.7):
//  - leituras: SB_C2DSTAT/C2DLEN/C2DST (0x005F6800/04/08), SB_FFST (0x005F688C, estado da FIFO lido em volta do RTC da
//    AICA por 0x8C183CD4: o valor guardado, 0 = vazia) e SB_SBREV (0x005F689C, 0x0000000B);
//  - escritas guardadas: os registradores do sistema (0x005F6800-0x005F68FF e 0x005F6940-0x005F69FF, depois do INTC: a
//    tabela de inicializacao 0x8C15FAC8 e 0x8C167FA0), do G1 (0x005F7400-0x005F74FF), do G2 (0x005F7800-0x005F78FF: a trava em volta do RTC) e do DMA do PVR
//    (0x005F7C00-0x005F7CFF, a inicializacao);
//  - SB_C2DST = 1 dispara o DMA do canal 2 (sh4_dmac_ch2_transfer: system/dma_bus) de SB_C2DLEN bytes para SB_C2DSTAT;
//    no fim C2DSTAT avanca, C2DLEN e C2DST zeram e ISTNRM bit 19 (fim do DMA do PVR) acende, como no modelo anterior.
// Fora disso: no produto, erro explicito (system/sb_ausente.cpp); na verificacao, o modelo (pvr/holly_sb.cpp) guarda o
// valor e delega ao servico o contrato.
#include "cpu/sh4_context.h"

namespace hh {

void sb_bus_reset();
// true se `a` e um acesso do contrato (32 bits); senao false (o chamador decide)
bool sb_bus_read(Sh4Context* c, uint32_t a, unsigned size, uint32_t& v);
bool sb_bus_write(Sh4Context* c, uint32_t a, unsigned size, uint32_t v);
bool sb_bus_contains(uint32_t a);  // a regiao do system block (endereco do guest), fora o INTC

} // namespace hh
