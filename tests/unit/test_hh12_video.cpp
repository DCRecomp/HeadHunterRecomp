// SPDX-License-Identifier: MIT
// Derivado de recomp-BStinger tests/unit/test_video_native.cpp (MIT, Copyright (c) 2026 Blue Stinger
// Recompiled contributors; ver third_party/licenses/recomp-BStinger-LICENSE.txt).
// HH12: catalogo dos filmes pelo caminho na instalacao unica (com a variante do disco 2), cabecalho do 4X Movie,
// MP4 sintetico pelo Media Foundation e reproducao pelo relogio logico. Arquivos sinteticos (nada do jogo).
#include "test.h"

#include "hh_extract/hash.hpp"
#include "video/fourxm_header.h"
#include "video/mf_movie.h"
#include "video/movie_build.h"
#include "video/movie_catalog.h"
#include "video/movie_playback.h"

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <numbers>
#include <string>
#include <vector>

namespace fs = std::filesystem;
namespace nv = hh::native::video;
namespace bv = hh::builder::video;

namespace {

struct TempDir {
    fs::path p;
    explicit TempDir(const char* name) : p(fs::temp_directory_path() / (std::string("hh12_") + name)) {
        std::error_code ec;
        fs::remove_all(p, ec);
        fs::create_directories(p);
    }
    ~TempDir() {
        std::error_code ec;
        fs::remove_all(p, ec);
    }
};

void writeText(const fs::path& p, const std::string& s) {
    fs::create_directories(p.parent_path());
    std::ofstream(p, std::ios::binary) << s;
}

void put32(std::string& s, uint32_t v) {
    for (int i = 0; i < 4; ++i) s += char((v >> (8 * i)) & 0xFF);
}

// RIFF 4XMV minimo com um vtrk de 11 u32 (so o que o leitor usa)
std::string fake4xm(uint32_t frames, uint32_t cw, uint32_t ch, uint32_t sw, uint32_t sh, bool riff = true) {
    std::string v;
    const uint32_t f[11] = {0, 0, 0x40005, 811, frames, 0, 0, cw, ch, sw, sh};
    for (uint32_t x : f) put32(v, x);
    std::string body = std::string("LIST") + "\x00\x00\x00\x00" + "HEAD" + "vtrk";
    put32(body, uint32_t(v.size()));
    body += v;
    std::string out = riff ? "RIFF" : "JUNK";
    put32(out, uint32_t(body.size() + 4));
    return out + "4XMV" + body;
}

} // namespace

