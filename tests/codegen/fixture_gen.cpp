// SPDX-License-Identifier: MIT
// Derivado de recomp-BStinger tests/codegen/fixture_gen.cpp (MIT, Copyright (c) 2026 Blue Stinger
// Recompiled contributors; ver third_party/licenses/recomp-BStinger-LICENSE.txt).
// Gera C++ para o programa sintetico em tempo de build (usado por test_exec).
#include "../fixtures/sh4_fixture.h"
#include "analysis/analysis.h"
#include "codegen/codegen.h"

#include <cstdio>

int main(int argc, char** argv) {
    if (argc < 2) return 2;
    hh::analysis::Image img;
    img.base = hh::fixture::kBase;
    for (uint16_t w : hh::fixture::kWords) {
        img.bytes.push_back(uint8_t(w));
        img.bytes.push_back(uint8_t(w >> 8));
    }
    hh::analysis::Config cfg;
    cfg.entry    = hh::fixture::kMain;
    const auto a = hh::analysis::analyze(img, cfg);
    hh::codegen::Options opt;
    opt.out_dir        = argv[1];
    opt.funcs_per_file = 1000;
    opt.inline_pool    = argc > 2 && argv[2][0] == '1';
    const auto r       = hh::codegen::generate(img, a, opt);
    std::printf("fixture: %zu funcoes\n", r.functions);
    return 0;
}
