// SPDX-License-Identifier: MIT
#pragma once
// HH15.7c: os texels das texturas que a IR usa, lidos da memoria das texturas (texture_heap.h, caminho de 64 bits) e da
// paleta do estado do quadro (frame_state.h), pelas conversoes nativas (render/texture_convert.h). As regras de alcance
// e de leitura sao as da captura (o que ela lia da VRAM do runtime), mais a paleta de 8 bits (convert_pal8), que antes
// ia ao decodificador do legado.
#include "render/kamui/frame_state.h"
#include "render/kamui/list_sink.h"
#include "render/kamui/texture_heap.h"

#include <cstdint>
#include <vector>

namespace hh::native::render::kamui {

// O deslocamento do maior nivel de um mipmap (em bytes, a partir da base do TCW): o formato (6 = paleta de 8 bits), VQ
// (no indice, depois do codebook) e a largura.
uint32_t mip_offset(uint32_t fmt, bool vq, uint32_t w);

// ARGB8888 linear (o maior nivel); false se o formato nao tem conversao nativa (YUV, bump, paleta de 4 bits com
// mipmap: o decodificador do ListSink nem os desenha).
bool convert_texture(const FrameState& fs, const TextureHeap& heap, const TextureKey& k, std::vector<uint32_t>& argb);
// Os bytes que definem a textura (o alcance que convert_texture le) e a paleta usada, para o cache saber se ela mudou.
void texture_bytes(const FrameState& fs, const TextureHeap& heap, const TextureKey& k, std::vector<uint8_t>& raw);
// Texels que uma textura linear le (stride 0 = a largura): a ultima linha vai ate x = largura - 1.
size_t linear_texture_texels(uint32_t width, uint32_t height, uint32_t stride);

} // namespace hh::native::render::kamui
