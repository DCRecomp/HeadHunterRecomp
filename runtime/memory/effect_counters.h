// SPDX-License-Identifier: MIT
// Derivado de recomp-BStinger runtime/memory/effect_counters.h (MIT, Copyright (c) 2026 Blue Stinger
// Recompiled contributors; ver third_party/licenses/recomp-BStinger-LICENSE.txt).
#pragma once
// N2b-2 (--tempo experimental): contadores de efeitos fora da RAM principal, lidos pelo detector
// de ociosidade da thread 3 (transitional/cri_idle). Sempre compilados: so os caminhos lentos
// incrementam (escrita de hardware, descarga de store queue, escrita do host na RAM do guest);
// as escritas da CPU na RAM nao passam por aqui.
#include <cstdint>

namespace hh {

struct EffectCounters {
    uint64_t hwWrites   = 0;  // hwWrite: MMIO, VRAM, AICA (inclui as escritas sem modelo)
    uint64_t sqFlushes  = 0;  // sh4_sq_flush: pref de store queue (TA, VRAM, RAM)
    uint64_t hostWrites = 0;  // GuestMemory::write: provedores nativos escrevendo na RAM do guest
};

extern EffectCounters g_effects;

} // namespace hh
