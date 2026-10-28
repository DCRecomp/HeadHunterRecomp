// SPDX-License-Identifier: MIT
// Derivado de recomp-BStinger runtime/memory/sh4_ccn.h (MIT, Copyright (c) 2026 Blue Stinger
// Recompiled contributors; ver third_party/licenses/recomp-BStinger-LICENSE.txt).
#pragma once
// Controle de cache do SH-4 (CCN): CCR e os arrays de enderecos/dados dos caches.
// O codigo nativo nao tem cache a emular; o que importa e o valor lido de volta.
#include <cstdint>

namespace hh {

void sh4_ccn_reset();
void sh4_ccn_adopt(uint32_t ccr);  // HH15.1: o servico do tempo logico saiu do contrato (sys_logical)

// true se `a` pertence ao CCN (o acesso foi tratado).
bool sh4_ccn_read(uint32_t a, unsigned size, uint32_t& v);
bool sh4_ccn_write(uint32_t a, unsigned size, uint32_t v);

} // namespace hh
