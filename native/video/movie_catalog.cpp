// SPDX-License-Identifier: MIT
// Derivado de recomp-BStinger native/video/movie_catalog.cpp (MIT, Copyright (c) 2026 Blue Stinger
// Recompiled contributors; ver third_party/licenses/recomp-BStinger-LICENSE.txt).
#include "video/movie_catalog.h"

#include <toml++/toml.hpp>

#include <algorithm>
#include <cctype>
#include <system_error>

namespace hh::native::video {

namespace fs = std::filesystem;

namespace {

std::string upper(std::string_view s) {
    std::string r(s);
    for (char& c : r) c = char(std::toupper(static_cast<unsigned char>(c)));
    return r;
}

std::string lowerLang(std::string_view s) {
    std::string r(s);
    for (char& c : r) c = char(std::tolower(static_cast<unsigned char>(c)));
    return r;
}

bool isFile(const fs::path& p) {
    std::error_code ec;
    return fs::is_regular_file(p, ec);
}

// Caminho relativo a pasta do manifesto, sem sair dela: nada absoluto, de unidade ou com "..".
bool safeRelative(const std::string& s) {
    if (s.empty()) return false;
    const fs::path p(std::u8string(s.begin(), s.end()));
    if (p.is_absolute() || p.has_root_name() || p.has_root_directory()) return false;
    for (const auto& part : p)
        if (part == "..") return false;
    return true;
}

// Depois de resolver juncoes e links: o alvo continua dentro da pasta do manifesto.
bool contained(const fs::path& base, const fs::path& target) {
    std::error_code ec;
    const fs::path  b = fs::weakly_canonical(base, ec);
    if (ec) return false;
    const fs::path t = fs::weakly_canonical(target, ec);
    if (ec) return false;
    auto bi = b.begin(), ti = t.begin();
    for (; bi != b.end(); ++bi, ++ti)
        if (ti == t.end() || *bi != *ti) return false;
    return true;
}

// [[filme]] original, arquivo, quadros, fps, largura, altura, sha256, legendas = { pt-br = "..." }
void readManifest(const fs::path& path, bool mod, std::map<std::string, MovieEntry>& out, std::vector<std::string>& problems) {
    if (!isFile(path)) return;
    toml::table t;
    try {
        t = toml::parse_file(path.wstring());
    } catch (const toml::parse_error& e) {
        problems.push_back(path.filename().string() + ": " + std::string(e.description()));
        return;
    }
    const auto* list = t["filme"].as_array();
    if (!list) return;
    for (const auto& node : *list) {
        const auto* f = node.as_table();
        if (!f) continue;
        const auto orig   = (*f)["original"].value<std::string>();
        const auto file   = (*f)["arquivo"].value<std::string>();
        const bool absent = (*f)["ausente"].value_or(false);
        if (!orig || orig->empty()) {
            problems.push_back(path.filename().string() + ": entrada [[filme]] sem original");
            continue;
        }
        const std::string k = MovieCatalog::key(*orig);
        if (absent && !mod) {  // nao convertido: so a chave (um mod pode fornece-lo)
            MovieEntry e;
            e.original = k, e.absent = true;
            out[k] = std::move(e);
            continue;
        }
        if (!file || file->empty()) {
            problems.push_back(path.filename().string() + ": filme " + k + " sem arquivo");
            continue;
        }
        if (!safeRelative(*file)) {
            problems.push_back(path.filename().string() + ": filme " + k + " com caminho fora da pasta (recusado): " + *file);
            continue;
        }
        MovieEntry e = mod && out.count(k) ? out[k] : MovieEntry{};
        e.original   = k;
        e.file       = path.parent_path() / fs::path(std::u8string(file->begin(), file->end()));
        if (!contained(path.parent_path(), e.file)) {
            problems.push_back(path.filename().string() + ": filme " + k + " aponta para fora da pasta por juncao ou link (recusado)");
            continue;
        }
        e.mod    = mod;
        e.absent = false;
        if (mod) e.frames = 0, e.width = 0, e.height = 0, e.fps = 0, e.sha256.clear(), e.videoEnd100ns = e.audioEnd100ns = 0;
        e.frames = uint32_t((*f)["quadros"].value_or(int64_t(e.frames)));
        e.width  = uint32_t((*f)["largura"].value_or(int64_t(e.width)));
        e.height = uint32_t((*f)["altura"].value_or(int64_t(e.height)));
        e.shownWidth  = uint32_t((*f)["largura_exibida"].value_or(int64_t(mod ? 0 : e.shownWidth)));
        e.shownHeight = uint32_t((*f)["altura_exibida"].value_or(int64_t(mod ? 0 : e.shownHeight)));
        if (!mod)
            e.sourceBytesPerSecond = uint32_t((*f)["taxa_audio_fonte"].value_or(int64_t(0)) * (*f)["canais"].value_or(int64_t(0)) * 2);
        e.fps    = (*f)["fps"].value_or(e.fps);
        e.sha256 = (*f)["sha256"].value_or(e.sha256);
        e.audioDelaySamples = int32_t((*f)["atraso_audio_amostras"].value_or(int64_t(0)));
        if (!mod) {
            e.videoEnd100ns = (*f)["fim_video_100ns"].value_or(int64_t(0));
            e.audioEnd100ns = (*f)["fim_audio_100ns"].value_or(int64_t(0));
        }
        if (const auto* subs = (*f)["legendas"].as_table())
            for (const auto& [lang, v] : *subs)
                if (auto s = v.value<std::string>()) {
                    const fs::path sp = path.parent_path() / fs::path(std::u8string(s->begin(), s->end()));
                    if (safeRelative(*s) && contained(path.parent_path(), sp)) e.subtitles[lowerLang(lang.str())] = sp;
                    else problems.push_back(path.filename().string() + ": legenda com caminho fora da pasta (recusada): " + *s);
                }
        if (!isFile(e.file)) problems.push_back(path.filename().string() + ": filme " + k + " sem o arquivo " + e.file.filename().string());
        out[k] = std::move(e);
    }
}

// "MOVIES/SCENE0.4XM" -> "SCENE0"; "_DISCO2/MOVIES/SCENE0.4XM" -> "SCENE0.D2" (nome do mod)
std::string modStem(const std::string& k) {
    std::string name = k.substr(k.rfind('/') + 1);
    if (const size_t dot = name.rfind('.'); dot != std::string::npos) name.resize(dot);
    return k.rfind("_DISCO2/", 0) == 0 ? name + ".D2" : name;
}

} // namespace

std::string MovieCatalog::key(std::string_view original) {
    std::string k = upper(original);
    std::replace(k.begin(), k.end(), '\\', '/');
    while (!k.empty() && k.front() == '/') k.erase(k.begin());
    return k;
}

MovieCatalog MovieCatalog::load(const CatalogOptions& o) {
    MovieCatalog c;
    readManifest(o.dataDir / "manifest.toml", false, c.entries_, c.problems_);
    if (o.modsDir.empty()) return c;
    readManifest(o.modsDir / "manifest.toml", true, c.entries_, c.problems_);
    // mod pelo nome (sem manifesto): mods\video\<NOME>[.D2].mp4 e <NOME>[.D2].<idioma>.srt/.vtt; o .D2 so vale para a
    // variante do disco 2 e tem prioridade sobre o nome simples
    for (auto& [k, e] : c.entries_) {
        const std::string stem  = modStem(k);
        const bool        disc2 = stem.ends_with(".D2");
        const std::string plain = disc2 ? stem.substr(0, stem.size() - 3) : stem;
        std::error_code   ec;
        fs::path          best;
        bool              bestExact = false;
        for (const auto& f : fs::directory_iterator(o.modsDir, ec)) {
            if (!f.is_regular_file(ec)) continue;
            // um link ou juncao com o nome certo nao pode apontar para fora de mods\video (revisao Codex)
            if (!contained(o.modsDir, f.path())) {
                c.problems_.push_back(f.path().filename().string() + ": aponta para fora da pasta de mods (recusado)");
                continue;
            }
            const std::string name = upper(f.path().filename().string());
            if (name == stem + ".MP4") best = f.path(), bestExact = true;
            else if (disc2 && name == plain + ".MP4" && !bestExact) best = f.path();
            for (const char* ext : {".SRT", ".VTT"}) {  // NOME.<idioma>.srt
                const std::string pre = stem + ".";
                if (name.size() > pre.size() + 4 && name.rfind(pre, 0) == 0 && name.ends_with(ext)) {
                    std::string lang = f.path().filename().string().substr(pre.size());
                    lang             = lowerLang(lang.substr(0, lang.size() - 4));
                    if (!lang.empty() && lang.find('.') == std::string::npos) e.subtitles[lang] = f.path();
                }
            }
        }
        if (!best.empty() && !e.mod) {
            e.file = best, e.mod = true, e.absent = false, e.audioDelaySamples = 0;
            e.frames = e.width = e.height = e.shownWidth = e.shownHeight = 0, e.fps = 0, e.sha256.clear();
            e.videoEnd100ns = e.audioEnd100ns = 0;
        }
    }
    return c;
}

const MovieEntry* MovieCatalog::find(std::string_view original) const {
    const auto it = entries_.find(key(original));
    return it == entries_.end() ? nullptr : &it->second;
}

} // namespace hh::native::video
