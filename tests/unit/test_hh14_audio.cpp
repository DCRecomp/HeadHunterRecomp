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
