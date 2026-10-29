// SPDX-License-Identifier: MIT
#pragma once
// HH15.7d: o DMA do canal 2 do SH-4 como servico nativo, com o contrato medido na rota nativa (docs/planos/HH15-PLANO.md,
// medicao da HH15.7): o funil de DMA do Kamui (0x8C17C600) grava SAR2 (0xFFA00020), DMATCR2 (0xFFA00028), CHCR2
// (0xFFA0002C) e DMAOR (0xFFA00040) e le o CHCR2; o tratador 0x8C168160 le e grava o CHCR2; SB_C2DST = 1 (o system block,
// system/sb_bus) dispara a transferencia. Sem modelo de hardware: os registradores do contrato guardam o valor, e a
// transferencia entrega os blocos de 32 bytes da RAM, na ordem, ao destino nativo das listas e texturas (o provedor nativo
// do Kamui), com o efeito do modelo anterior no fim (SAR2 avanca, DMATCR2 zera, CHCR2.TE acende). Os canais 0, 1 e 3 nao
// fazem parte do contrato: no produto, erro explicito (system/dmac_ausente.cpp); na verificacao, o modelo do DMAC
// (memory/sh4_dmac.cpp) os guarda e delega ao servico o canal 2 e o DMAOR.
#include "cpu/sh4_context.h"

namespace hh {

void dma_bus_reset();
// true se `a` e um registrador do contrato (DMAOR, SAR2, DAR2, DMATCR2, CHCR2) com 32 bits; senao false (nao e do servico)
bool dma_bus_read(uint32_t a, unsigned size, uint32_t& v);
bool dma_bus_write(uint32_t a, unsigned size, uint32_t v);

// O destino dos blocos (FIFO de poligonos 0x10000000 ou caminho direto de textura 0x11000000+): true = aceito.
using DmaSink = bool (*)(Sh4Context* c, uint32_t target, const uint32_t* block);
void dma_bus_set_sink(DmaSink sink);  // nullptr desliga
bool dma_bus_has_sink();

// SB_C2DST = 1: `len` bytes de SAR2 para `dest`. false se o canal nao estiver habilitado (DMAOR.DME e CHCR2.DE); sem
// destino nativo, ou com o destino recusando o bloco, erro explicito.
bool dma_bus_ch2_transfer(Sh4Context* c, uint32_t dest, uint32_t len);

} // namespace hh
