// SPDX-License-Identifier: MIT
// Derivado de recomp-BStinger runtime/pvr/pvr_render.h (MIT, Copyright (c) 2026 Blue Stinger
// Recompiled contributors; ver third_party/licenses/recomp-BStinger-LICENSE.txt).
#pragma once
// Rasterizador de referencia do PowerVR2 (software): executa o render pedido por
// STARTRENDER a partir das listas recebidas pelo TA e grava o resultado no framebuffer
// da VRAM (FB_W_SOF1, formato de FB_W_CTRL). Fiel ao modelo do hardware (buffer de
// tile de 32 bits, profundidade em 1/w, fundo, listas opaca/punch-through/translucida),
// com aproximacoes documentadas em pvr_render.cpp.
#include "cpu/sh4_context.h"
#include "render/soft/soft_raster.h"

#include <cstddef>
#include <cstdint>
#include <span>
#include <vector>

namespace hh {

struct RenderStats {
    uint32_t triangles = 0, sprites = 0, skipped = 0;  // skipped: formatos ainda nao suportados
    uint32_t width = 0, height = 0;
};

RenderStats pvr_render(Sh4Context* c);

// Testes: true se, desde a ultima chamada, algum worker da resolucao paralela calculou com
// MXCSR diferente do padrao (o render deve isolar o ambiente de ponto flutuante).
bool pvr_render_take_fp_violation();

// HH15.7b: as partes do render que nao decodificam: a gravacao da imagem (ARGB8888 width x height) no framebuffer
// (FB_W_SOF1, formato de FB_W_CTRL, FB_W_LINESTRIDE e clip) e o aviso do Region Array com modo de ordenacao por tile.
void pvr_write_framebuffer(std::span<const uint32_t> argb, uint32_t width, uint32_t height);
void pvr_check_region_sort(bool autosort);

// ---- O que o decodificador das listas do TA produz (N7a: a mesma decodificacao alimenta o rasterizador e a captura
// da IR de render; nenhum segundo parser) ----

// HH15.7a: os tipos sao os do rasterizador de referencia (native/render/soft/soft_raster.h), que saiu daqui.
using PvrColor  = native::render::soft::Color;   // [0, 1], ja quantizados como o TA guarda (8 bits por canal)
using PvrVertex = native::render::soft::Vertex;  // z = 1/w
// Um triangulo com o estado decodificado do global e do recorte do usuario (as palavras do hardware, sem traducao).
using PvrTri = native::render::soft::Tri;
// Os registradores do quadro que o rasterizador usa (lidos uma vez por render): fundo, profundidade do fundo,
// FPU_CULL_VAL, stride linear das texturas (TEXT_CONTROL), PT_ALPHA_REF, HALF_OFFSET, clamp de cor e a ordenacao
// dos translucidos. A captura da IR redesenha a partir dos FrameParams convertidos de volta para esta forma.
using PvrFrame = native::render::soft::Frame;

PvrFrame pvr_frame_from_regs();

struct TaParam;

// Captura (N7a): recebe, na ordem de submissao, o que o decodificador produz em cada render. `vertex` e cada vertice
// de um strip com o estado do desenho (os vertices de `state` nao valem; end = fim de strip pelo PCW);
// `strip_break` quando o decodificador recomeca o strip sem fim (global novo, fim de lista); `sprite` os 4 cantos
// (A, B, C e D calculado) de um sprite; `modifier` cada parametro de vertice de volume modificador (listas 1 e 3,
// que o rasterizador ainda nao desenha) com o global dele; `global` cada parametro global (tipos 4 e 5, antes de o
// decodificador aplica-lo: os volumes modificadores fecham pela instrucao do global); `skipped` cada parametro de vertice omitido por estado
// nao suportado. `end` recebe os registradores do quadro usados, os triangulos que o rasterizador desenhou (na ordem
// de submissao) e a imagem ARGB8888 do render (w x h), antes da gravacao no framebuffer.
class PvrCaptureSink {
public:
    virtual ~PvrCaptureSink() = default;
    virtual void begin(uint32_t width, uint32_t height)                                                      = 0;
    virtual void vertex(const PvrTri& state, const PvrVertex& v, bool end_of_strip)                          = 0;
    virtual void strip_break()                                                                               = 0;
    virtual void sprite(const PvrTri& state, const PvrVertex& a, const PvrVertex& b, const PvrVertex& c,
                        const PvrVertex& d)                                                                  = 0;
    virtual void global(const TaParam& p)                                                                    = 0;
    virtual void modifier(const TaParam& global, const TaParam& p)                                           = 0;
    virtual void skipped(const TaParam& p)                                                                   = 0;
    virtual void end(const PvrFrame& frame, std::span<const PvrTri> drawn, std::span<const uint32_t> image, uint32_t w,
                     uint32_t h)                                                                             = 0;
    // N7b: depois da decodificacao, antes do rasterizador em software. true = `image` (w x h ARGB8888, ja com o
    // tamanho certo) foi preenchida por outro renderizador e o software nao roda neste render.
    virtual bool render(const PvrFrame&, uint32_t, uint32_t, std::vector<uint32_t>&) { return false; }
    // HH15.7a: true = o renderizador do render e o deste destino (o software sobre a IR ou o Vulkan): um render que ele
    // nao desenhou e erro explicito, nunca o rasterizador sobre o TA no lugar.
    virtual bool renders() const { return false; }
};
void pvr_render_set_capture(PvrCaptureSink* sink);  // nullptr desliga
uint32_t pvr_background_color();                     // cor do plano de fundo do render atual (ISP_BACKGND_T), RGB

// Rasteriza triangulos dados (na ordem de submissao) com os registradores de quadro `frame` (nenhum registrador do
// PVR e lido; os texels vem da VRAM atual), sem a VRAM de saida: a imagem ARGB8888 w x h. Mesma regra do pvr_render
// (prova de equivalencia da IR: a imagem a partir da IR tem de ser identica a do caminho do TA).
std::vector<uint32_t> pvr_render_triangles(std::span<const PvrTri> tris, const PvrFrame& frame, uint32_t width,
                                           uint32_t height);

} // namespace hh
