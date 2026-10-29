// SPDX-License-Identifier: MIT
// HH15.7d: o lugar do modelo do DMAC no produto (HeadHunter.exe). Compilado como fonte do proprio produto
// (app/CMakeLists.txt): a API do modelo (memory/sh4_dmac.h) que o boot, a memoria e o system block chamam vai direto ao
// servico nativo do canal 2 (system/dma_bus), entao o linker nao puxa memory/sh4_dmac do hh_runtime (catraca
// hh15_produto_papeis). Os canais 0, 1 e 3 nao fazem parte do contrato: erro explicito.
#include "memory/sh4_dmac.h"

#include "cpu/sh4_runtime.h"
#include "system/dma_bus.h"

namespace hh {

void sh4_dmac_reset() { dma_bus_reset(); }

bool sh4_dmac_read(Sh4Context* c, uint32_t a, unsigned size, uint32_t& v) {
    if (dma_bus_read(a, size, v)) return true;
    if (a >= 0xFFA00000 && a < 0xFFA00044)
        sh4_fatal(c, "DMAC: leitura fora do contrato (so o canal 2 e o DMAOR, de 32 bits)", c->pc, a);
    return false;
}

bool sh4_dmac_write(Sh4Context* c, uint32_t a, unsigned size, uint32_t v) {
    if (dma_bus_write(a, size, v)) return true;
    if (a >= 0xFFA00000 && a < 0xFFA00044)
        sh4_fatal(c, "DMAC: escrita fora do contrato (so o canal 2 e o DMAOR, de 32 bits)", c->pc, a);
    return false;
}

bool sh4_dmac_ch2_transfer(Sh4Context* c, uint32_t dest, uint32_t len) { return dma_bus_ch2_transfer(c, dest, len); }

} // namespace hh