HH_TEST(hh12_catalog_keys_by_install_path_and_disc2_variant) {
    TempDir        d("catalogo");
    const fs::path data = d.p / "data" / "video", mods = d.p / "mods" / "video";
    fs::create_directories(data), fs::create_directories(mods);
    writeText(data / "SCENE0.mp4", "x"), writeText(data / "SCENE0.D2.mp4", "x"), writeText(data / "SCENE22.mp4", "x");
    writeText(data / "manifest.toml",
              "versao = 1\n"
              "[[filme]]\noriginal = \"MOVIES/SCENE0.4XM\"\narquivo = \"SCENE0.mp4\"\nquadros = 512\nfps = 30.0\nlargura = 480\n"
              "altura = 368\nlargura_exibida = 480\naltura_exibida = 360\ntaxa_audio_fonte = 32000\ncanais = 2\nsha256 = \"ab\"\n"
              "[[filme]]\noriginal = \"_DISCO2/MOVIES/SCENE0.4XM\"\narquivo = \"SCENE0.D2.mp4\"\nquadros = 513\nfps = 30.0\n"
              "[[filme]]\noriginal = \"MOVIES/SCENE22.4XM\"\narquivo = \"SCENE22.mp4\"\n"
              "[[filme]]\noriginal = \"MOVIES/SCENE99.4XM\"\nausente = true\n"
              "[[filme]]\noriginal = \"MOVIES/FORA.4XM\"\narquivo = \"../fora.mp4\"\n"
              "[[filme]]\narquivo = \"SEM_ORIGINAL.mp4\"\n");
    {
        auto        c  = nv::MovieCatalog::load({data, {}});
        const auto* s0 = c.find("\\MOVIES\\Scene0.4xm");  // separador e caixa do jogo
        HH_CHECK(s0 && !s0->mod && s0->frames == 512 && s0->shownHeight == 360 && s0->sourceBytesPerSecond == 128000);
        HH_CHECK(s0 && s0->file == data / "SCENE0.mp4");
        const auto* d2 = c.find("_disco2/movies/scene0.4xm");
        HH_CHECK(d2 && d2 != s0 && d2->frames == 513);
        HH_CHECK(c.find("MOVIES/SCENE99.4XM") && c.find("MOVIES/SCENE99.4XM")->absent);
        HH_CHECK(c.find("MOVIES/FORA.4XM") == nullptr);  // caminho fora da pasta: recusado
        HH_CHECK(c.problems().size() == 2);              // ../fora.mp4 e a entrada sem original
    }
    // mods por nome: SCENE0.mp4 vale para os dois discos; SCENE0.D2.mp4 so para o disco 2 e tem prioridade
    writeText(mods / "scene0.mp4", "y");
    writeText(mods / "SCENE0.pt-br.srt", "1\n00:00:01,000 --> 00:00:02,000\nOla\n");
    {
        auto c = nv::MovieCatalog::load({data, mods});
        HH_CHECK(c.find("MOVIES/SCENE0.4XM")->mod && c.find("MOVIES/SCENE0.4XM")->file == mods / "scene0.mp4");
        HH_CHECK(c.find("MOVIES/SCENE0.4XM")->frames == 0 && c.find("MOVIES/SCENE0.4XM")->shownHeight == 0);
        HH_CHECK(c.find("MOVIES/SCENE0.4XM")->subtitles.count("pt-br") == 1);
        HH_CHECK(c.find("_DISCO2/MOVIES/SCENE0.4XM")->mod && c.find("_DISCO2/MOVIES/SCENE0.4XM")->file == mods / "scene0.mp4");
        HH_CHECK(!c.find("MOVIES/SCENE22.4XM")->mod);
    }
    writeText(mods / "SCENE0.D2.mp4", "z");
    {
        auto c = nv::MovieCatalog::load({data, mods});
        HH_CHECK(c.find("_DISCO2/MOVIES/SCENE0.4XM")->file == mods / "SCENE0.D2.mp4");
        HH_CHECK(c.find("MOVIES/SCENE0.4XM")->file == mods / "scene0.mp4");
    }
    // manifesto de mods quebrado: problema registrado, catalogo do Builder intacto
    writeText(mods / "manifest.toml", "[[filme]\noriginal = ");
    auto c2 = nv::MovieCatalog::load({data, mods});
    HH_CHECK(c2.find("MOVIES/SCENE22.4XM") && !c2.find("MOVIES/SCENE22.4XM")->mod && !c2.problems().empty());
}

HH_TEST(hh12_movie_file_names) {
    HH_CHECK(bv::movie_file_name("MOVIES/SCENE0.4XM") == "SCENE0.mp4");
    HH_CHECK(bv::movie_file_name("_DISCO2/MOVIES/SCENE0.4XM") == "SCENE0.D2.mp4");
    HH_CHECK(bv::movie_file_name("movies/scene24a_25.4xm") == "SCENE24A_25.mp4");
}

HH_TEST(hh12_4xm_header) {
    TempDir         d("cabecalho");
    bv::FourXmHeader h;
    std::string      err;
    writeText(d.p / "a.4xm", fake4xm(512, 480, 368, 480, 360));
    HH_CHECK(bv::read_4xm_header(d.p / "a.4xm", h, &err) && h.frames == 512 && h.codedHeight == 368 && h.shownHeight == 360);
    writeText(d.p / "b.4xm", fake4xm(10, 480, 272, 0, 0));  // sem area exibida: o quadro todo
    HH_CHECK(bv::read_4xm_header(d.p / "b.4xm", h, &err) && h.shownWidth == 480 && h.shownHeight == 272);
    writeText(d.p / "c.4xm", fake4xm(10, 480, 272, 480, 300));  // exibida maior que a codificada
    HH_CHECK(!bv::read_4xm_header(d.p / "c.4xm", h, &err) && err.find("invalido") != std::string::npos);
    writeText(d.p / "d.4xm", fake4xm(10, 480, 272, 480, 270, false));
    HH_CHECK(!bv::read_4xm_header(d.p / "d.4xm", h, &err) && err.find("4XMV") != std::string::npos);
    HH_CHECK(!bv::read_4xm_header(d.p / "nao_existe.4xm", h, &err));
}

