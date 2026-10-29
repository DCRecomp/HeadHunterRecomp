// SPDX-License-Identifier: MIT
// HH15.7d: o lugar do modelo do system block no produto (HeadHunter.exe). Compilado como fonte do proprio produto
// (app/CMakeLists.txt): a API do modelo (pvr/holly_sb.h) que o boot e a memoria chamam vai ao Maple (o barramento sem
// dispositivos do produto, system/maple_bus) e ao servico nativo do contrato medido (system/sb_bus), entao o linker nao
// puxa pvr/holly_sb do hh_runtime (catraca hh15_produto_papeis). O resto da regiao e erro explicito.
#include "pvr/holly_sb.h"

#include "cpu/sh4_runtime.h"
#include "maple/maple.h"
#include "system/sb_bus.h"

namespace hh {

void holly_sb_reset() {
    sb_bus_reset();
    maple_reset();
}

bool holly_sb_read(Sh4Context* c, uint32_t a, unsigned size, uint32_t& v) {
    if (!sb_bus_contains(a)) return false;
    if (size == 4 && maple_read(c, a, v)) return true;
    if (sb_bus_read(c, a, size, v)) return true;
    sh4_fatal(c, "system block: leitura fora do contrato", c->pc, a);
}

bool holly_sb_write(Sh4Context* c, uint32_t a, unsigned size, uint32_t v) {
    if (!sb_bus_contains(a)) return false;
    if (size == 4 && maple_write(c, a, v)) return true;
    if (sb_bus_write(c, a, size, v)) return true;
    sh4_fatal(c, "system block: escrita fora do contrato", c->pc, a);
}

} // namespace hh
