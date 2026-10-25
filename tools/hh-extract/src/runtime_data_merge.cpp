// SPDX-License-Identifier: MIT
// HH08: os dois discos do Head Hunter numa instalacao so (runtime_data.hpp, merge_runtime_discs).
#include "hh_extract/runtime_data.hpp"

#include <fstream>
#include <map>
#include <system_error>

namespace hh::disc {

namespace fs = std::filesystem;

namespace {

std::string upper(std::string s) {
    for (char& c : s) c = char(std::toupper(static_cast<unsigned char>(c)));
    return s;
}

void place(const fs::path& from, const fs::path& to, bool hardlinks) {
    std::error_code ec;
    fs::create_directories(to.parent_path(), ec);
    if (fs::exists(to, ec)) throw RuntimeDataError("juncao dos discos: destino ja existe: " + to.string());
    if (hardlinks) fs::create_hard_link(from, to, ec);
    else fs::rename(from, to, ec);
    if (ec) throw RuntimeDataError("juncao dos discos: " + from.string() + " -> " + to.string() + ": " + ec.message());
}

} // namespace

MergeResult merge_runtime_discs(const fs::path& dir1, const fs::path& dir2, bool hardlinks) {
    const RuntimeManifest m1 = load_runtime_manifest(dir1), m2 = load_runtime_manifest(dir2);
    if (m1.versionId != "hedhunter_v1006_a" || m2.versionId != "hedhunter_v1006_b")
        throw RuntimeDataError("juncao dos discos: esperado o disco 1 (hedhunter_v1006_a) e o disco 2 "
                               "(hedhunter_v1006_b), recebido " + m1.versionId + " e " + m2.versionId);
    MergeResult r;
    r.version1 = m1.versionId, r.version2 = m2.versionId;
    std::map<std::string, const RuntimeFile*> byName;  // nome canonico (maiusculas) -> arquivo do disco 1
    for (const auto& f : m1.files) byName[upper(f.path)] = &f;
    struct Entry {
        uint32_t    size;
        std::string sha, path;
    };
    std::vector<Entry> out;
    for (const auto& f : m1.files) out.push_back({f.size, f.sha256, f.path});
    for (const auto& f : m2.files) {
        if (upper(f.path) == upper(m2.bootFile)) continue;  // o executavel vem do disco 1
        const auto it = byName.find(upper(f.path));
        if (it != byName.end() && it->second->sha256 == f.sha256 && it->second->size == f.size) {
            ++r.identical;
            continue;
        }
        const std::string dst = it == byName.end() ? f.path : std::string(kDisc2Variants) + "/" + f.path;
        place(dir2 / "disc" / fs::path(reinterpret_cast<const char8_t*>(f.path.c_str())),
              dir1 / "disc" / fs::path(reinterpret_cast<const char8_t*>(dst.c_str())), hardlinks);
        out.push_back({f.size, f.sha256, dst});
        ++(it == byName.end() ? r.added : r.variants);
    }
    std::ofstream o(dir1 / kMergedManifest, std::ios::binary | std::ios::trunc);
    o << "hh-arquivos 1\n"
         "# Arquivos do Head Hunter (discos 1 e 2 juntos) preparados pelo Builder. Nao publique.\n"
      << "disco1 " << m1.versionId << "\ndisco2 " << m2.versionId << "\n";
    for (const auto& e : out) o << "file 0 " << e.size << " " << e.sha << " " << e.path << "\n";
    if (!o) throw RuntimeDataError("juncao dos discos: nao foi possivel gravar " + (dir1 / kMergedManifest).string());
    return r;
}

} // namespace hh::disc
