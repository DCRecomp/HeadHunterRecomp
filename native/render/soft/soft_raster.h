// SPDX-License-Identifier: MIT
// Derivado de recomp-BStinger native/render/soft/soft_raster.h (MIT, Copyright (c) 2026 Blue Stinger
// Recompiled contributors; ver third_party/licenses/recomp-BStinger-LICENSE.txt).
#pragma once
// HH15.7a: o rasterizador de referencia do PowerVR2 em software, sem o hardware simulado: o recuo permanente do Vulkan e
// a referencia das comparacoes. As contas sao as do rasterizador do HH06 (antes no pvr_render do runtime, movidas
// sem mudanca): buffer de 32 bits e profundidade em 1/w; listas opaca, punch-through e translucida (autosort por pixel ou
// na ordem de submissao); interpolacao com correcao de perspectiva, regra top-left, amostras por HALF_OFFSET; recorte do
// usuario; color clamp. Os triangulos chegam com as palavras do hardware (ISP/TSP/TCW) e os texels vem de uma
// TexelSource: a VRAM no caminho do TA (verificacao) ou as texturas do dispositivo (a IR, soft_device.h).
// Aproximacoes (STATUS.md): sem fog, dither, mipmap, VQ, YUV, bump, dois volumes, volumes modificadores, buffers
// secundarios (SRC/DST select) e escala do SCALER_CTL; empates de amostragem exatamente na fronteira de texels dependem
// da precisao numerica.
#include <cstddef>
#include <cstdint>
#include <span>
#include <vector>

namespace hh::native::render::soft {

struct Color {
    float a = 0, r = 0, g = 0, b = 0;  // [0, 1], ja quantizados como o TA guarda (8 bits por canal)
};

struct Vertex {
    float x = 0, y = 0, z = 0, u = 0, v = 0;  // z = 1/w
    Color col, off;
};

// Um triangulo com o estado decodificado do global e do recorte do usuario (as palavras do hardware, sem traducao).
struct Tri {
    uint32_t list = 0, isp = 0, tsp = 0, tcw = 0;
    bool     tex = false, offset = false, gouraud = true;
    uint32_t clipMode = 0;                       // PCW bits 17-16: 2 dentro, 3 fora do recorte
    int      cx0 = 0, cy0 = 0, cx1 = 0, cy1 = 0;  // recorte do usuario em pixels (inclusivo)
    uint32_t texture = 0;                        // textura do dispositivo (a IR; 0 no caminho do TA, que usa o TCW)
    Vertex   v[3];
};

// Os registradores do quadro que o rasterizador usa (lidos uma vez por render): fundo, profundidade do fundo,
// FPU_CULL_VAL, stride linear das texturas (TEXT_CONTROL), PT_ALPHA_REF, HALF_OFFSET, clamp de cor e a ordenacao dos
// translucidos.
struct Frame {
    uint32_t background = 0;  // RGB do plano de fundo
    float    background_depth = 0, cull_val = 0;
    uint32_t tex_stride = 0, pt_ref = 0, half_offset = 0, clamp_max = 0, clamp_min = 0;
    bool     autosort = true;
    bool     operator==(const Frame&) const = default;
};

// A textura de um desenho: o TCW e o tamanho do TSP; `texture` e a do dispositivo quando vem da IR.
struct TexRef {
    uint32_t tcw = 0, width = 0, height = 0, stride = 0, texture = 0;
};

// De onde vem o texel (x, y ja dentro da textura, depois de wrap/clamp/flip): ARGB8888. A memoria das texturas nao muda
// durante um render: o rasterizador guarda os texels lidos por textura.
class TexelSource {
public:
    virtual ~TexelSource()                                          = default;
    virtual uint32_t texel(const TexRef& t, uint32_t x, uint32_t y) = 0;
};

struct Image {
    uint32_t              width = 0, height = 0;
    std::vector<uint32_t> color;  // ARGB8888
    std::vector<float>    depth;  // 1/w
};

// Fases medidas por rasterize (diagnostico de perfil): 1 opaca, 2 punch-through, 3 translucida, 4 resolucao
using PhaseFn = void (*)(void* ctx, int phase);

// O fundo do quadro (alfa 0xFF) e os triangulos (na ordem de submissao; desenhados na ordem das listas opaca,
// punch-through, translucida) na imagem width x height (o tamanho vem de `image`). Devolve o numero de fragmentos
// translucidos ordenados. Roda com o ambiente de ponto flutuante fixo (MXCSR padrao), nesta thread e nos workers da
// resolucao paralela.
size_t rasterize(std::span<const Tri> tris, const Frame& frame, TexelSource& texels, Image& image, PhaseFn phase = nullptr,
                 void* phase_ctx = nullptr);

// Testes: true se, desde a ultima chamada, algum worker da resolucao paralela calculou com MXCSR diferente do padrao.
bool take_fp_violation();

// O ambiente de ponto flutuante do render (MXCSR padrao: mais proximo, sem FTZ/DAZ), restaurado no fim do escopo. O modo
// de ponto flutuante do guest (arredondamento para zero, FTZ/DAZ) nao vale no render.
class FpEnv {
public:
    FpEnv();
    ~FpEnv();
    FpEnv(const FpEnv&)            = delete;
    FpEnv& operator=(const FpEnv&) = delete;

private:
    unsigned saved_;
};

} // namespace hh::native::render::soft
