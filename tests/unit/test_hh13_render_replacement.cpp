// SPDX-License-Identifier: MIT
// Derivado de recomp-BStinger tests/unit/test_render_replacement.cpp (MIT, Copyright (c) 2026 Blue Stinger
// Recompiled contributors; ver third_party/licenses/recomp-BStinger-LICENSE.txt).
// N7: contrato das texturas substituiveis (native/render/texture_replacement.h). So identidade e compatibilidade:
// nenhum carregamento nem upscale no N7.
#include "test.h"

#include "render/texture_replacement.h"

#include <vector>

namespace nr = hh::native::render;

HH_TEST(texture_id_depends_on_content_and_size_only) {
    std::vector<uint32_t> t(64);
    for (uint32_t i = 0; i < 64; ++i) t[i] = 0xFF000000u | i * 0x010203u;
    const nr::TextureId a = nr::texture_id(8, 8, t);
    HH_CHECK(nr::texture_id(8, 8, t) == a);  // deterministico
    HH_CHECK(!(nr::texture_id(16, 4, t) == a));  // mesmos texels, outra forma
    t[63] ^= 1;
    HH_CHECK(!(nr::texture_id(8, 8, t) == a));  // um bit
    t[63] ^= 1;
    t[0] &= 0x00FFFFFF;
    HH_CHECK(!(nr::texture_id(8, 8, t) == a));  // so o alfa
}

HH_TEST(texture_id_text_round_trip) {
    const nr::TextureId a{256, 128, 0x0123456789ABCDEFull};
    HH_CHECK(a.text() == "256x128-0123456789abcdef");
    HH_CHECK(nr::parse_texture_id(a.text()) == a);
    HH_CHECK(!nr::parse_texture_id("256x128-0123"));              // so a forma canonica: 16 hex
    HH_CHECK(!nr::parse_texture_id("256x128-0123456789ABCDEF"));  // minusculos
}

HH_TEST(texture_id_parse_rejects_malformed) {
    HH_CHECK(!nr::parse_texture_id(""));
    HH_CHECK(!nr::parse_texture_id("0x8-0123456789abcdef"));
    HH_CHECK(!nr::parse_texture_id("8x8-0123456789abcdefg"));
    HH_CHECK(!nr::parse_texture_id("8x8_0123456789abcdef"));
    HH_CHECK(!nr::parse_texture_id("8x8-0123456789abcdef.png"));
}

HH_TEST(texture_replacement_compatibility) {
    const nr::TextureId id{64, 32, 1};
    auto r = [](uint32_t w, uint32_t h) { return nr::ReplacementTexture{w, h, std::vector<uint32_t>(size_t(w) * h)}; };
    HH_CHECK(nr::replacement_compatible(id, r(64, 32)));    // fator 1
    HH_CHECK(nr::replacement_compatible(id, r(256, 128)));  // 4x
    HH_CHECK(nr::replacement_compatible(id, r(1024, 512))); // 16x
    HH_CHECK(!nr::replacement_compatible(id, r(2048, 1024)));  // 32x: acima do limite
    HH_CHECK(!nr::replacement_compatible(id, r(256, 256)));    // outra proporcao
    HH_CHECK(!nr::replacement_compatible(id, r(96, 48)));      // fator nao inteiro
    nr::ReplacementTexture bad = r(128, 64);
    bad.argb.pop_back();
    HH_CHECK(!nr::replacement_compatible(id, bad));  // dados do tamanho errado
}
