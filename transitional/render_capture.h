// SPDX-License-Identifier: MIT
// Derivado de recomp-BStinger transitional/render_capture.h (MIT, Copyright (c) 2026 Blue Stinger
// Recompiled contributors; ver third_party/licenses/recomp-BStinger-LICENSE.txt).
#pragma once
// N7a: captura da IR de render (native/render/render.h) a partir do que o decodificador das listas do TA produz
// (runtime/pvr/pvr_render.h, PvrCaptureSink): o rasterizador em software continua igual e e a referencia. Cada render
// vira uma CommandList (strips como Draw, sprites como dois triangulos (A, B, C) e (A, C, D), o estado em DrawState,
// o quadro em FrameParams, os volumes modificadores fechados pela instrucao do global) com a tabela das texturas
// usadas (TextureHandle = indice + 1).
//
// Validacao (HH_RENDER_IR=conferir; HH_RENDER_IR_CADA=N confere 1 render a cada N, padrao 1):
//   - validate() da IR;
//   - os triangulos que a IR descreve (strips com a alternancia de sentido do PVR) sao os que o rasterizador
//     desenhou: mesmos vertices e o mesmo estado efetivo, na mesma ordem;
//   - os FrameParams reproduzem os registradores de quadro que o rasterizador usou (pvr_frame);
//   - a imagem redesenhada so a partir da IR (triangulos e FrameParams; pvr_render_triangles nao le registradores)
//     e IDENTICA a do caminho do TA;
//   - HH15.7a: em TODO render (sem a amostragem de HH_RENDER_IR_CADA), a imagem do software sobre a IR
//     (native/render/soft, texturas pelas conversoes nativas) e IDENTICA a do caminho do TA (negativo
//     HH_RENDER_SOFT_MUTACAO=textura: o primeiro texel de cada textura enviada muda);
//   - cada textura usada, convertida pelas conversoes nativas (native/render/texture_convert) a partir dos bytes da
//     VRAM, e igual texel a texel ao decodificador do legado (pvr_texel).
// HH_RENDER_IR=capturar so monta a IR (para o backend). Resumo no fim: render_capture::summary().
//
// N7b (backend Vulkan, native/render/vulkan): Mode::Native (--render nativo) desenha a IR no Vulkan e a imagem
// substitui a do software (o resto do caminho, gravacao no framebuffer e apresentacao, nao muda); sem Vulkan,
// fica no software com o motivo no resumo. Mode::Compare (HH_RENDER_IR=comparar) mantem o software como saida e
// compara a imagem do Vulkan pixel a pixel (HH_RENDER_IR_BMP=<pasta>: software | Vulkan | diferenca x8).
// Texturas: as conversoes nativas; sem conversao nativa (paleta de 8 bits, YUV, bump), o decodificador do legado
// texel a texel (contadas, nunca descartadas). O cache do backend refaz uma textura quando os bytes dela na VRAM
// (ou a paleta) mudam.
//
// HH15.7d4b: os dispositivos (Vulkan e software sobre a IR), os tipos comuns e o renderizador do produto ficam em
// render_devices.h (no produto); a captura (so verificacao) monta a IR do caminho do TA e desenha nos mesmos
// dispositivos com os texels do runtime (legacy_textures).
#include "pvr/pvr_render.h"
#include "render_devices.h"

#include <cstdint>
#include <string>
#include <vector>

namespace hh::transitional::render_capture {

// IR estruturalmente valida (validate) e handles de textura dentro da tabela do quadro.
bool valid(const Frame& f, std::string* error);

// Conversoes (as regras ficam aqui, num lugar so): o estado decodificado do PVR -> DrawState, e a volta, que prova
// que a IR guarda tudo o que o rasterizador usa (a imagem a partir da IR tem de ser identica).
nr::DrawState  draw_state(const PvrTri& t, uint32_t texture_handle);
PvrTri         pvr_state(const nr::DrawState& s, const TextureKey* tex);  // sem os vertices
nr::Vertex     vertex(const PvrVertex& v);
PvrVertex      pvr_vertex(const nr::Vertex& v);
nr::FrameParams frame_params(uint32_t width, uint32_t height);  // dos registradores atuais do PVR
PvrFrame       pvr_frame(const Frame& f);  // a volta: o que o rasterizador usa, so a partir da IR

// Os triangulos que a IR descreve, na ordem de submissao (strip: o k-esimo com os vertices k, k+1, k+2, os dois
// primeiros trocados em k impar, como o decodificador do TA).
std::vector<PvrTri> triangles(const Frame& f);

// Texels de uma textura (ARGB8888 linear, o maior nivel) pelas conversoes nativas; false se o formato nao tem
// conversao nativa (paleta de 8 bits, YUV, bump: o jogo nao os usa nos arquivos, docs/N7-PLANO.md secao 12).
bool convert_texture(const TextureKey& k, std::vector<uint32_t>& argb);

// Identidade estavel por conteudo (native/render/texture_replacement.h) da textura que `k` descreve agora na VRAM:
// a chave futura das substituicoes HD. false se o formato nao tem conversao nativa.
bool content_id(const TextureKey& k, nr::TextureId& id);

void         install(Mode m, uint32_t every = 1);  // instala no pvr_render e zera as estatisticas (Off desinstala e descarta)
const Frame& last_frame();                         // a IR do ultimo render
const Stats& stats();
std::string  summary();
void         set_compare_dump(const std::string& dir, uint32_t every);  // BMP das comparacoes (Mode::Compare)
// HH15.7b: o ListSink nativo (transitional/kamui_device, modo nativo). Registrado, em todo render pelo caminho do TA a
// captura confere a IR que ele decodifica direto com a montada dos retornos do decodificador do legado
// (Stats::list_*); nullptr desliga.
void set_list_sink(const native::render::kamui::ListSink* sink);
// HH15.7c: a imagem do ultimo render pelo caminho do TA com o estado nativo registrado (para o comparador da gravacao
// do framebuffer); nullptr sem ela.
const std::vector<uint32_t>* last_image(uint32_t& width, uint32_t& height);
// Diagnostico (HH_RENDER_IR_GUARDAR=<render>:<arquivo>, render_devices.h) com os texels do runtime.
bool save_frame_dump(const std::string& path, const Frame& f);

// HH13: texels de 16 bits que uma textura linear le (stride 0 = a largura): a linha y comeca em y * stride e vai ate
// x = largura - 1, entao a ultima termina em (altura - 1) * stride + max(stride, largura). 0 para dimensoes nulas.
size_t linear_texture_texels(uint32_t width, uint32_t height, uint32_t stride);

// Texels de uma textura com o fallback: as conversoes nativas ou, sem elas, o decodificador do legado.
bool texture_argb(const TextureKey& k, std::vector<uint32_t>& argb, bool& legacy);
// Os bytes que definem a textura na VRAM e a paleta usada (para o cache do backend saber se ela mudou).
void texture_source(const TextureKey& k, std::vector<uint8_t>& raw);
// As duas acima como fonte dos dispositivos (render_devices.h).
TextureSource& legacy_textures();

} // namespace hh::transitional::render_capture
