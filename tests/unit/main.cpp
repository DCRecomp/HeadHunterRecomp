// SPDX-License-Identifier: MIT
// Derivado de recomp-BStinger tests/unit/main.cpp (MIT, Copyright (c) 2026 Blue Stinger
// Recompiled contributors; ver third_party/licenses/recomp-BStinger-LICENSE.txt).
#include "test.h"

int main() {
    for (auto& c : hh::test::registry()) {
        const int before = hh::test::failures(), skipsBefore = hh::test::skips();
        c.fn();
        const char* r = hh::test::failures() != before ? "FAIL" : hh::test::skips() != skipsBefore ? "skip" : " ok ";
        std::printf("[%s] %s\n", r, c.name);
        std::fflush(stdout);  // saida em arquivo: o ultimo teste fica visivel se outro travar
    }
    // contagem no fim: uma saida cortada (processo interrompido) nao passa por aprovada (revisao Codex 2026-10-20);
    // os casos sem dados do jogo aparecem a parte, nunca somados aos que conferiram (revisao Codex 2026-10-20, BAIXA)
    std::printf("%zu testes, %d falhas, %d sem dados (skip)\n", hh::test::registry().size(), hh::test::failures(),
                hh::test::skips());
    return hh::test::failures() ? 1 : 0;
}
