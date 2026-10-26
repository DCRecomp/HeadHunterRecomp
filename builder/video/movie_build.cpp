// SPDX-License-Identifier: MIT
// Derivado de recomp-BStinger builder/video/movie_build.cpp (MIT, Copyright (c) 2026 Blue Stinger
// Recompiled contributors; ver third_party/licenses/recomp-BStinger-LICENSE.txt).
#include "video/movie_build.h"

#include "child_process.h"
#include "hh_extract/hash.hpp"

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>

#include <toml++/toml.hpp>

#include <cctype>
#include <cstdio>
#include <fstream>
#include <map>
#include <sstream>

namespace hh::builder::video {

namespace fs = std::filesystem;

namespace {

bool fail(std::string* err, const std::string& m) {
    if (err) *err = m;
    return false;
}

std::string fileSha(const fs::path& p) {
    std::ifstream     f(p, std::ios::binary);
    hh::Sha256        sha;
    std::vector<char> buf(1 << 20);
    while (f) {
        f.read(buf.data(), std::streamsize(buf.size()));
        sha.update(reinterpret_cast<const uint8_t*>(buf.data()), size_t(f.gcount()));
    }
    return sha.hexdigest();
}

std::string upper(std::string s) {
    for (char& c : s) c = char(std::toupper(static_cast<unsigned char>(c)));
    return s;
}

struct Entry {
    std::string   original, file, sourceSha, mp4Sha;
    bool          unconverted = false;
    ConvertResult c;
    Fidelity      f;
};

// filmes do arquivos.manifest: "file <disco> <tamanho> <sha256> <caminho>" com caminho terminado em .4XM
std::vector<std::pair<std::string, std::string>> listMovies(const fs::path& manifest) {
    std::vector<std::pair<std::string, std::string>> out;  // (caminho, sha256)
    std::ifstream                                    in(manifest, std::ios::binary);
    for (std::string l; std::getline(in, l);) {
        if (!l.empty() && l.back() == '\r') l.pop_back();
        if (l.rfind("file ", 0) != 0) continue;
        std::istringstream is(l.substr(5));
        std::string        disc, size, sha, path;
        is >> disc >> size >> sha;
        std::getline(is >> std::ws, path);
        if (upper(path).ends_with(".4XM")) out.push_back({path, sha});
    }
    return out;
}

// manifesto anterior (para reaproveitar): original -> entrada
std::map<std::string, toml::table> previous(const fs::path& manifest, std::string& converter) {
    std::map<std::string, toml::table> out;
    std::error_code                    ec;
    if (!fs::exists(manifest, ec)) return out;
    try {
        const toml::table t = toml::parse_file(manifest.wstring());
        converter           = t["conversor"].value_or(std::string());
        if (const auto* arr = t["filme"].as_array())
            for (const auto& n : *arr)
                if (const auto* e = n.as_table())
                    if (auto o = (*e)["original"].value<std::string>()) out[*o] = *e;
    } catch (const toml::parse_error&) {
    }
    return out;
}

std::string q(const std::string& s) { return "\"" + s + "\""; }

// Troca atomica que substitui o destino (o std::filesystem::rename nao substitui no Windows).
bool replaceFile(const fs::path& from, const fs::path& to) {
    return MoveFileExW(from.c_str(), to.c_str(), MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH) != 0;
}

bool toolMissing(const std::string& e) {
    return e.find("Media Foundation indisponivel") != std::string::npos || e.find("codificador H.264 indisponivel") != std::string::npos ||
           e.find("codificador AAC indisponivel") != std::string::npos || e.find("instale o FFmpeg") != std::string::npos;
}

} // namespace

std::string movie_file_name(const std::string& original) {
    const std::string u    = upper(original);
    std::string       name = u.substr(u.rfind('/') + 1);
    if (const size_t dot = name.rfind('.'); dot != std::string::npos) name.resize(dot);
    return name + (u.rfind("_DISCO2/", 0) == 0 ? ".D2" : "") + ".mp4";
}

bool build_movies(const fs::path& dataDir, const MovieBuildOptions& o, MovieBuildResult& r, std::string* err) {
    r = {};
    std::error_code ec;
    const fs::path  outDir = dataDir / "video", disc = dataDir / "disc";
    // instalacao unica: arquivos.manifest; dados de um disco so (testes e portoes): disc.manifest, com as mesmas linhas
    const bool merged = fs::is_regular_file(dataDir / "arquivos.manifest", ec);
    const auto movies = listMovies(dataDir / (merged ? "arquivos.manifest" : "disc.manifest"));
    if (movies.empty()) return fail(err, "nenhum filme 4X no manifesto dos dados (dados incompletos)");
    fs::create_directories(outDir, ec);
    if (ec) return fail(err, "nao foi possivel criar " + path_text(outDir));
    std::string prevConverter;
    const auto  prev = previous(outDir / "manifest.toml", prevConverter);
    std::vector<Entry> entries;
    for (size_t i = 0; i < movies.size(); ++i) {
        if (o.cancel && o.cancel->load()) return fail(err, "cancelado");
        Entry e;
        e.original = upper(movies[i].first), e.sourceSha = movies[i].second, e.file = movie_file_name(e.original);
        const fs::path src = disc / fs::u8path(movies[i].first), mp4 = outDir / e.file;
        if (o.progress) o.progress(double(i) / double(movies.size()), e.original);
        // a chave do reaproveitamento e o conteudo real da fonte, conferido com o manifesto dos dados (revisao Codex)
        if (const std::string real = fileSha(src); real != e.sourceSha)
            return fail(err, e.original + ": a fonte nao confere com o manifesto dos dados (SHA-256 " + real + ", esperado " +
                                 e.sourceSha + ")");
        // reaproveita: mesma fonte, mesmo conversor e o MP4 intacto
        if (const auto it = prev.find(e.original); it != prev.end() && prevConverter == kConverterTag &&
                                                    it->second["fonte_sha256"].value_or(std::string()) == e.sourceSha &&
                                                    fs::is_regular_file(mp4, ec) &&
                                                    it->second["sha256"].value_or(std::string()) == fileSha(mp4)) {
            const toml::table& t = it->second;
            e.mp4Sha             = t["sha256"].value_or(std::string());
            e.c.frames           = uint32_t(t["quadros"].value_or(int64_t(0)));
            e.c.src.fpsNum       = uint32_t(t["fps_num"].value_or(int64_t(0)));
            e.c.src.fpsDen       = uint32_t(t["fps_den"].value_or(int64_t(1)));
            e.c.src.width        = uint32_t(t["largura"].value_or(int64_t(0)));
            e.c.src.height       = uint32_t(t["altura"].value_or(int64_t(0)));
            e.c.hdr.frames       = e.c.frames;
            e.c.hdr.shownWidth   = uint32_t(t["largura_exibida"].value_or(int64_t(0)));
            e.c.hdr.shownHeight  = uint32_t(t["altura_exibida"].value_or(int64_t(0)));
            e.c.src.sampleRate   = uint32_t(t["taxa_audio_fonte"].value_or(int64_t(0)));
            e.c.src.channels     = uint32_t(t["canais"].value_or(int64_t(0)));
            e.c.videoEnd100ns    = t["fim_video_100ns"].value_or(int64_t(0));
            e.c.audioEnd100ns    = t["fim_audio_100ns"].value_or(int64_t(0));
            e.c.audioSamples     = uint32_t(t["amostras_audio"].value_or(int64_t(0)));
            e.f.audioDelaySamples = int32_t(t["atraso_audio_amostras"].value_or(int64_t(0)));
            e.f.psnrMin = t["psnr_min"].value_or(0.0), e.f.psnrAvg = t["psnr_med"].value_or(0.0);
            e.f.audioSnr = t["snr_audio"].value_or(0.0);
            ++r.reused;
            entries.push_back(e);
            continue;
        }
        const fs::path tmp = outDir / (e.file + ".tmp.mp4");
        fs::remove(tmp, ec);
        std::string cerr;
        ConvertOptions co = o.conv;
        co.cancel         = o.cancel;
        if (!r.unavailable.empty() || !convert_4xm(src, tmp, co, e.c, &cerr)) {
            // sem FFmpeg ou sem o codificador do Windows nenhum filme converte: registra uma vez e segue sem filmes
            if (r.unavailable.empty() && !toolMissing(cerr)) return fail(err, e.original + ": " + cerr);
            if (r.unavailable.empty()) {
                r.unavailable = cerr;
                if (o.log) o.log("filmes NAO convertidos: " + cerr + " (o jogo pula os filmes; corrija e gere de novo)");
            }
            fs::remove(tmp, ec);
            e.unconverted = true;
            entries.push_back(e);
            continue;
        }
        if (o.verify) {
            if (!verify_4xm(src, tmp, co, e.f, &cerr)) return fail(err, e.original + ": " + cerr);
            if (e.f.framesMp4 != e.f.framesRef || e.f.framesRef != e.c.frames)
                return fail(err, e.original + ": o MP4 tem " + std::to_string(e.f.framesMp4) + " quadros, a fonte " + std::to_string(e.f.framesRef));
        }
        if (!replaceFile(tmp, mp4)) return fail(err, "nao foi possivel gravar " + path_text(mp4));
        e.mp4Sha = fileSha(mp4);
        if (o.log) {
            char b[320];
            std::snprintf(b, sizeof b, "filme %s: %u quadros %ux%u, PSNR med %.2f min %.2f dB, audio atraso %d amostras SNR %.1f dB",
                          e.original.c_str(), e.c.frames, e.c.src.width, e.c.src.height, e.f.psnrAvg, e.f.psnrMin,
                          e.f.audioDelaySamples, e.f.audioSnr);
            o.log(b);
        }
        ++r.converted;
        entries.push_back(e);
    }
    // manifesto (gravado por troca: o catalogo nunca ve um manifesto pela metade)
    std::ostringstream m;
    m << "# Gerado pelo Head Hunter Builder a partir dos discos do usuario (docs/planos/HH12-PLANO.md). Nao versionar.\n"
      << "versao = 1\nconversor = " << q(kConverterTag) << "\n";
    for (const Entry& e : entries) {
        m << "\n[[filme]]\noriginal = " << q(e.original) << "\n";
        if (e.unconverted) {  // o catalogo le como ausente; um mod ainda pode fornece-lo
            m << "ausente = true  # nao convertido: sem FFmpeg ou sem codificador de video no Windows\nfonte_sha256 = " << q(e.sourceSha) << "\n";
            continue;
        }
        const double fps = e.c.src.fpsDen ? double(e.c.src.fpsNum) / e.c.src.fpsDen : 0;
        char         b[1200];
        std::snprintf(b, sizeof b,
                      "arquivo = \"%s\"\nquadros = %u\nfps = %.6f\nfps_num = %u\nfps_den = %u\nlargura = %u\naltura = %u\n"
                      "largura_exibida = %u\naltura_exibida = %u\n"
                      "taxa_audio_fonte = %u\ncanais = %u\nfim_video_100ns = %lld\nfim_audio_100ns = %lld\namostras_audio = %u\n"
                      "atraso_audio_amostras = %d\npsnr_min = %.3f\npsnr_med = %.3f\nsnr_audio = %.2f\nsha256 = \"%s\"\n"
                      "fonte_sha256 = \"%s\"\n",
                      e.file.c_str(), e.c.frames, fps, e.c.src.fpsNum, e.c.src.fpsDen, e.c.src.width, e.c.src.height,
                      e.c.hdr.shownWidth, e.c.hdr.shownHeight,
                      e.c.src.sampleRate, e.c.src.channels, static_cast<long long>(e.c.videoEnd100ns),
                      static_cast<long long>(e.c.audioEnd100ns), e.c.audioSamples, e.f.audioDelaySamples, e.f.psnrMin,
                      e.f.psnrAvg, e.f.audioSnr, e.mp4Sha.c_str(), e.sourceSha.c_str());
        m << b;
    }
    const fs::path man = outDir / "manifest.toml", mtmp = outDir / "manifest.toml.tmp";
    {
        std::ofstream f(mtmp, std::ios::binary | std::ios::trunc);
        f << m.str();
        if (!f) return fail(err, "nao foi possivel gravar " + path_text(mtmp));
    }
    if (!replaceFile(mtmp, man)) return fail(err, "nao foi possivel gravar " + path_text(man));
    if (o.progress) o.progress(1.0, "filmes prontos");
    return true;
}

} // namespace hh::builder::video
