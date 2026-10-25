// SPDX-License-Identifier: MIT
// Derivado de recomp-BStinger tests/unit/test.h (MIT, Copyright (c) 2026 Blue Stinger
// Recompiled contributors; ver third_party/licenses/recomp-BStinger-LICENSE.txt).
#pragma once
// Mini-framework de testes sem dependencias (pode virar doctest no M2).
#include <cstdio>
#include <cstdlib>
#include <functional>
#include <utility>
#include <vector>

namespace hh::test {

struct Case {
    const char*           name;
    std::function<void()> fn;
};

inline std::vector<Case>& registry() { static std::vector<Case> r; return r; }
inline int& failures() { static int f = 0; return f; }
inline int& skips() { static int s = 0; return s; }  // casos que nao conferiram nada (dados do jogo ausentes)

struct Reg {
    Reg(const char* n, std::function<void()> f) { registry().push_back({n, std::move(f)}); }
};

} // namespace hh::test

#define HH_TEST(name)                                         \
    static void name();                                       \
    static ::hh::test::Reg reg_##name(#name, name);           \
    static void name()

// Caso que depende de dados ausentes: aparece como [skip], nao como [ ok ]. Com HH_EXIGIR_DADOS=1 vira falha (a
// validacao de fidelidade exige os dados do disco; revisao Codex 2026-10-20).
#define HH_SKIP(motivo)                                                                             \
    do {                                                                                            \
        if (const char* exigir_ = std::getenv("HH_EXIGIR_DADOS"); exigir_ && *exigir_ == '1') {    \
            std::fprintf(stderr, "  FALHA %s:%d: dados exigidos: %s\n", __FILE__, __LINE__, motivo); \
            ++::hh::test::failures();                                                               \
        } else {                                                                                    \
            ++::hh::test::skips();                                                                  \
        }                                                                                           \
        return;                                                                                     \
    } while (0)

#define HH_CHECK(cond)                                                              \
    do {                                                                            \
        if (!(cond)) {                                                              \
            std::fprintf(stderr, "  FALHA %s:%d: %s\n", __FILE__, __LINE__, #cond); \
            ++::hh::test::failures();                                               \
        }                                                                           \
    } while (0)
