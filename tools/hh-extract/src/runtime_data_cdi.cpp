// SPDX-License-Identifier: MIT
// Derivado de recomp-BStinger tools/extractor/runtime_data.cpp (MIT, Copyright (c) 2026 Blue Stinger
// Recompiled contributors; ver third_party/licenses/recomp-BStinger-LICENSE.txt).
// Preparacao e conferencia dos dados de execucao a partir do CDI (Builder, hh-extract). Fora do
// produto: o HeadHunter.exe nao pode ligar este objeto (teste hh07_produto_sem_cdi).
#include "hh_extract/runtime_data.hpp"
#include "hh_extract/cdi.hpp"
#include "hh_extract/hash.hpp"
#include "hh_extract/ip_bin.hpp"
#include "hh_extract/iso9660.hpp"
#include "hh_extract/known_versions.hpp"
#include "hh_extract/scramble.hpp"
#include <algorithm>
#include <cctype>
#include <cstdint>
#include <cstring>
#include <fstream>
#include <map>
#include <sstream>
#include "runtime_data_detail.hpp"

namespace fs = std::filesystem;

namespace hh::disc {

using namespace detail;

// --- preparacao ---

PrepareResult prepare_runtime_data(hh::CdiImage& img, const fs::path& dir, const ProgressFn& progress,
                                   const std::atomic<bool>* cancel) {
    PrepareResult res;
    auto          say = [&](double f, const std::string& w) {
        if (progress) progress(f, w);
    };
    const Track& boot = img.bootTrack();
    std::vector<uint8_t> ipbin(kIpBinSize);
    for (uint32_t i = 0; i < kIpBinSize / kSector; ++i) img.readUserData(boot.start_lba + i, ipbin.data() + i * kSector);
    const IpBin ip = parseIpBin(ipbin.data(), ipbin.size());
    Iso9660     iso(img, boot.start_lba + 16);
    const auto* bootEntry = iso.find(ip.boot_file);
    if (!bootEntry || bootEntry->is_dir) throw RuntimeDataError("executavel de boot " + ip.boot_file + " nao encontrado");

    // versao: hash do executavel (como no disco ou desembaralhado)
    const std::vector<uint8_t> raw = iso.readAll(*bootEntry);
    std::string                bootSha;
    bool                       scrambled = false;
    for (int pass = 0; pass < 2 && res.versionId.empty(); ++pass) {
        const std::vector<uint8_t> d = pass ? descramble(raw) : raw;
        const std::string          h = sha256Hex(d.data(), d.size());
        for (const auto& v : kKnownVersions)
            if (h == v.boot_sha256) res.versionId = v.id, bootSha = h, scrambled = pass == 1;
    }
    if (res.versionId.empty())
        throw RuntimeDataError("versao nao suportada: " + ip.product + " " + ip.version +
                               " (o executavel de boot nao tem o hash da versao recompilada)");

    std::error_code ec;
    // HH05 (revisao Codex): a pasta de saida precisa ser nova ou vazia e nao pode ser link ou
    // juncao; assim nenhum link preexistente dentro dela desvia a gravacao para fora do destino
    // (os caminhos do disco ja sao validados lexicalmente por safeIsoPath).
    if (fs::is_symlink(fs::symlink_status(dir, ec)))
        throw RuntimeDataError("a pasta de dados nao pode ser um link: " + pathText(dir));
    if (fs::exists(dir, ec) && !fs::is_empty(dir, ec))
        throw RuntimeDataError("a pasta de dados precisa ser nova ou vazia: " + pathText(dir));
    fs::create_directories(dir / "disc", ec);
    if (ec) throw RuntimeDataError("nao foi possivel criar " + pathText(dir) + " (" + ec.message() + ")");

    // faixas e mapa de setores cobertos por arquivos
    uint32_t maxLba = 0;
    for (const auto& t : img.tracks()) maxLba = std::max(maxLba, t.start_lba + t.length);
    std::vector<uint8_t> covered(maxLba, 0);  // 1 = arquivo; 2 = arquivo com sobra nao nula (vai para meta)
    uint64_t             total = 0;
    for (const auto& e : iso.entries())
        if (!e.is_dir) total += e.size;

    std::ostringstream files;
    uint64_t           done = 0;
    uint8_t            sec[kSector];
    for (const auto& e : iso.entries()) {
        if (e.is_dir) continue;
        checkCancel(cancel);
        if (!safeIsoPath(e.path)) throw RuntimeDataError("caminho invalido no disco: " + e.path);
        const fs::path dst = dir / "disc" / u8path(e.path);
        fs::create_directories(dst.parent_path());
        hh::Sha256  sha;
        std::ofstream f(dst, std::ios::binary | std::ios::trunc);
        iso.read(e, [&](const uint8_t* p, size_t n) {
            f.write(reinterpret_cast<const char*>(p), std::streamsize(n));
            sha.update(p, n);
        });
        f.close();
        if (!f) throw RuntimeDataError("falha ao gravar " + pathText(dst));
        const uint32_t n = sectorsOf(e.size);
        for (uint32_t i = 0; i < n && e.lba + i < maxLba; ++i) covered[e.lba + i] = std::max<uint8_t>(covered[e.lba + i], 1);
        if (e.size % kSector && e.lba + n - 1 < maxLba) {  // sobra do ultimo setor
            img.readUserData(e.lba + n - 1, sec);
            const uint32_t tail = e.size % kSector;
            if (std::any_of(sec + tail, sec + kSector, [](uint8_t b) { return b != 0; })) covered[e.lba + n - 1] = 2;
        }
        files << "file " << e.lba << " " << e.size << " " << sha.hexdigest() << " " << e.path << "\n";
        ++res.files;
        res.bytes += e.size;
        done += e.size;
        say(total ? 0.7 * double(done) / double(total) : 0.7, "arquivos do disco: " + std::to_string(res.files));
    }

    // setores que nao vem de arquivo (ou cuja sobra nao e nula): so os nao nulos
    {
        std::ofstream idx(dir / kMetaIdx, std::ios::binary | std::ios::trunc);
        std::ofstream bin(dir / kMetaBin, std::ios::binary | std::ios::trunc);
        uint64_t      dataSectors = 0, scanned = 0;
        for (const auto& t : img.tracks())
            if (t.isData()) dataSectors += t.length;
        for (const auto& t : img.tracks()) {
            if (!t.isData()) continue;
            for (uint32_t lba = t.start_lba; lba < t.start_lba + t.length; ++lba, ++scanned) {
                if ((scanned & 0x3FFF) == 0) {
                    checkCancel(cancel);
                    say(0.7 + 0.3 * double(scanned) / double(dataSectors), "setores do sistema e diretorios");
                }
                if (covered[lba] == 1) continue;
                img.readUserData(lba, sec);
                if (covered[lba] != 2 && std::all_of(sec, sec + kSector, [](uint8_t b) { return b == 0; })) continue;
                idx.write(reinterpret_cast<const char*>(&lba), 4);
                bin.write(reinterpret_cast<const char*>(sec), kSector);
                ++res.metaSectors;
            }
        }
        idx.close();
        bin.close();
        if (!idx || !bin) throw RuntimeDataError("falha ao gravar os setores do sistema em " + pathText(dir));
    }

    // manifesto por ultimo (presenca = dados completos), gravado por troca
    {
        const fs::path tmp = dir / (std::string(kManifest) + ".tmp");
        std::ofstream  m(tmp, std::ios::binary | std::ios::trunc);
        m << "hh-runtime-data " << kRuntimeDataFormat << "\n"
          << "# Dados do Head Hunter preparados pelo Builder a partir do CDI do usuario. Nao publique.\n"
          << "version " << res.versionId << "\n"
          << "boot " << ip.boot_file << " " << bootSha << " " << (scrambled ? 1 : 0) << " " << boot.start_lba << "\n"
          << "meta " << fileSha256(dir / kMetaIdx) << " " << fileSha256(dir / kMetaBin) << "\n";
        for (const auto& t : img.tracks())
            m << "track " << t.number << " " << t.session << " " << t.start_lba << " " << t.length << " "
              << (t.isData() ? 1 : 0) << "\n";
        m << files.str();
        m.close();
        if (!m) throw RuntimeDataError("falha ao gravar " + std::string(kManifest));
        fs::rename(tmp, dir / kManifest);
    }
    say(1.0, "dados prontos");
    return res;
}

std::string verify_runtime_data(hh::CdiImage& img, const fs::path& dir, const ProgressFn& progress,
                                const std::atomic<bool>* cancel) {
    RuntimeDisc rd(dir);
    if (rd.manifest().tracks.size() != img.tracks().size()) return "numero de faixas diferente";
    uint64_t dataSectors = 0, scanned = 0;
    for (const auto& t : img.tracks())
        if (t.isData()) dataSectors += t.length;
    uint8_t a[kSector], b[kSector];
    for (const auto& t : img.tracks()) {
        if (!t.isData()) continue;
        for (uint32_t lba = t.start_lba; lba < t.start_lba + t.length; ++lba, ++scanned) {
            if ((scanned & 0x3FFF) == 0) {
                checkCancel(cancel);
                if (progress) progress(double(scanned) / double(dataSectors), "conferindo setores");
            }
            img.readUserData(lba, a);
            if (!rd.readSector(lba, b)) return "setor " + std::to_string(lba) + " ausente nos dados";
            if (std::memcmp(a, b, kSector) != 0) return "setor " + std::to_string(lba) + " diferente da imagem";
        }
    }
    return {};
}

} // namespace hh::disc
