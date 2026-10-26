// SPDX-License-Identifier: MIT
// Derivado de recomp-BStinger native/video/movie_catalog.h (MIT, Copyright (c) 2026 Blue Stinger
// Recompiled contributors; ver third_party/licenses/recomp-BStinger-LICENSE.txt).
#pragma once
// HH12 (docs/planos/HH12-PLANO.md): catalogo dos filmes pelo caminho do original na instalacao. O Builder grava
// data\video\manifest.toml com os filmes convertidos do disco do usuario (a referencia de fidelidade); a chave e o
// caminho relativo a data\disc, em maiusculas ("MOVIES/SCENE0.4XM"; a variante do disco 2 de um nome repetido fica
// em "_DISCO2/MOVIES/SCENE0.4XM", como na instalacao unica do HH08). Mods em mods\video substituem por nome sem
// recompilar: <NOME>.mp4 vale para o nome nos dois discos; <NOME>.D2.mp4 so para a variante do disco 2. Legendas
// (.srt/.vtt) por idioma seguem a mesma regra. Sem nada do Dreamcast: o caminho original e so uma chave.
#include <cstdint>
#include <filesystem>
#include <map>
#include <string>
#include <string_view>
#include <vector>

namespace hh::native::video {

struct MovieEntry {
    std::string           original;     // caminho relativo a data\disc, maiusculo (p.ex. "MOVIES/SCENE0.4XM")
    std::filesystem::path file;         // arquivo a tocar (absoluto)
    bool                  mod = false;  // veio de mods\video (modo modificado: duracao do proprio arquivo)
    // metadados da conversao (modo fiel); zero quando o mod nao informa
    uint32_t    frames = 0, width = 0, height = 0;
    uint32_t    shownWidth = 0, shownHeight = 0;  // area exibida pelo jogo (canto superior esquerdo; 0 = o quadro todo)
    uint32_t    sourceBytesPerSecond = 0;         // audio da fonte em PCM 16 bits (taxa x canais x 2; 0 sem audio)
    double      fps = 0;
    std::string sha256;              // do arquivo do Builder (diagnostico de recurso trocado ou corrompido)
    bool        absent = false;      // nao convertido (e nenhum mod o fornece)
    int32_t     audioDelaySamples = 0;  // atraso do audio no arquivo (priming do AAC), por canal
    int64_t     videoEnd100ns = 0, audioEnd100ns = 0;  // fins de cada stream na fonte original
    std::map<std::string, std::filesystem::path> subtitles;  // idioma -> .srt/.vtt
};

struct CatalogOptions {
    std::filesystem::path dataDir;  // data\video (manifesto do Builder)
    std::filesystem::path modsDir;  // mods\video (opcional)
};

class MovieCatalog {
public:
    // Le os manifestos. Erros de sintaxe ou campos invalidos nao abortam: vao para `problems` e a
    // entrada e ignorada (o filme cai no diagnostico de recurso ausente).
    static MovieCatalog load(const CatalogOptions& o);

    // Pelo caminho original (sem diferenciar caixa; '\' ou '/'); nullptr se o catalogo nao o conhece.
    const MovieEntry*               find(std::string_view original) const;
    const std::vector<std::string>& problems() const { return problems_; }
    size_t                          size() const { return entries_.size(); }
    // todas as entradas (inspecao de recursos do Developer; so leitura)
    const std::map<std::string, MovieEntry>& all() const { return entries_; }

    // Chave normalizada: maiusculas, '/' e sem barra no inicio.
    static std::string key(std::string_view original);

private:
    std::map<std::string, MovieEntry> entries_;
    std::vector<std::string>          problems_;
};

} // namespace hh::native::video
