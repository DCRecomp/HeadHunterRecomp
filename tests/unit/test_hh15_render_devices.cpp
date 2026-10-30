// SPDX-License-Identifier: MIT
// HH15.7d4b: o renderizador do produto (transitional/render_devices) separado da captura: a IR do ListSink desenhada no
// software sobre a IR igual ao dispositivo direto, as contagens e o resumo no formato da captura, os texels so da
// memoria nativa (sem ela, a textura nao e enviada e o render falha, nunca pelo decodificador do legado) e a
// instalacao (so Native e Software; Off descarta).
#include "test.h"

#include "render/kamui/list_sink.h"
#include "render/soft/soft_device.h"
#include "render_devices.h"

#include <string>
#include <vector>

namespace rc = hh::transitional::render_capture;
namespace nr = hh::native::render;
namespace km = hh::native::render::kamui;

namespace {

// 32x32, um triangulo sem textura que cobre metade da tela (ou com a textura 8x8 RGB565 em 0x400000)
km::ListFrame triangle(bool textured) {
    km::ListFrame lf;
    lf.cl.frame.width = 32, lf.cl.frame.height = 32;
    lf.cl.frame.clear_color = 0xFF102030;
    nr::Draw d;
    d.first_vertex = 0, d.vertex_count = 3;
    if (textured) {
        d.state.texture = nr::TextureHandle{1};
        lf.textures.push_back({(1u << 27) | (0x400000u >> 3), 8, 8, 0});
    }
    lf.cl.draws.push_back(d);
    for (auto [x, y] : {std::pair{0.f, 0.f}, std::pair{32.f, 0.f}, std::pair{0.f, 32.f}}) {
        nr::Vertex v;
        v.x = x, v.y = y, v.inv_w = 1, v.color = 0xFFC08040;
        lf.cl.vertices.push_back(v);
    }
    lf.sprites = 0;
    return lf;
}

} // namespace

HH_TEST(hh15_render_devices_produto_desenha_a_ir_do_list_sink) {
    rc::install_renderer(rc::Mode::Off);
    std::vector<uint32_t> image(32 * 32, 0xFF102030);
    const km::ListFrame   lf = triangle(false);
    HH_CHECK(!rc::draws_lists() && !rc::draw_list(lf, 32, 32, image) && rc::renderer_summary().empty());
    rc::install_renderer(rc::Mode::Capture);  // so Native e Software sao do renderizador
    HH_CHECK(!rc::draws_lists() && rc::renderer_mode() == rc::Mode::Off);

    rc::install_renderer(rc::Mode::Software);
    HH_CHECK(rc::draws_lists() && rc::renderer_mode() == rc::Mode::Software);
    HH_CHECK(rc::draw_list(lf, 32, 32, image));
    nr::soft::SoftwareDevice direct;  // a mesma IR no dispositivo direto
    HH_CHECK(direct.submit(lf.cl).ok());
    std::vector<uint32_t> ref;
    uint32_t              rw = 0, rh = 0;
    HH_CHECK(direct.read_frame(ref, rw, rh).ok() && rw == 32 && rh == 32);
    HH_CHECK(image == ref);
    HH_CHECK(image[1 * 32 + 1] == 0xFFC08040 && image[31 * 32 + 31] == 0xFF102030);  // desenhou o triangulo, so ele
    const rc::Stats& s = rc::renderer_stats();
    HH_CHECK(s.renders == 1 && s.soft_frames == 1 && s.soft_failures == 0 && s.draws == 1 && s.vertices == 3);
    HH_CHECK(s.vk_frames == 0 && s.vk_device.empty());  // o software nao abre o Vulkan
    const std::string sum = rc::renderer_summary();
    HH_CHECK(sum == rc::format_summary(s));
    HH_CHECK(sum.rfind("[render-ir] renders 1 (conferidos 0): desenhos 1, vertices 3,", 0) == 0);
    HH_CHECK(sum.find("[render-soft] quadros 1, falhas 0,") != std::string::npos);

    // sem a memoria nativa a textura nao tem texels: o render falha (o chamador para com erro explicito)
    rc::set_native_memory(nullptr, nullptr);
    HH_CHECK(!rc::draw_list(triangle(true), 32, 32, image));
    HH_CHECK(rc::renderer_stats().soft_failures == 1 && rc::renderer_stats().renders == 2);
    HH_CHECK(rc::renderer_stats().soft_error.find("textura sem conversao") != std::string::npos);

    // com a memoria nativa, os texels vem dela (RGB565 vermelho)
    km::FrameState  fs;
    km::TextureHeap heap;
    for (uint32_t i = 0; i < 2 * 64; ++i)  // pelo caminho de 64 bits, como o jogo enderecou
        heap.data()[km::TextureHeap::offset64(0x400000 + i)] = (i & 1) ? 0xF8 : 0x00;
    rc::set_native_memory(&fs, &heap);
    HH_CHECK(rc::draw_list(triangle(true), 32, 32, image));
    HH_CHECK((image[1 * 32 + 1] & 0x0000FFFF) == 0 && (image[1 * 32 + 1] & 0x00FF0000) != 0);  // modulado pelo vermelho
    HH_CHECK(rc::renderer_stats().soft_tex_uploads == 1 && rc::renderer_stats().soft_tex_legacy == 0);
    rc::set_native_memory(nullptr, nullptr);

    rc::install_renderer(rc::Mode::Off);  // descarta
    HH_CHECK(!rc::draws_lists() && rc::renderer_stats().renders == 0 && rc::renderer_summary().empty());
}
