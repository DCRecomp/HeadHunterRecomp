// SPDX-License-Identifier: MIT
// Derivado de recomp-BStinger translator/hwscan/pipeline.cpp (MIT, Copyright (c) 2026 Blue Stinger
// Recompiled contributors; ver third_party/licenses/recomp-BStinger-LICENSE.txt).
#include "pipeline.h"

#include <algorithm>
#include <fstream>
#include <iterator>
#include <stdexcept>

namespace hh::hwscan {

std::unique_ptr<Report> run(const std::filesystem::path& config, const std::filesystem::path& executable,
                            const std::filesystem::path& probeTsv) {
    auto r      = std::make_unique<Report>();
    r->cfg      = loadConfig(config);
    r->img.base = r->cfg.base;
    std::ifstream f(executable, std::ios::binary);
    if (!f) throw std::runtime_error("nao foi possivel abrir o executavel (rode hh-extract antes)");
    r->img.bytes.assign(std::istreambuf_iterator<char>(f), {});
    for (const auto& rel : r->cfg.relocated) {
        if (!r->img.inMain(rel.source, rel.size)) throw std::runtime_error("relocated: origem fora do executavel");
        r->img.relocated.push_back(rel);
    }
    r->analysis = analysis::analyze(r->img, r->cfg.analysis);
    r->scan     = scan(r->img, r->analysis);
    // HH07: literais declarados no config como nao-endereco saem do manifesto (so constantes;
    // acesso de verdade nunca e removido). A quantidade fica nas metricas.
    if (!r->cfg.hw_not_hardware.empty()) {
        auto& refs = r->scan.refs;
        const size_t before = refs.size();
        refs.erase(std::remove_if(refs.begin(), refs.end(),
                                  [&](const HwRef& x) { return x.kind == Kind::Constant && r->cfg.hw_not_hardware.count(x.pc); }),
                   refs.end());
        r->excludedConstants = before - refs.size();
        if (r->excludedConstants != r->cfg.hw_not_hardware.size())
            throw std::runtime_error("[hwscan] constantes_nao_hw: algum pc nao e um literal do manifesto (lista desatualizada)");
    }
    set_game_markers(r->cfg.sdk_game_markers);
    r->sdk      = identify(r->img, r->analysis, r->scan);
    if (!probeTsv.empty()) {
        std::ifstream p(probeTsv, std::ios::binary);
        if (!p) throw std::runtime_error("nao foi possivel abrir o TSV da sonda");
        const std::string text((std::istreambuf_iterator<char>(p)), {});
        r->probe    = merge_probe(r->scan, r->img, text);
        r->hasProbe = true;
    }
    r->boundary = derive_boundary(r->scan, r->sdk, r->cfg.native_slots);
    return r;
}

} // namespace hh::hwscan
