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

// Linha "version <id>" do disc.manifest (hh-extract --runtime); "" se ausente.
std::string manifestVersion(const fs::path& manifest) {
    std::ifstream f(manifest);
    if (!f) throw std::runtime_error("nao foi possivel ler " + u8(manifest) + ". Refaca a instalacao com o Builder.");
    std::string line;
    while (std::getline(f, line))
        if (line.rfind("version ", 0) == 0) return line.substr(8);
    return {};
}

} // namespace

const char* disc_version(int number) {
    switch (number) {
    case 1: return "hedhunter_v1006_a";
    case 2: return "hedhunter_v1006_b";
    default: return nullptr;
    }
}

DiscData resolve_disc(const fs::path& root, int number) {
    const char* want = disc_version(number);
    if (!want) throw std::runtime_error("disco " + std::to_string(number) + " invalido: o Head Hunter tem os discos 1 e 2");
    DiscData d;
    d.number = number;
    std::error_code ec;
    const fs::path sub = root / ("disc" + std::to_string(number));
    if (fs::exists(sub / "disc.manifest", ec)) d.dir = sub;
    else if (fs::exists(root / "disc.manifest", ec)) d.dir = root;  // pasta de um disco
    else
        throw std::runtime_error("dados do disco " + std::to_string(number) + " ausentes em " + u8(root) +
                                 ". Refaca a instalacao com o Builder informando o CDI desse disco.");
    d.version = manifestVersion(d.dir / "disc.manifest");
    if (d.version != want) {
        const std::string other = d.version == disc_version(1) ? " (e o disco 1)"
                                  : d.version == disc_version(2) ? " (e o disco 2)"
                                                                 : "";
        throw std::runtime_error("a pasta " + u8(d.dir) + " nao tem o disco " + std::to_string(number) + ": versao '" +
                                 d.version + "'" + other + ", esperada '" + want +
                                 "'. Refaca a instalacao com o Builder.");
    }
    if (!fs::is_directory(d.dir / "disc", ec))
        throw std::runtime_error("arquivos do disco " + std::to_string(number) + " ausentes em " + u8(d.dir / "disc") +
                                 ". Refaca a instalacao com o Builder.");
    return d;
}

} // namespace hh
