// SPDX-License-Identifier: MIT
#include "hh_verified_files.h"

#include "hh_extract/hash.hpp"

#include <cstdio>
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <vector>

namespace hh {

namespace nf = native::files;

VerifiedFileSystem::VerifiedFileSystem(nf::DataFileSystem& inner, const std::filesystem::path& manifest)
    : inner_(inner) {
    std::ifstream f(manifest);
    if (!f) throw std::runtime_error("manifesto dos dados ausente: " + manifest.string());
    // "file <lba> <bytes> <sha256> <caminho>" (hh-extract --runtime; o caminho pode ter espacos)
    std::string line;
    while (std::getline(f, line)) {
        if (line.rfind("file ", 0) != 0) continue;
        std::istringstream is(line.substr(5));
        uint64_t           lba = 0, size = 0;
        std::string        sha, name;
        if (!(is >> lba >> size >> sha) || sha.size() != 64) throw std::runtime_error("manifesto invalido: " + line);
        std::getline(is >> std::ws, name);
        expected_[nf::canonical_path(name)] = {size, sha};
    }
    if (expected_.empty()) throw std::runtime_error("manifesto sem arquivos: " + manifest.string());
    for (const auto& m : inner_.mods()) modded_.insert(m.logical);
    // Presenca e tamanho de todos os arquivos ja no boot (rapido; o conteudo e conferido na
    // primeira abertura): um arquivo do disco 2 ou uma variante ausente nunca vira "serve o do
    // disco 1" em silencio (revisao Codex HH08).
    for (const auto& [name, e] : expected_) {
        if (modded_.count(name)) continue;
        const auto h = inner_.open(name);
        if (!h) throw std::runtime_error("arquivo do jogo ausente ou ilegivel: " + name + ". Refaca a instalacao com o Builder.");
        const uint64_t size = inner_.size(h.value()).value();
        inner_.close(h.value());
        if (size != e.size)
            throw std::runtime_error("arquivo do jogo com tamanho errado: " + name + " (" + std::to_string(size) +
                                     ", esperado " + std::to_string(e.size) + "). Refaca a instalacao com o Builder.");
    }
}

native::Result<nf::FileHandle> VerifiedFileSystem::open(std::string_view path) {
    const std::string name = nf::canonical_path(path);
    auto              h    = inner_.open(path);
    if (!h || ok_.count(name) || modded_.count(name)) return h;
    const auto it = expected_.find(name);
    auto       reject = [&](const std::string& why) -> native::Result<nf::FileHandle> {
        inner_.close(h.value());
        problem_ = name + ": " + why;
        std::fprintf(stderr, "[arquivos] recusado: %s\n", problem_.c_str());
        return native::Error::Corrupt;
    };
    if (it == expected_.end()) return reject("nao esta no manifesto do disco");
    const uint64_t size = inner_.size(h.value()).value();
    if (size != it->second.size)
        return reject("tamanho " + std::to_string(size) + ", esperado " + std::to_string(it->second.size));
    Sha256                 sha;
    std::vector<std::byte> buf(1u << 20);
    for (uint64_t off = 0; off < size;) {
        const auto r = inner_.read(h.value(), off, buf);
        if (!r || r.value() == 0) return reject("falha de leitura");
        sha.update(reinterpret_cast<const uint8_t*>(buf.data()), r.value());
        off += r.value();
    }
    if (sha.hexdigest() != it->second.sha256) return reject("conteudo diferente do disco (SHA-256)");
    ok_.insert(name);
    return h;
}

std::string VerifiedFileSystem::verify_all() {
    std::vector<std::string> names;
    for (const auto& [n, e] : expected_) names.push_back(n);
    for (const std::string& n : names) {
        const auto h = open(n);
        if (!h) return problem_.empty() ? n + ": nao abre" : problem_;
        inner_.close(h.value());
    }
    return {};
}

} // namespace hh
