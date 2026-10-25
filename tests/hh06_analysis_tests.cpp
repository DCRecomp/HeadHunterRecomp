// SPDX-License-Identifier: MIT
// HH06: switch por `braf r0; nop` numa tabela de stubs `bra caso; nop` declarada no config.
// As entradas viram rotulos da propria funcao (codigo, nao dado); entrada que nao e
// `bra; nop` invalida a tabela com aviso (o braf cai no despacho, nunca em silencio).
#include "test.h"

#include "analysis/analysis.h"

#include <algorithm>
#include <string>
#include <vector>

namespace {

constexpr uint32_t kBase = 0x8C010000;

hh::analysis::Image image() {
    // 0: mov #0,r0  2: braf r0  4: nop  6: bra +E  8: nop  A: bra +12  C: nop
    // E: rts  10: nop  12: rts  14: nop
    const uint16_t w[] = {0xE000, 0x0023, 0x0009, 0xA002, 0x0009, 0xA002, 0x0009, 0x000B, 0x0009, 0x000B, 0x0009};
    hh::analysis::Image img;
    img.base = kBase;
    for (uint16_t x : w) img.bytes.push_back(uint8_t(x)), img.bytes.push_back(uint8_t(x >> 8));
    return img;
}

hh::analysis::Analysis run(uint32_t count) {
    hh::analysis::Config cfg;
    cfg.entry = kBase;
    cfg.jump_tables.push_back({kBase + 2, kBase + 6, count});
    return hh::analysis::analyze(image(), cfg);
}

} // namespace

HH_TEST(hh06_braf_stub_table_targets_are_labels) {
    const auto a  = run(2);
    const auto jt = a.jump_tables.find(kBase + 2);
    HH_CHECK(jt != a.jump_tables.end());
    if (jt == a.jump_tables.end()) return;
    HH_CHECK(jt->second.bounded && jt->second.targets == std::vector<uint32_t>({kBase + 6, kBase + 0xA}));
    HH_CHECK(!a.data.count(kBase + 6) && !a.data.count(kBase + 0xA));  // entradas sao codigo
    const auto& f = a.functions.at(kBase);
    HH_CHECK(f.labels.count(kBase + 6) && f.labels.count(kBase + 0xA));
    HH_CHECK(f.insns.count(kBase + 0xE) && f.insns.count(kBase + 0x12));  // casos alcancados
    HH_CHECK(a.functions.size() == 1 && a.stats.dead_ends == 0 && a.stats.orphans == 0);
}

HH_TEST(hh06_braf_stub_table_rejects_non_bra_entry) {
    const auto a = run(3);  // a 3a entrada (kBase+0xE) e rts, nao bra
    HH_CHECK(!a.jump_tables.count(kBase + 2));
    const bool warned = std::any_of(a.warnings.begin(), a.warnings.end(),
                                    [](const std::string& w) { return w.find("nao e bra") != std::string::npos; });
    HH_CHECK(warned);
}
