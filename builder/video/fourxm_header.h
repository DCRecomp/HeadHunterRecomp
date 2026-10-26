// SPDX-License-Identifier: MIT
#pragma once
// HH12: cabecalho do 4X Movie (RIFF "4XMV", empacotador 4xmovie 4.5.0.811). Leitura propria, independente do FFmpeg:
// o chunk `vtrk` (u32 little-endian) traz [4] = numero de quadros (== numero de LIST FRAM nos 53 filmes),
// [7..8] = largura x altura codificadas e [9..10] = largura x altura exibidas (analysis/formatos, 2026-10-24;
// confirmado no objeto do filme do jogo pelo oraculo do Flycast: 480 x 360 exibidos para o SCENE0 de 480 x 368).
#include <cstdint>
#include <filesystem>
#include <string>

namespace hh::builder::video {

struct FourXmHeader {
    uint32_t frames = 0;
    uint32_t codedWidth = 0, codedHeight = 0;
    uint32_t shownWidth = 0, shownHeight = 0;
};

// Procura o chunk vtrk nos primeiros 64 KiB do arquivo. Falha com mensagem clara.
bool read_4xm_header(const std::filesystem::path& file, FourXmHeader& h, std::string* err = nullptr);

} // namespace hh::builder::video
