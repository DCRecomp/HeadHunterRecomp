// SPDX-License-Identifier: MIT
// Derivado de recomp-BStinger native/render/soft/soft_device.h (MIT, Copyright (c) 2026 Blue Stinger
// Recompiled contributors; ver third_party/licenses/recomp-BStinger-LICENSE.txt).
#pragma once
// HH15.7a: o renderizador em software sobre a IR (o recuo permanente do Vulkan e o software do produto). Recebe a
// CommandList como o backend Vulkan (IRenderDevice) e desenha com o rasterizador de referencia (soft_raster.h), com os
// texels das texturas que recebeu (nenhuma VRAM, registrador ou endereco do guest).
//
// A IR volta para as palavras do hardware (ISP/TSP/TCW) com as funcoes abaixo, as mesmas regras que o modo `conferir`
// da captura usa para provar que a IR reproduz o caminho do TA (conferido pelo unitario hh15_soft_tests).
#include "render/render.h"
#include "render/soft/soft_raster.h"

#include <unordered_map>

namespace hh::native::render::soft {

uint32_t pack_color(const Color& c);
Color    unpack_color(uint32_t argb);  // x / 255.f, como o decodificador do TA: a ida e a volta sao exatas

uint32_t list_number(ListType l);
// O estado de um desenho nas palavras do hardware. tcw/tex_w/tex_h: a textura (tex_w = 0: sem textura); o tamanho vai
// para os bits do TSP (8 << k).
Tri    tri_state(const DrawState& s, uint32_t tcw, uint32_t tex_w, uint32_t tex_h);
Vertex tri_vertex(const render::Vertex& v);
// Os parametros do quadro (tex_stride: o TEXT_CONTROL, so para as texturas lineares com stride do caminho do TA).
Frame  tri_frame(const FrameParams& p, uint32_t tex_stride);

struct SoftStats {
    uint64_t frames = 0, triangles = 0, fragments = 0, uploads = 0;
};

class SoftwareDevice final : public IRenderDevice {
public:
    Result<TextureHandle> create_texture(const TextureDesc& desc, std::span<const std::byte> data) override;
    Result<void>          update_texture(TextureHandle t, std::span<const std::byte> data) override;
    void                  set_palette(std::span<const uint32_t>) override {}  // as texturas chegam ja em ARGB8888
    void                  destroy_texture(TextureHandle t) override;
    // Os volumes modificadores da IR nao sao desenhados (o rasterizador de referencia nao os tem, como o caminho do TA).
    Result<void>          submit(const CommandList& frame) override;
    Result<void>          present_image(std::span<const uint32_t> argb, uint32_t width, uint32_t height) override;
    Result<void>          present() override { return {}; }
    void                  resize(uint32_t, uint32_t) override {}

    // A imagem do ultimo submit/present_image: ARGB8888, width x height, de cima para baixo.
    Result<void>     read_frame(std::vector<uint32_t>& argb, uint32_t& width, uint32_t& height) const;
    const SoftStats& stats() const { return st_; }

private:
    struct Tex {
        uint32_t              width = 0, height = 0;
        std::vector<uint32_t> argb;
    };
    class Texels;
    std::unordered_map<uint32_t, Tex> texs_;
    uint32_t                          next_ = 0;
    Image                             image_;
    SoftStats                         st_;
};

} // namespace hh::native::render::soft
