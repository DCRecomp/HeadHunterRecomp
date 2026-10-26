// SPDX-License-Identifier: MIT
// Derivado de recomp-BStinger builder/video/movie_build.h (MIT, Copyright (c) 2026 Blue Stinger
// Recompiled contributors; ver third_party/licenses/recomp-BStinger-LICENSE.txt).
#pragma once
// HH12 (docs/planos/HH12-PLANO.md): passo do Builder que converte os filmes 4X do disco do usuario (os .4XM da
// instalacao unica, data\disc, ja conferidos por hash no arquivos.manifest) em data\video\*.mp4 e grava o
// data\video\manifest.toml lido pelo catalogo do jogo (native/video/movie_catalog.h). Reaproveita filmes ja
// convertidos quando a fonte (SHA-256 do arquivos.manifest), o conversor e o MP4 batem.
#include "video/fourxm_convert.h"

#include <atomic>
#include <filesystem>
#include <functional>
#include <string>
#include <vector>

namespace hh::builder::video {

// Muda quando a conversao muda (forca reconverter os filmes ja preparados).
inline constexpr const char* kConverterTag = "hh12-1 ffmpeg-4xm cfr h264-q100 aac-192-48k";

struct MovieBuildOptions {
    ConvertOptions                                  conv;
    bool                                            verify = true;  // PSNR/SNR de cada filme convertido
    std::function<void(double, const std::string&)> progress;  // 0..1
    std::function<void(const std::string&)>         log;
    const std::atomic<bool>*                        cancel = nullptr;
};

struct MovieBuildResult {
    int converted = 0, reused = 0;
    // Sem FFmpeg ou sem codificador H.264/AAC no Windows: os filmes ficam "nao convertidos" (com o motivo) e o resto
    // da compilacao segue; o jogo os trata como ausentes.
    std::string unavailable;
};

// dataDir = pasta data\ da instalacao unica (arquivos.manifest + disc\); grava em dataDir\video.
bool build_movies(const std::filesystem::path& dataDir, const MovieBuildOptions& o, MovieBuildResult& r,
                  std::string* err = nullptr);

// Nome do MP4 de um filme: "MOVIES/SCENE0.4XM" -> "SCENE0.mp4"; "_DISCO2/MOVIES/SCENE0.4XM" -> "SCENE0.D2.mp4".
std::string movie_file_name(const std::string& original);

} // namespace hh::builder::video