// MP4 sintetico pelo Media Foundation: 30 quadros de cor solida (480x272, 30 fps) e 1 s de tom estereo a 48 kHz (a taxa
// do Builder); lido de volta, os quadros saem em ordem, com os tempos de 1/30 s e as cores proximas (H.264 com perdas).
HH_TEST(hh12_mf_roundtrip_synthetic) {
    TempDir           d("mf");
    const fs::path    mp4 = d.p / "teste.mp4";
    nv::MfMovieWriter w;
    std::string       err;
    nv::WriterOptions o;
    o.width = 480, o.height = 272, o.sampleRate = 48000, o.quality = 100;
    if (!w.open(mp4, o, &err)) {
        std::printf("  (sem Media Foundation/H.264 nesta maquina: %s)\n", err.c_str());
        HH_CHECK(false);
        return;
    }
    auto color = [](int i) { return 0xFF000000u | uint32_t((i * 8) & 0xFF) << 16 | uint32_t((255 - i * 8) & 0xFF) << 8 | 0x40u; };
    std::vector<uint32_t> frame(480 * 272);
    for (int i = 0; i < 30; ++i) {
        std::fill(frame.begin(), frame.end(), color(i));
        HH_CHECK(w.write_frame(frame, &err));
    }
    std::vector<int16_t> pcm(48000 * 2);
    for (size_t i = 0; i < 48000; ++i) pcm[2 * i] = pcm[2 * i + 1] = int16_t(8000 * std::sin(2 * std::numbers::pi * 440 * double(i) / 48000));
    HH_CHECK(w.write_audio(pcm, &err));
    HH_CHECK(w.finish(&err));

    nv::MfMovieReader r;
    HH_CHECK(r.open(mp4, &err));
    const auto& f = r.format();
    HH_CHECK(f.width == 480 && f.height == 272 && std::abs(f.fps - 30.0) < 0.01 && f.hasAudio && f.sampleRate == 48000 && f.channels == 2);
    nv::VideoFrame vf;
    int            n = 0;
    bool           order = true, colors = true;
    while (r.next_frame(vf, &err)) {
        order &= std::llabs(vf.time100ns - int64_t(n) * 10'000'000 / 30) < 20'000;
        const uint32_t px = vf.argb[size_t(136) * 480 + 240], want = color(n);
        for (int sh : {16, 8, 0}) colors &= std::abs(int((px >> sh) & 0xFF) - int((want >> sh) & 0xFF)) <= 12;
        ++n;
    }
    HH_CHECK(n == 30 && order && colors);
}

