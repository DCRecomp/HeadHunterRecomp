// SPDX-License-Identifier: MIT
// HH12: entradas do objeto do filme 4X atendidas pelos MP4 do Builder (estrutura do video_adapter do N5 do BS, MIT,
// Copyright (c) 2026 Blue Stinger Recompiled contributors; ver third_party/licenses/recomp-BStinger-LICENSE.txt).
// Contrato em docs/planos/HH12-PLANO.md.
#include "video_adapter.h"

#include "audio/audio.h"
#include "cpu/sh4_runtime.h"
#include "gdfs_adapter.h"
#include "hh_extract/hash.hpp"
#include "guest_abi.h"
#include "mods/mod_api.h"
#include "pvr/holly_intc.h"
#include "video/movie_catalog.h"
#include "video/movie_playback.h"
#include "video/subtitles.h"

#include <algorithm>
#include <deque>
#include <fstream>
#include <cstdio>
#include <memory>
#include <stdexcept>

namespace hh::transitional::video {

namespace {

namespace nv = native::video;
namespace na = native::audio;

struct State {
    bool                               on = false;
    Options                            o;
    nv::MovieCatalog                   catalog;
    std::unique_ptr<nv::MoviePlayback> playback;  // aberto no open, tocado no play
    const nv::MovieEntry*              entry = nullptr;
    uint32_t                           obj   = 0;  // objeto do filme do open atual
    bool                               showing = false;  // o host apresenta o quadro do filme
    std::vector<nv::SubtitleCue>       cues;             // legendas do filme atual (do jogo)
    std::wstring                       subtitle;         // texto na tela agora
    std::unique_ptr<nv::ISubtitleRasterizer> raster;
    Stats                              stats;
};

State& st() {
    static State s;
    return s;
}

// Chamada do guest a partir do nativo, convencao normal (r4-r7), retorno conferido pelo runtime (como no bu_adapter).
uint32_t callGuest(Sh4Context* c, uint32_t func, uint32_t a0 = 0, uint32_t a1 = 0, uint32_t a2 = 0, uint32_t a3 = 0) {
    if (!func) return 0;
    const uint32_t savedPr = c->pr, savedRet = c->ret_expect, savedSp = c->ret_sp, savedFn = c->cur_fn;
    const uint32_t savedR4 = c->r[4], savedR5 = c->r[5], savedR6 = c->r[6], savedR7 = c->r[7];
    constexpr uint32_t kBack = 0xFFFFFFF0u;  // retorno sentinela (so comparado)
    c->pr = c->ret_expect = kBack;
    c->ret_sp            = c->r[15];
    c->r[4] = a0, c->r[5] = a1, c->r[6] = a2, c->r[7] = a3;
    sh4_dispatch(c, 0, func);
    const uint32_t r0 = c->r[0];
    c->pr = savedPr, c->ret_expect = savedRet, c->ret_sp = savedSp, c->cur_fn = savedFn;
    c->r[4] = savedR4, c->r[5] = savedR5, c->r[6] = savedR6, c->r[7] = savedR7;
    return r0;
}

// Um VBlank pelo relogio do jogo, como o laco de espera do original (ponto declarado 0x8C11FC3A): o tempo anda pelos
// polls do runtime e as interrupcoes (VBlank, render, Maple) sao atendidas aqui dentro.
void waitVblank(Sh4Context* c) {
    constexpr uint64_t kSpinCycles = 64;  // custo de uma volta do laco original (so ordem de grandeza)
    const uint64_t     v0          = holly_vblank_count();
    while (holly_vblank_count() == v0) {
        c->cycles += kSpinCycles;
        // voltas sem evento nao mudam nada: pula (no tempo logico os ciclos nao contam e next_event fica no maximo)
        if (!sh4_logical_cadence() && c->cycles < c->next_event) c->cycles = c->next_event;
        sh4_wait_point(c, kWaitPc);
        ++c->polls;
        c->pc = kWaitPc;
        sh4_service(c);
    }
    sh4_wait_end(c, kWaitPc);
    ++st().stats.vblanks;
}

// Instante logico de um VBlank (59,94 Hz: 1001/60000 s).
int64_t vblankTime100ns(uint64_t n) { return int64_t(n * 10'000'000ull * 1001 / 60000); }

// Legendas do jogo carregadas para o filme (lista em *kSubList, opcao em *kSubOn): uma cue por no com segundos != 0, com as
// linhas seguintes (segundos 0); vai ate a proxima cue ou o fim do filme. Texto em Latin-1 (acentos do alemao/frances).
std::vector<nv::SubtitleCue> readGameSubtitles(const GuestMemory& mem, int64_t movieEnd100ns) {
    std::vector<nv::SubtitleCue> cues;
    if (mem.load<uint32_t>(kSubOn).value_or(0) == 0) return cues;
    uint32_t node = mem.load<uint32_t>(kSubList).value_or(0);
    for (int guard = 0; node && guard < 4096; ++guard) {
        const auto text = mem.load<uint32_t>(node + 0x00), next = mem.load<uint32_t>(node + 0x0C),
                   secs = mem.load<uint32_t>(node + 0x10);
        if (!text || !next || !secs) break;  // fora da RAM: lista invalida, para aqui
        std::wstring line;
        if (const auto s = mem.cstring(*text, 512))
            for (char ch : *s)
                if (ch != '\r' && ch != '\n') line += wchar_t(static_cast<unsigned char>(ch));
        if (*secs != 0 || cues.empty()) {
            if (!cues.empty()) cues.back().end100ns = int64_t(*secs) * 10'000'000;
            cues.push_back({int64_t(*secs) * 10'000'000, movieEnd100ns, line});
        } else if (!line.empty()) {
            cues.back().text += (cues.back().text.empty() ? L"" : L"\n") + line;
        }
        node = *next;
    }
    std::erase_if(cues, [](const nv::SubtitleCue& c) { return c.text.empty() || c.end100ns <= c.start100ns; });
    return cues;
}

// SHA-256 do arquivo inteiro (vazio se nao abre): o MP4 do Builder e conferido contra o manifesto antes de tocar.
std::string fileSha256(const std::filesystem::path& p) {
    std::ifstream in(p, std::ios::binary);
    if (!in) return {};
    hh::Sha256        sha;
    std::vector<char> buf(1 << 20);
    while (in) {
        in.read(buf.data(), std::streamsize(buf.size()));
        sha.update(reinterpret_cast<const uint8_t*>(buf.data()), size_t(in.gcount()));
    }
    return sha.hexdigest();
}

// Legendas de um mod (catalogo): pt-br, depois pt e en, senao a primeira por nome. Vazio se o mod nao tem legendas.
std::vector<nv::SubtitleCue> modSubtitles(const nv::MovieEntry& e) {
    std::vector<nv::SubtitleCue> cues;
    if (e.subtitles.empty()) return cues;
    const std::filesystem::path* f = &e.subtitles.begin()->second;
    for (const char* lang : {"en", "pt", "pt-br"})
        if (const auto it = e.subtitles.find(lang); it != e.subtitles.end()) f = &it->second;
    std::string err;
    std::vector<std::string> warn;
    if (!nv::load_subtitles(*f, cues, &err, &warn))
        std::fprintf(stderr, "[video] legenda do mod %s: %s\n", f->string().c_str(), err.c_str());
    for (const std::string& w : warn) std::fprintf(stderr, "[video] legenda do mod %s: %s\n", f->string().c_str(), w.c_str());
    return cues;
}

std::string key_for(const std::string& name) {
    const std::string plain = "MOVIES/" + name;
    if (gdfs::current_disc() == 2) {
        const std::string d2 = "_DISCO2/" + plain;
        if (st().catalog.find(d2)) return nv::MovieCatalog::key(d2);
    }
    return nv::MovieCatalog::key(plain);
}

void fOpen(Sh4Context* c) {
    State&         s   = st();
    const uint32_t obj = c->r[4];
    GuestMemory    mem(c);
    const auto     name = mem.cstring(c->r[5], 64);
    ++s.stats.opens;
    callGuest(c, kGdFsChangeDir, kMoviesDirText);  // o original troca a pasta do gdFs para \MOVIES antes de abrir
    s.playback.reset(), s.entry = nullptr, s.obj = obj;
    const std::string k = name ? key_for(*name) : std::string("?");
    s.stats.last        = k;
    const nv::MovieEntry* e = name ? s.catalog.find(k) : nullptr;
    std::string           err;
    // o MP4 do Builder tem de ser o do manifesto (um arquivo trocado ou truncado vira ausente, com o motivo); o de um mod
    // nao tem hash (e do usuario)
    if (e && !e->absent && !e->mod && !e->sha256.empty() && fileSha256(e->file) != e->sha256)
        err = "o arquivo " + e->file.filename().string() + " nao confere com o manifesto (SHA-256)", e = nullptr;
    if (e && !e->absent) {
        auto pb = std::make_unique<nv::MoviePlayback>();
        nv::PlaybackOptions po;
        po.end                 = nv::EndRule::VideoEnd;  // o laco do jogo para no fim do stream de quadros
        po.audioSkipFrames     = e->audioDelaySamples;
        po.sourceVideoEnd100ns = e->videoEnd100ns;
        po.sourceAudioEnd100ns = e->audioEnd100ns;
        if (pb->open(e->file, po, &err)) s.playback = std::move(pb), s.entry = e;
    }
    if (!s.playback) {
        ++s.stats.absent;
        std::fprintf(stderr, "[video] filme %s ausente%s%s: o jogo segue sem ele\n", k.c_str(), err.empty() ? "" : ": ",
                     err.c_str());
        mem.store<uint32_t>(obj + 8, 0);
        GuestCall(c).ret(1);  // como o open original quando o stream nao abre o arquivo
        return;
    }
    const nv::MovieFormat& f  = s.playback->format();
    const uint32_t         sw = e->shownWidth ? e->shownWidth : f.width, sh = e->shownHeight ? e->shownHeight : f.height;
    mem.store<uint32_t>(obj + 8, 0);  // sem stream do arquivo
    mem.store<uint32_t>(obj + 0x0C, sw);
    mem.store<uint32_t>(obj + 0x10, sh);
    mem.store<uint32_t>(obj + 0x14, 640);
    mem.store<uint32_t>(obj + 0x18, 480);
    mem.store<uint32_t>(obj + 0x1C, f.hasAudio ? 1u : 0u);
    mem.store<uint32_t>(obj + 0x20, 0);
    mem.store<uint32_t>(obj + 0x24, f.hasAudio ? e->sourceBytesPerSecond : 0u);
    GuestCall(c).ret(0);
}

void fAudioTrack(Sh4Context* c) {
    GuestMemory(c).store<uint32_t>(c->r[4] + 0x20 + 16 * c->r[5] + 12, 0);  // o efeito do original no objeto
}

void fPlay(Sh4Context* c) {
    State&         s   = st();
    const uint32_t obj = c->r[4], cb = c->r[5];
    GuestMemory    mem(c);
    mem.store<uint32_t>(obj + kObjContinue, 1);
    ++s.stats.plays;
    if (!s.playback || obj != s.obj) return GuestCall(c).ret(0);  // ausente: o filme "acaba" na hora
    std::unique_ptr<na::IAudioOutput> out;
    const nv::MovieFormat&            f = s.playback->format();
    const uint32_t                    ch = std::max(1u, f.channels);
    std::deque<int16_t>               pending;  // audio decodificado que a saida ainda nao aceitou (controle de fluxo)
    auto nullOutput = [&] {
        out = na::make_null_output();
        out->open(f.sampleRate, uint8_t(ch));
    };
    if (f.hasAudio) {
        out = s.o.audio ? na::make_xaudio2_output(200) : na::make_null_output();
        if (!out->open(f.sampleRate, uint8_t(ch))) {
            std::fprintf(stderr, "[video] sem dispositivo de audio: o filme toca sem som\n");
            nullOutput();
        }
    }
    struct Showing {  // o host so apresenta o filme durante o play (inclusive se uma parada atravessar a funcao)
        State& s;
        explicit Showing(State& s) : s(s) { s.showing = true; }
        ~Showing() { s.showing = false; }
    } showing(s);
    const double   fps    = s.entry->fps > 0 ? s.entry->fps : f.fps > 0 ? f.fps : 30.0;
    const uint64_t frames = s.entry->frames;
    const uint64_t v0     = holly_vblank_count();
    auto           at     = [&](uint64_t k) { return int64_t(double(k) * 1e7 / fps + 0.5); };
    // legendas: as do mod (se o mod trouxer) ou as do proprio jogo, quando a opcao do jogo esta ligada
    s.cues = readGameSubtitles(mem, frames ? at(frames) : (f.duration100ns ? f.duration100ns : INT64_MAX));
    if (!s.cues.empty() || mem.load<uint32_t>(kSubOn).value_or(0) != 0)
        if (auto m = modSubtitles(*s.entry); !m.empty()) s.cues = std::move(m);
    s.subtitle.clear();
    s.stats.subtitleCues += s.cues.size();
    if (!s.cues.empty() && !s.raster) s.raster = nv::make_subtitle_rasterizer();
    std::fprintf(stderr, "[video] %s: inicio no VBlank %llu (%llu quadros, %zu legendas)\n", s.stats.last.c_str(),
                 (unsigned long long)v0, (unsigned long long)frames, s.cues.size());
    const char* why = "fim natural";
    std::string decodeError;
    // a ordem do laco original: quadro k na tela, callback do jogo, espera ate o instante do quadro k+1 (0x8C11FC3A),
    // fim do stream (0x8C11FED4) ou obj+0x33C zerado pelo jogo
    for (uint64_t k = 0;; ++k) {
        std::string err;
        if (s.playback->advance(at(k), &err)) ++s.stats.framesShown;
        if (out && err.empty()) {
            const auto pcm = s.playback->take_audio(at(k), &err);
            pending.insert(pending.end(), pcm.begin(), pcm.end());
            // so o que a saida aceita agora; o resto espera o proximo quadro (o XAudio2 tem fila limitada)
            const size_t n = std::min(pending.size(), out->writable_frames() * ch);
            if (n) {
                const std::vector<int16_t> blk(pending.begin(), pending.begin() + ptrdiff_t(n));
                pending.erase(pending.begin(), pending.begin() + ptrdiff_t(n));
                if (!out->write(blk) || !out->healthy()) {
                    std::fprintf(stderr, "[video] dispositivo de audio perdido: o filme segue sem som\n");
                    nullOutput(), pending.clear();
                }
            }
        }
        if (!err.empty()) {  // erro do decodificador nao e fim natural: o filme para com o motivo
            decodeError = err;
            ++s.stats.decodeErrors;
            why = "erro de decodificacao";
            break;
        }
        s.subtitle = nv::subtitle_at(s.cues, at(k));
        // efeitos por quadro do laco original no jogo: contadores de quadros desenhados e os tempos derivados
        mem.store<uint32_t>(kFrameCount, mem.load<uint32_t>(kFrameCount).value_or(0) + 1);
        mem.store<uint32_t>(kDrawnFrames, mem.load<uint32_t>(kDrawnFrames).value_or(0) + 1);
        callGuest(c, kFrameTimes);
        if (cb) callGuest(c, cb, obj);  // skip pelo controle (0x8C09E896)
        while (vblankTime100ns(holly_vblank_count() - v0) < at(k + 1)) waitVblank(c);
        const bool end = frames ? k + 1 >= frames : s.playback->finished(at(k + 1));
        if (end) {
            ++s.stats.endedNatural;
            break;
        }
        if (mem.load<uint32_t>(obj + kObjContinue).value_or(0) == 0) {
            ++s.stats.endedByGame;
            why = "parado pelo jogo (skip)";
            break;
        }
    }
    std::fprintf(stderr, "[video] %s: %s no VBlank %llu%s%s\n", s.stats.last.c_str(), why,
                 (unsigned long long)holly_vblank_count(), decodeError.empty() ? "" : ": ", decodeError.c_str());
    s.stats.framesSkipped = s.playback->frames_skipped();
    if (out) out->close();
    GuestCall(c).ret(0);
}

void fClose(Sh4Context* c) {
    State& s = st();
    GuestMemory(c).store<uint32_t>(c->r[4] + 8, 0);
    s.playback.reset(), s.entry = nullptr, s.obj = 0;
}

const std::pair<uint32_t, Sh4Func> kFns[] = {
    {kOpen, &fOpen}, {kAudioTrack, &fAudioTrack}, {kPlay, &fPlay}, {kClose, &fClose}};

} // namespace

void install(BoundaryRegistry& registry, const Options& o, std::vector<std::string>* warnings) {
    for (const auto& [addr, fn] : kFns)
        if (!sh4_lookup_generated(addr) || !mods::has_slot(addr)) {
            char hexa[16];
            std::snprintf(hexa, sizeof hexa, "%08X", addr);
            throw std::runtime_error(std::string("provedor nativo de video: a funcao do filme ") + hexa +
                                     " nao tem slot (gere o codigo de novo com [native] slots)");
        }
    State& s  = st();
    s         = State{};
    s.o       = o;
    s.catalog = nv::MovieCatalog::load({o.dataDir, o.modsDir});
    if (warnings)
        for (const std::string& p : s.catalog.problems()) warnings->push_back(p);
    for (const auto& [addr, fn] : kFns) {
        registry.add(addr, dcmap::Subsystem::Video, "4x");
        registry.set_native(addr, fn);
        if (registry.select(addr, Provider::Native) != SelectResult::Ok)
            throw std::runtime_error("provedor nativo de video: troca de provedor falhou");
    }
    s.on = true;
}

void uninstall(BoundaryRegistry& registry) {
    for (const auto& [addr, fn] : kFns) registry.select(addr, Provider::Legacy);
    st() = State{};
}

bool installed() { return st().on; }

Stats stats() { return st().stats; }

std::string summary() {
    const Stats& s = st().stats;
    char         buf[384];
    std::snprintf(buf, sizeof buf,
                  "[video] nativo: %zu filmes no catalogo, open %llu (ausentes %llu), play %llu, quadros %llu (pulados %llu), "
                  "fim natural %llu, pelo jogo %llu, VBlanks %llu, legendas %llu, ultimo %s\n",
                  st().catalog.size(), (unsigned long long)s.opens, (unsigned long long)s.absent,
                  (unsigned long long)s.plays, (unsigned long long)s.framesShown, (unsigned long long)s.framesSkipped,
                  (unsigned long long)s.endedNatural, (unsigned long long)s.endedByGame, (unsigned long long)s.vblanks,
                  (unsigned long long)s.subtitleCues, s.last.empty() ? "-" : s.last.c_str());
    return buf;
}

bool compose_frame(std::vector<uint32_t>& argb, uint32_t& width, uint32_t& height) {
    State& s = st();
    if (!s.showing || !s.playback || !s.playback->has_frame()) return false;
    const nv::VideoFrame& f  = s.playback->frame();
    const uint32_t        sw = std::min(f.width, s.entry && s.entry->shownWidth ? s.entry->shownWidth : f.width);
    const uint32_t        sh = std::min(f.height, s.entry && s.entry->shownHeight ? s.entry->shownHeight : f.height);
    if (!sw || !sh) return false;
    width = 640, height = 480;
    argb.assign(size_t(width) * height, 0xFF000000u);
    const double   scale = std::min(640.0 / sw, 480.0 / sh);
    const uint32_t dw = uint32_t(sw * scale + 0.5), dh = uint32_t(sh * scale + 0.5);
    const uint32_t x0 = (640 - dw) / 2, y0 = (480 - dh) / 2;
    for (uint32_t y = 0; y < dh; ++y) {  // amostragem do vizinho mais proximo (o PVR do jogo filtra; ver V3)
        const uint32_t sy = std::min(sh - 1, uint32_t(y / scale));
        for (uint32_t x = 0; x < dw; ++x) {
            const uint32_t sx = std::min(sw - 1, uint32_t(x / scale));
            argb[size_t(y0 + y) * width + x0 + x] = f.argb[size_t(sy) * f.width + sx] | 0xFF000000u;
        }
    }
    // legenda do jogo (opcao ligada no jogo): centralizada embaixo, sobre o quadro composto
    if (!s.subtitle.empty() && s.raster && s.raster->ok()) s.raster->draw(argb, width, height, s.subtitle);
    return true;
}

} // namespace hh::transitional::video
