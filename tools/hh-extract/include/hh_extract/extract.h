// SPDX-License-Identifier: MIT
// hh-extract: le a imagem de disco e extrai IP.BIN, 1ST_READ.BIN e a arvore
// ISO9660 da sessao de boot para a pasta de saida, com manifest.json de hashes.
#pragma once
#include <filesystem>
#include <ostream>
#include <string>

namespace hh {

struct ExtractOptions {
    std::filesystem::path image;
    std::filesystem::path out = "game-data";
    bool info_only = false; // so mostra faixas, IP.BIN e hash do executavel
    bool force = false;     // extrai mesmo com hash de executavel desconhecido
};

int runExtract(const ExtractOptions& opt, std::ostream& log);

} // namespace hh
