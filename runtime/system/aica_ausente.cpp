// SPDX-License-Identifier: MIT
// HH15.6: o lugar do modelo da AICA no produto (HeadHunter.exe). Compilado como fonte do proprio produto
// (app/CMakeLists.txt): a API do modelo (aica/aica_regs.h) que o boot e a memoria chamam vai direto a area de som nativa
// (system/sound_bus), entao o linker nao puxa aica/aica_regs do hh_runtime (catraca hh15_produto_papeis). A RAM de som
// do modelo (aica_ram) nao existe no produto: o audio nativo tem a sua, e o --audio legado e recusado antes.
#include "aica/aica_regs.h"
#include "system/sound_bus.h"

namespace hh {

void     aica_regs_reset() { sound_bus_reset(); }
void     aica_rtc_set(uint32_t seconds, uint64_t now) { sound_bus_rtc_set(seconds, now); }
uint32_t aica_rtc_host_now() { return sound_bus_rtc_host_now(); }
bool     aica_reg_read(Sh4Context* c, uint32_t a, unsigned size, uint32_t& v) { return sound_bus_read(c, a, size, v); }
bool     aica_reg_write(Sh4Context* c, uint32_t a, unsigned size, uint32_t v) { return sound_bus_write(c, a, size, v); }

} // namespace hh
