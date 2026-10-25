// SPDX-License-Identifier: MIT
#include "hh_discs.h"

#include <fstream>
#include <stdexcept>

namespace hh {

namespace fs = std::filesystem;

namespace {

std::string u8(const fs::path& p) {
    const std::u8string s = p.u8string();
    return std::string(s.begin(), s.end());
}

// Linha "<chave> <valor>" de um manifesto; "" se ausente.
std::string field(const fs::path& manifest, const std::string& key) {
    std::ifstream f(manifest);
    if (!f) throw std::runtime_error("nao foi possivel ler " + u8(manifest) + ". Refaca a instalacao com o Builder.");
    std::string line;
    while (std::getline(f, line))
        if (line.rfind(key + " ", 0) == 0) return line.substr(key.size() + 1);
    return {};
}

} // namespace

InstallData resolve_install(const fs::path& dir) {
    std::error_code ec;
    if (!fs::exists(dir / "disc.manifest", ec) || !fs::is_directory(dir / "disc", ec))
        throw std::runtime_error("dados do jogo ausentes em " + u8(dir) + ". Refaca a instalacao com o Builder.");
    InstallData d;
    d.dir = dir;
    const std::string v1 = field(dir / "disc.manifest", "version");
    if (v1 != kInstallBootVersion)
        throw std::runtime_error("os dados em " + u8(dir) + " sao '" + v1 + "', esperado o disco 1 ('" +
                                 kInstallBootVersion + "') com o disco 2 junto. Refaca a instalacao com o Builder.");
    const fs::path merged = dir / "arquivos.manifest";
    if (fs::exists(merged, ec)) {
        if (field(merged, "disco2") != "hedhunter_v1006_b")
            throw std::runtime_error("o disco 2 junto em " + u8(dir) + " e de outra versao. Refaca a instalacao com o Builder.");
        d.manifest = merged;
        d.merged   = true;
    } else {
        d.manifest = dir / "disc.manifest";  // dados de desenvolvimento: so o disco 1
    }
    return d;
}

} // namespace hh
