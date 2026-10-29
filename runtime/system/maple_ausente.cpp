// SPDX-License-Identifier: MIT
// HH15.4: o lugar do modelo do Maple no produto (HeadHunter.exe). Compilado como fonte do proprio produto
// (app/CMakeLists.txt): a API do modelo (maple/maple.h) que o runtime e o host chamam vai direto ao barramento sem
// dispositivos (system/maple_bus), entao o linker nao puxa maple/maple do hh_runtime (catraca hh15_produto_papeis). O
// controle do modelo (--controles legado) nao existe no produto: a linha de comando o recusa antes.
#include "maple/maple.h"
#include "system/maple_bus.h"

namespace hh {

void          maple_reset() { maple_bus_reset(); }
void          maple_set_controller(const MapleControllerState&) {}
bool          maple_read(Sh4Context* c, uint32_t a, uint32_t& v) { return maple_bus_read(c, a, v); }
bool          maple_write(Sh4Context* c, uint32_t a, uint32_t v) { return maple_bus_write(c, a, v); }
uint64_t      maple_cond_seq(int) { return 0; }  // nenhum controle responde ao GET_CONDITION
void          maple_vblank_in() { maple_bus_vblank_in(); }
uint64_t      maple_dma_due() { return maple_bus_dma_due(); }
void          maple_dma_take() { maple_bus_dma_take(); }
void          maple_set_controller_present(bool) {}  // sempre sem controle no barramento
bool          maple_controller_present() { return false; }
MapleCounters maple_counters() { return maple_bus_counters(); }
void          maple_counters_reset() { maple_bus_counters_reset(); }

} // namespace hh
