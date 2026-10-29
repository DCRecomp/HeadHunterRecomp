// SPDX-License-Identifier: MIT
// Derivado de recomp-BStinger native/render/kamui/display_service.h (MIT, Copyright (c) 2026 Blue Stinger
// Recompiled contributors; ver third_party/licenses/recomp-BStinger-LICENSE.txt).
#pragma once
// HH15.7c: a imagem que o video mostra, lida da memoria das texturas pelos registradores de leitura do framebuffer que o
// Kamui programa (o estado do quadro). Servico nativo sobre o estado e a memoria (os mesmos bytes nos mesmos
// enderecos), nao o hardware simulado: a mesma conta da saida de video anterior (pvr_video do runtime).
//  - FB_R_CTRL (0x044): bit 0 liga a leitura, bit 1 duplica linhas, bits 3-2 formato (0 0555, 1 565, 2 888 empacotado,
//    3 0888);
//  - FB_R_SIZE (0x05C): largura em palavras - 1 (9-0), linhas - 1 (19-10), modulo (29-20; passo de linha = largura +
//    modulo - 1 palavras);
//  - FB_R_SOF1/SOF2 (0x050/0x054): campo par/impar; com entrelacamento (SPG_CONTROL bit 4) os dois campos sao
//    intercalados;
//  - leitura desligada ou blank (VO_CONTROL 0x0E8 bit 3): preto.
// O SPG_CONTROL vem do chamador (a temporizacao do video fica fora do estado do quadro ate o HH15.7d).
#include "render/kamui/frame_state.h"
#include "render/kamui/texture_heap.h"

#include <cstdint>
#include <vector>

namespace hh::native::render::kamui {

// O quadro exibido em ARGB8888 (alfa 0xFF); devolve largura e altura, o vetor e redimensionado.
void scanout(const FrameState& fs, uint32_t spg_control, const TextureHeap& heap, std::vector<uint32_t>& argb,
             uint32_t& width, uint32_t& height);

} // namespace hh::native::render::kamui