// Reproducao pelo relogio logico: o quadro mostrado e o ultimo com tempo <= posicao; relogio parado repete; salto pula
// quadros (contados); o fim pelo video.
HH_TEST(hh12_playback_by_logical_clock) {
    TempDir           d("playback");
    const fs::path    mp4 = d.p / "p.mp4";
    nv::MfMovieWriter w;
    std::string       err;
    nv::WriterOptions o;
    o.width = 480, o.height = 208, o.sampleRate = 48000;
    HH_CHECK(w.open(mp4, o, &err));
    std::vector<uint32_t> fr(480 * 208);
    for (int i = 0; i < 30; ++i) {
        std::fill(fr.begin(), fr.end(), 0xFF000000u | uint32_t(i * 8) << 16);
        w.write_frame(fr, &err);
    }
    std::vector<int16_t> pcm(48000 * 2, 1000);
    w.write_audio(pcm, &err);
    HH_CHECK(w.finish(&err));
    auto red = [](const nv::VideoFrame& f) { return int((f.argb[size_t(104) * 480 + 240] >> 16) & 0xFF); };

    nv::MoviePlayback   p;
    nv::PlaybackOptions po;
    po.end = nv::EndRule::VideoEnd;
    HH_CHECK(p.open(mp4, po, &err));
    const int64_t ms = 10'000;
    HH_CHECK(p.advance(0) && p.has_frame() && std::abs(red(p.frame()) - 0) <= 12);
    HH_CHECK(!p.advance(10 * ms));
    HH_CHECK(p.advance(34 * ms) && std::abs(red(p.frame()) - 8) <= 12);
    HH_CHECK(!p.advance(34 * ms));
    HH_CHECK(p.advance(500 * ms) && std::abs(red(p.frame()) - 15 * 8) <= 12 && p.frames_skipped() == 13);
    HH_CHECK(!p.finished(990 * ms));
    p.advance(1'100 * ms);
    p.take_audio(1'100 * ms);
    HH_CHECK(p.finished(1'100 * ms) && std::llabs(p.video_end100ns() - 10'000'000) < 20'000);
}

// Sem FFmpeg: os filmes ficam "nao convertidos" no manifesto (o jogo os pula), sem falhar a compilacao; o catalogo os le
// como ausentes e um mod pelo nome os fornece.
HH_TEST(hh12_build_without_ffmpeg) {
    TempDir        d("sem_ffmpeg");
    const fs::path data = d.p / "data";
    const std::string src = fake4xm(1, 480, 208, 480, 204);
    writeText(data / "disc" / "MOVIES" / "SCENE0.4XM", src);
    hh::Sha256 sha;
    sha.update(reinterpret_cast<const uint8_t*>(src.data()), src.size());
    const std::string hex = sha.hexdigest();
    auto manifest = [&](const std::string& h) {
        writeText(data / "arquivos.manifest", "hh-arquivos 1\ndisco1 hedhunter_v1006_a\ndisco2 hedhunter_v1006_b\n"
                                              "file 0 1 " + h + " MOVIES/SCENE0.4XM\nfile 0 1 00 MOVIES/OUTRO.BIN\n");
    };
    bv::MovieBuildOptions o;
    o.conv.ffmpeg = d.p / "nao_existe" / "ffmpeg.exe";
    bv::MovieBuildResult r;
    std::string          err;
    // a fonte que nao confere com o manifesto dos dados e recusada (nada e convertido nem reaproveitado)
    manifest(std::string(64, '0'));
    HH_CHECK(!bv::build_movies(data, o, r, &err) && err.find("nao confere") != std::string::npos);
    manifest(hex);
    HH_CHECK(bv::build_movies(data, o, r, &err) && r.converted == 0 && !r.unavailable.empty());
    auto c = nv::MovieCatalog::load({data / "video", d.p / "mods" / "video"});
    HH_CHECK(c.size() == 1 && c.find("MOVIES/SCENE0.4XM") && c.find("MOVIES/SCENE0.4XM")->absent);
    writeText(d.p / "mods" / "video" / "SCENE0.mp4", "x");
    auto c2 = nv::MovieCatalog::load({data / "video", d.p / "mods" / "video"});
    HH_CHECK(c2.find("MOVIES/SCENE0.4XM") && !c2.find("MOVIES/SCENE0.4XM")->absent && c2.find("MOVIES/SCENE0.4XM")->mod);
}

// Dados do disco (HH_DATA_DIR = dados de execucao do disco 1) e FFmpeg (PATH ou HH_FFMPEG): o SCENE0 convertido tem os
// quadros e o tamanho exibido do vtrk, o MP4 bate com o decodificado pelo FFmpeg (perda so do H.264) e o audio do AAC
// com o PCM da fonte (a 48 kHz).
HH_TEST(hh12_convert_scene0_from_disc) {
    const char* data = std::getenv("HH_DATA_DIR");
    if (!data || !*data) HH_SKIP("HH_DATA_DIR ausente");
    const fs::path src = fs::path(data) / "disc" / "MOVIES" / "SCENE0.4XM";
    std::error_code ec;
    if (!fs::is_regular_file(src, ec)) HH_SKIP("SCENE0.4XM ausente em HH_DATA_DIR");
    TempDir            d("scene0");
    bv::ConvertOptions o;
    if (const char* f = std::getenv("HH_FFMPEG"); f && *f) o.ffmpeg = f;
    bv::ConvertResult r;
    std::string       err;
    if (!bv::convert_4xm(src, d.p / "SCENE0.mp4", o, r, &err)) {
        std::fprintf(stderr, "  conversao: %s\n", err.c_str());
        if (err.find("instale o FFmpeg") != std::string::npos) HH_SKIP("sem FFmpeg");
        HH_CHECK(false);
        return;
    }
    HH_CHECK(r.frames == 512 && r.hdr.frames == 512 && r.hdr.shownWidth == 480 && r.hdr.shownHeight == 360);
    HH_CHECK(r.src.width == 480 && r.src.height == 368 && r.src.fpsNum == 30 && r.src.fpsDen == 1);
    HH_CHECK(r.src.sampleRate == 32000 && r.src.channels == 2 && r.audioSamples > 0);
    bv::Fidelity f;
    HH_CHECK(bv::verify_4xm(src, d.p / "SCENE0.mp4", o, f, &err));
    std::printf("  SCENE0: %u quadros, PSNR med %.2f min %.2f dB (%u < 35 dB), audio atraso %d SNR %.1f dB\n", f.framesMp4,
                f.psnrAvg, f.psnrMin, f.framesBelow35, f.audioDelaySamples, f.audioSnr);
    HH_CHECK(f.framesRef == 512 && f.framesMp4 == 512);
    HH_CHECK(f.psnrAvg >= 38.0 && f.psnrMin >= 30.0);
    HH_CHECK(f.audioSnr >= 15.0);
}
