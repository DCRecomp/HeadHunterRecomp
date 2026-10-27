// SPDX-License-Identifier: MIT
// HH13: conversao nativa das texturas da captura da IR (transitional/render_capture) contra o decodificador do legado
// (runtime/pvr/pvr_texture), com VRAM sintetica.
#include "test.h"

#include "pvr/pvr_texture.h"
#include "pvr/vram.h"
#include "render_capture.h"

#include <cstdint>
#include <cstring>
#include <vector>

namespace rc = hh::transitional::render_capture;

namespace {

void write64(uint32_t addr, uint16_t v) { std::memcpy(hh::vram_data() + hh::vram_offset64(addr), &v, 2); }

} // namespace

// Textura linear com stride select e largura maior que o stride (a do filme do jogo: TCW 0x0E0C2960, 1024 x 512, stride
// menor): a linha y comeca em y * stride e vai ate x = largura - 1, entao as ultimas linhas leem alem de stride * altura.
// A conversao nativa tem de ler esses texels da VRAM como o legado (antes da correcao, o buffer era stride * altura e a
// leitura passava do fim dele).
HH_TEST(hh13_linear_stride_texture_past_last_row) {
    hh::vram_reset();
    const uint32_t base = 0x100000, w = 64, h = 16, stride = 32;
    for (uint32_t i = 0; i < (h - 1) * stride + w; ++i) write64(base + 2 * i, uint16_t(0x8000u | ((i * 2654435761u) >> 17)));
    const uint32_t tcw = (1u << 26) | (1u << 25) | (1u << 27) | (base >> 3);  // linear, stride select, RGB565
    rc::TextureKey k;
    k.tcw = tcw, k.width = w, k.height = h, k.stride = stride;
    std::vector<uint32_t> argb;
    HH_CHECK(rc::convert_texture(k, argb) && argb.size() == size_t(w) * h);
    const hh::TexInfo t{tcw, w, h, stride};
    uint32_t          diff = 0;
    for (uint32_t y = 0; y < h; ++y)
        for (uint32_t x = 0; x < w; ++x) diff += argb[size_t(y) * w + x] != hh::pvr_texel(t, x, y);
    HH_CHECK(diff == 0);
    hh::vram_reset();
}

// O alcance da leitura (sem depender de comportamento indefinido): cobre o ultimo texel acessado em todos os casos e,
// com stride menor que a largura, e maior que stride * altura (o calculo antigo, que deixava a leitura passar do fim).
HH_TEST(hh13_linear_texels_cover_every_access) {
    struct Caso {
        uint32_t w, h, stride;
    };
    for (const Caso c : {Caso{64, 16, 32}, Caso{64, 16, 64}, Caso{32, 16, 64}, Caso{64, 16, 0}, Caso{1024, 512, 480}, Caso{8, 1, 4}}) {
        const size_t row  = c.stride ? c.stride : c.w;
        const size_t last = (size_t(c.h) - 1) * row + (c.w - 1);  // indice do ultimo texel que o laco le
        HH_CHECK(rc::linear_texture_texels(c.w, c.h, c.stride) > last);
    }
    HH_CHECK(rc::linear_texture_texels(64, 16, 32) == 15 * 32 + 64 && rc::linear_texture_texels(64, 16, 32) > 32 * 16);
    HH_CHECK(rc::linear_texture_texels(64, 16, 0) == 64 * 16);
    HH_CHECK(rc::linear_texture_texels(64, 0, 32) == 0 && rc::linear_texture_texels(0, 16, 32) == 0);  // sem underflow
}

// texture_source (a chave do cache do backend): com stride, o ultimo texel da ultima linha faz parte da fonte (mudar
// ele muda a fonte); numa textura de paleta, os bits 26/25 sao o seletor (sempre twiddled): o tamanho e o de 4 bits por
// texel mais o banco da paleta, nunca o da regra linear (revisao Codex HH13).
HH_TEST(hh13_texture_source_ranges) {
    hh::vram_reset();
    const uint32_t base = 0x200000, w = 64, h = 16, stride = 32;
    rc::TextureKey k;
    k.tcw = (1u << 26) | (1u << 25) | (1u << 27) | (base >> 3), k.width = w, k.height = h, k.stride = stride;
    std::vector<uint8_t> a, b;
    rc::texture_source(k, a);
    write64(base + 2 * uint32_t((h - 1) * stride + (w - 1)), 0x1234);
    rc::texture_source(k, b);
    HH_CHECK(a.size() == 2 * rc::linear_texture_texels(w, h, stride) && a != b);
    rc::TextureKey p;  // paleta de 4 bits, seletor com os bits 26 e 25 ligados
    p.tcw = (5u << 27) | (1u << 26) | (1u << 25) | (base >> 3), p.width = 8, p.height = 8, p.stride = stride;
    std::vector<uint8_t> pal;
    rc::texture_source(p, pal);
    HH_CHECK(pal.size() == 8 * 8 / 2 + 16 * 4);
    hh::vram_reset();
}
