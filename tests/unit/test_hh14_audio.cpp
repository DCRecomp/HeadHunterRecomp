// SPDX-License-Identifier: MIT
// HH14: o nucleo de audio portado do BS sobre os arquivos do Head Hunter (HH14_STREAMS = a pasta STREAMS de um disco;
// sem ela, SKIP). Prova a leitura dos WAV com ADPCM de 4 bits da AICA (tag 0) mono e estereo: taxa e canais do cabecalho,
// duracao pelo tamanho dos dados, sinal plausivel (RMS, saturacao rara) e, no estereo, canais correlacionados (um
// entrelacamento errado das amostras de cada canal deixa os dois lados sem relacao).
#include "test.h"

#include "audio/sound_file.h"

#include <cmath>
#include <cstdio>
#include <algorithm>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>
#include <vector>

namespace na = hh::native::audio;

namespace {

struct WavHeader {
    uint16_t tag = 0, channels = 0;
    uint32_t rate = 0, dataBytes = 0;
};

bool read_header(const std::filesystem::path& p, WavHeader& h) {
    std::ifstream f(p, std::ios::binary);
    char          b[64]{};
    if (!f.read(b, sizeof b) || std::string(b, 4) != "RIFF") return false;
    std::memcpy(&h.tag, b + 20, 2), std::memcpy(&h.channels, b + 22, 2), std::memcpy(&h.rate, b + 24, 4);
    for (int i = 36; i + 8 <= int(sizeof b); i += 2)  // o bloco data vem depois do fmt (18 bytes nestes arquivos)
        if (std::string(b + i, 4) == "data") return std::memcpy(&h.dataBytes, b + i + 4, 4), true;
    return false;
}

} // namespace

HH_TEST(hh14_streams_wav_adpcm) {
    const char* dir = std::getenv("HH14_STREAMS");
    if (!dir) HH_SKIP("HH14_STREAMS ausente");
    int    files = 0, stereo = 0, failed = 0, implausible = 0, uncorrelated = 0;  // uncorrelated: falas *_RL
    double minCorr = 1.0;
    for (const auto& e : std::filesystem::directory_iterator(dir)) {
        WavHeader h;
        if (!read_header(e.path(), h)) continue;  // os ADX (musicas) nao sao WAV
        ++files;
        std::string err;
        auto        s = na::load_sound(e.path(), &err);
        if (!s || h.tag != 0 || s->rate != h.rate || s->channels != h.channels ||
            s->frames() != int64_t(h.dataBytes) * 2 / h.channels) {
            ++failed;
            std::printf("  falha: %s (%s)\n", e.path().filename().string().c_str(), err.c_str());
            continue;
        }
        double   sum = 0;
        uint64_t clip = 0;
        for (int16_t v : s->pcm) sum += double(v) * v, clip += v == 32767 || v == -32768;
        const double rms = std::sqrt(sum / double(std::max<size_t>(s->pcm.size(), 1)));
        if (rms < 30 || rms > 20000 || clip * 1000 > s->pcm.size()) ++implausible;
        if (s->channels == 2) {
            ++stereo;
            double sl = 0, sr = 0, slr = 0;
            for (int64_t i = 0; i < s->frames(); ++i) {
                const double l = s->pcm[size_t(2 * i)], r = s->pcm[size_t(2 * i + 1)];
                sl += l * l, sr += r * r, slr += l * r;
            }
            const double c = slr / std::sqrt(std::max(sl * sr, 1.0));
            // as falas estereo das cenas (*_RL) tem os dois lados quase iguais; efeitos e musicas, nao necessariamente
            if (e.path().stem().string().ends_with("_RL")) minCorr = std::min(minCorr, c), uncorrelated += c < 0.8;  // leiautes errados: < 0,2
        }
    }
    std::printf("  %d WAV ADPCM (%d estereo): falhas %d, sinal implausivel %d, estereo sem correlacao %d (menor %.2f)\n",
                files, stereo, failed, implausible, uncorrelated, minCorr);
    HH_CHECK(files > 0 && failed == 0 && implausible == 0);
    HH_CHECK(uncorrelated == 0);
}

// HH14 (mods e dublagem): encode_aica_wav gera o formato dos streams do jogo; ida e volta pelo leitor com o sinal
// preservado (ADPCM de 4 bits: relacao sinal/ruido acima de 15 dB) nos dois canais, sem misturar os lados.
HH_TEST(hh14_encode_aica_wav_roundtrip) {
    na::SoundData s;
    s.rate = 32000, s.channels = 2;
    for (int i = 0; i < 32000; ++i)
        s.pcm.push_back(int16_t(9000 * std::sin(i * 0.05))), s.pcm.push_back(int16_t(6000 * std::sin(i * 0.013)));
    const auto  wav = na::encode_aica_wav(s);
    std::string err;
    auto        back = na::parse_wav(wav, &err);
    HH_CHECK(back && back->channels == 2 && back->rate == 32000 && back->frames() == 32000);
    if (!back) return;
    for (int c = 0; c < 2; ++c) {
        double sig = 0, noise = 0;
        for (size_t i = 0; i < 32000; ++i) {
            const double a = s.pcm[2 * i + size_t(c)], b = back->pcm[2 * i + size_t(c)];
            sig += a * a, noise += (a - b) * (a - b);
        }
        HH_CHECK(10 * std::log10(sig / std::max(noise, 1.0)) > 15);
    }
    HH_CHECK(wav.size() == 0x30 + 32000 && std::memcmp(wav.data() + 0x28, "data", 4) == 0);
}

// Com os streams do disco: reconverter um stream real (decodificado pelo leitor) da o mesmo cabecalho, byte a byte (o
// jogo le taxa, canais e tamanho dali), mono e estereo (os 20 primeiros WAV da pasta).
HH_TEST(hh14_encode_aica_wav_same_header) {
    const char* dir = std::getenv("HH14_STREAMS");
    if (!dir) HH_SKIP("HH14_STREAMS ausente");
    int checked = 0;
    for (const auto& e : std::filesystem::directory_iterator(dir)) {
        WavHeader h;
        if (checked >= 20 || !read_header(e.path(), h)) continue;  // os WAV do disco (os ADX nao sao WAV)
        const auto        path = e.path();
        const std::string file = path.filename().string();
        const char*       name = file.c_str();
        std::ifstream             f(path, std::ios::binary);
        const std::vector<uint8_t> orig((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
        auto                       s = na::parse_wav(orig);
        HH_CHECK(s != nullptr);
        if (!s) continue;
        const auto enc  = na::encode_aica_wav(*s);
        const bool same = enc.size() == orig.size() && std::memcmp(enc.data(), orig.data(), 0x30) == 0;
        if (!same) {
            std::printf("  %s: tamanho %zu x %zu\n  original:", name, enc.size(), orig.size());
            for (int i = 0; i < 0x30; ++i) std::printf(" %02X", orig[size_t(i)]);
            std::printf("\n  gerado:  ");
            for (int i = 0; i < 0x30 && i < int(enc.size()); ++i) std::printf(" %02X", enc[size_t(i)]);
            std::printf("\n");
        }
        HH_CHECK(same);
        ++checked;
    }
    HH_CHECK(checked > 0);
}
