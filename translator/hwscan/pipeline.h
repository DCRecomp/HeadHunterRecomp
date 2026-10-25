// SPDX-License-Identifier: MIT
// Derivado de recomp-BStinger translator/hwscan/pipeline.h (MIT, Copyright (c) 2026 Blue Stinger
// Recompiled contributors; ver third_party/licenses/recomp-BStinger-LICENSE.txt).
#pragma once
// N0: o fluxo completo do hh-hwscan como biblioteca (CLI e teste com dados reais).
#include "app/config.h"
#include "closure.h"
#include "metrics.h"
#include "probe.h"
#include "sdkid.h"

#include <filesystem>
#include <memory>

namespace hh::hwscan {

struct Report {
    GameConfig                        cfg;
    analysis::Image                   img;
    analysis::Analysis                analysis;
    Scan                              scan;
    SdkId                             sdk;
    std::map<uint32_t, BoundaryEntry> boundary;  // derivada do grafo atual (com a sonda, se dada)
    ProbeStats                        probe;
    bool                              hasProbe = false;
};

// Lanca std::runtime_error em erro. A sonda (TSV de HH_HW_PROBE) entra depois da
// identificacao: camadas e familias vem so da analise estatica.
std::unique_ptr<Report> run(const std::filesystem::path& config, const std::filesystem::path& executable,
                            const std::filesystem::path& probeTsv = {});

} // namespace hh::hwscan
