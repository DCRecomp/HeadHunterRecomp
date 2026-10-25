// SPDX-License-Identifier: MIT
// Derivado de recomp-BStinger translator/hwscan/probe.h (MIT, Copyright (c) 2026 Blue Stinger
// Recompiled contributors; ver third_party/licenses/recomp-BStinger-LICENSE.txt).
#pragma once
// N0: leitura do TSV da sonda dinamica (runtime/debug/hw_probe, HH_HW_PROBE) e juncao com o
// manifesto estatico: acessos vistos em execucao viram HwRef dinamicos e desvios indiretos
// observados viram arestas EdgeKind::Dynamic.
#include "hwscan.h"

#include <string>

namespace hh::hwscan {

struct ProbeStats {
    size_t accesses = 0, edges = 0, skipped = 0;
};

// Lanca std::runtime_error em linha malformada.
ProbeStats merge_probe(Scan& s, const analysis::Image& img, const std::string& tsv);

} // namespace hh::hwscan
