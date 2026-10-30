// SPDX-License-Identifier: MIT
// Derivado de recomp-BStinger runtime/pvr/pvr_video.h (MIT, Copyright (c) 2026 Blue Stinger
// Recompiled contributors; ver third_party/licenses/recomp-BStinger-LICENSE.txt).
#pragma once
// Saida de video do PVR: a imagem que o hardware mostra (nao a que o render escreve),
// lida da VRAM pelos registradores de leitura do framebuffer, e o aviso de VBlank para o
// host apresentar quadros. Independente de janela/plataforma.
#include <cstdint>
#include <vector>

namespace hh {

// Quadro exibido em ARGB8888 (alfa 0xFF):
//  - FB_R_CTRL (0x044): bit 0 liga a leitura, bit 1 duplica linhas, bits 3-2 formato
//    (0 0555, 1 565, 2 888 empacotado, 3 0888);
//  - FB_R_SIZE (0x05C): largura em palavras - 1 (9-0), linhas - 1 (19-10), modulo (29-20;
//    passo de linha = (largura + modulo - 1) palavras);
//  - FB_R_SOF1/SOF2 (0x050/0x054): campo par/impar; com entrelacamento (SPG_CONTROL bit 4)
//    os dois campos sao intercalados (weave);
//  - video desligado (FB_R_CTRL bit 0 = 0) ou em blank (VO_CONTROL 0x0E8 bit 3): preto.
// Devolve largura e altura; o vetor e redimensionado.
void pvr_video_frame(std::vector<uint32_t>& argb, uint32_t& width, uint32_t& height);

// HH15.7d4a: o aviso de VBlank para o host e o servico nativo system/video_bus.h.

} // namespace hh
