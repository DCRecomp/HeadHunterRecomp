// SPDX-License-Identifier: MIT
// Derivado de recomp-BStinger tests/unit/test_analysis.cpp (MIT, Copyright (c) 2026 Blue Stinger
// Recompiled contributors; ver third_party/licenses/recomp-BStinger-LICENSE.txt).
// Testes do analisador sobre o programa sintetico (tests/fixtures/sh4_fixture.h).
#include "test.h"

#include "../fixtures/sh4_fixture.h"
#include "analysis/analysis.h"
#include "codegen/codegen.h"

using namespace hh;
namespace fx = hh::fixture;

namespace {
analysis::Image fixtureImage() {
    analysis::Image img;
    img.base = fx::kBase;
    for (uint16_t w : fx::kWords) {
        img.bytes.push_back(uint8_t(w));
        img.bytes.push_back(uint8_t(w >> 8));
    }
    return img;
}
analysis::Analysis run() {
    analysis::Config cfg;
    cfg.entry = fx::kMain;
    return analysis::analyze(fixtureImage(), cfg);
}
} // namespace

HH_TEST(analysis_finds_functions_and_calls) {
    const auto a = run();
    HH_CHECK(a.functions.size() == 3);
    HH_CHECK(a.functions.count(fx::kMain) && a.functions.count(fx::kDouble) && a.functions.count(fx::kSet1234));
    HH_CHECK(a.stats.dead_ends == 0 && a.stats.orphans == 0);
    const auto& m = a.functions.at(fx::kMain);
    // jsr @r3 resolvido pelo pool (mov.l + jsr sem label no meio)
    HH_CHECK(m.direct.count(fx::kJsr) && m.direct.at(fx::kJsr) == fx::kSet1234);
    HH_CHECK(m.unresolved.empty());
    // delay slot do bsr fica em slots, nao em insns
    HH_CHECK(m.slots.count(0x8C010010) && !m.insns.count(0x8C010010));
}

HH_TEST(analysis_jump_table_bounded) {
    const auto a = run();
    HH_CHECK(a.jump_tables.size() == 1);
    const auto it = a.jump_tables.find(fx::kBraf);
    HH_CHECK(it != a.jump_tables.end());
    if (it != a.jump_tables.end()) {
        HH_CHECK(it->second.bounded && it->second.table == fx::kTable && it->second.entry_size == 2);
        HH_CHECK(it->second.targets == std::vector<uint32_t>({0x8C010026, 0x8C01002C, 0x8C010034}));
    }
    // a tabela e o pool sao dado, nao codigo
    HH_CHECK(a.data.count(fx::kTable) && a.data.count(0x8C010054) && a.data.count(0x8C010058));
    HH_CHECK(!a.code.count(fx::kTable));
}

HH_TEST(analysis_register_write_sets) {
    using isa::decode;
    HH_CHECK(analysis::gprWrites(decode(0x64F6)) == ((1u << 4) | (1u << 15)));  // mov.l @r15+,r4
    HH_CHECK(analysis::gprWrites(decode(0x2F46)) == (1u << 15));                // mov.l r4,@-r15
    HH_CHECK(analysis::gprWrites(decode(0x3127)) == 0);                         // cmp/gt
    HH_CHECK(analysis::gprWrites(decode(0xC70B)) == 1);                         // mova -> r0
    HH_CHECK(analysis::gprWrites(decode(0x410E)) == 0xFF);                      // ldc r1,sr (banco)
    HH_CHECK(analysis::gprWrites(decode(0xF00C)) == 0);                         // fmov (FPU)
}

HH_TEST(codegen_emits_expected_constructs) {
    const auto img = fixtureImage();
    const auto a   = run();
    codegen::Options opt;
    const std::string s = codegen::emitFunction(img, a, a.functions.at(fx::kMain), opt);
    HH_CHECK(s.find("switch (::hh::sh4_phys(tgt))") != std::string::npos);
    HH_CHECK(s.find("HH_CALL(HHG_FN(8C01005C)(c), 0x8C010012u)") != std::string::npos);  // bsr double
    HH_CHECK(s.find("HH_EXPECT_TARGET(0x8C01003Cu, tgt, 0x8C010060u)") != std::string::npos);
    HH_CHECK(s.find("c->r[1] = 0x0000012Cu;") != std::string::npos);  // pool inline (300)
    HH_CHECK(s.find("const uint32_t take = c->t;") != std::string::npos);  // bf/s
}

// Revisao Codex M3: rte le SPC antes do slot, restaura SR e verifica o retorno.
HH_TEST(codegen_rte_uses_spc) {
    analysis::Image img;
    img.base = fx::kBase;
    for (uint16_t w : {uint16_t(0x002B), uint16_t(0x0009)}) {  // rte; nop
        img.bytes.push_back(uint8_t(w));
        img.bytes.push_back(uint8_t(w >> 8));
    }
    analysis::Config cfg;
    cfg.entry    = fx::kBase;
    const auto a = analysis::analyze(img, cfg);
    const std::string s = codegen::emitFunction(img, a, a.functions.at(fx::kBase), codegen::Options{});
    const size_t spc = s.find("const uint32_t tgt = c->spc;"), sr = s.find("sh4_set_sr(*c, c->ssr)"),
                 ret = s.find("HH_RTE(0x8C010000u, tgt)");  // rte: retorno conferido ou continuacao/troca de thread
    HH_CHECK(spc != std::string::npos && sr != std::string::npos && ret != std::string::npos);
    HH_CHECK(spc < sr && sr < ret);
}
