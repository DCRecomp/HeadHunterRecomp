// SPDX-License-Identifier: MIT
// HH14: os canais da AICA nativos (native/audio/aica_channels) com RAM de som sintetica.
#include "test.h"

#include "audio/aica_adpcm.h"
#include "audio/aica_channels.h"

#include <cmath>
#include <cstring>
#include <vector>

namespace na = hh::native::audio;

namespace {

std::vector<int16_t> render(na::AicaChannels& a, size_t n) {
    std::vector<int16_t> out(2 * n);
    a.render(n, out.data());
    return out;
}

} // namespace

// PCM 16 bits no centro, nivel maximo: a saida (cada lado) e a amostra; o CA anda uma amostra por saida com pitch 1:1
// e duas com OCT = 1
HH_TEST(hh14_channels_pcm16_pitch) {
    std::vector<uint8_t> ram(2u << 20);
    for (int i = 0; i < 100; ++i) {
        const int16_t v = int16_t(i * 100);
        std::memcpy(&ram[0x1000 + 2 * i], &v, 2);
    }
    na::AicaChannels  a(ram);
    na::AicaChannelParams p;
    p.sa = 0x1000, p.pcms = 0, p.lea = 100;
    a.key_on(0, p);
    auto o = render(a, 10);
    for (int i = 0; i < 10; ++i) HH_CHECK(o[2 * i] == i * 100 && o[2 * i + 1] == i * 100);
    HH_CHECK(a.position(0) == 10);
    p.oct = 1;
    a.key_on(0, p);
    o = render(a, 10);
    HH_CHECK(o[2] == 200 && a.position(0) == 20);
    p.oct = -1;  // meia velocidade: interpolacao linear entre as amostras
    a.key_on(0, p);
    o = render(a, 4);
    HH_CHECK(o[0] == 0 && o[2] == 50 && o[4] == 100 && a.position(0) == 2);
}

// LEA exclusivo: sem laco o canal toca ate LEA-1 e para; com laco volta ao LSA depois de LEA-1
HH_TEST(hh14_channels_end_and_loop) {
    std::vector<uint8_t> ram(2u << 20);
    for (int i = 0; i < 8; ++i) ram[0x2000 + i] = uint8_t(int8_t(i + 1));
    na::AicaChannels  a(ram);
    na::AicaChannelParams p;
    p.sa = 0x2000, p.pcms = 1, p.lea = 8;
    a.key_on(3, p);
    auto o = render(a, 12);
    HH_CHECK(o[2 * 7] == 8 * 256 && !a.playing(3) && o[2 * 9] == 0);
    p.loop = true, p.lsa = 4;
    a.key_on(3, p);
    o = render(a, 12);
    // 1..8, depois 5..8 de novo (LSA = 4)
    HH_CHECK(o[2 * 8] == 5 * 256 && o[2 * 11] == 8 * 256 && a.playing(3) && a.position(3) == 4 + (12 - 8) % 4);
}

// ADPCM: igual ao decodificador de buffer inteiro; no laco o formato 2 restaura o estado do LSA e o 3 continua
HH_TEST(hh14_channels_adpcm_loop_state) {
    std::vector<int16_t> pcm(64);
    for (int i = 0; i < 64; ++i) pcm[size_t(i)] = int16_t(8000 * std::sin(i * 0.3));
    na::AicaAdpcmState   enc;
    const auto           adpcm = na::aica_adpcm_encode(pcm, enc);
    std::vector<uint8_t> ram(2u << 20);
    std::memcpy(&ram[0x4000], adpcm.data(), adpcm.size());
    std::vector<int16_t> ref;
    na::AicaAdpcmState   st;
    na::aica_adpcm_decode(adpcm, ref, st);
    na::AicaChannels  a(ram);
    na::AicaChannelParams p;
    p.sa = 0x4000, p.pcms = 2, p.loop = true, p.lsa = 16, p.lea = 64;
    a.key_on(0, p);
    auto o = render(a, 64 + 48);
    bool same = true;
    for (int i = 0; i < 64; ++i) same &= o[2 * size_t(i)] == ref[size_t(i)];
    HH_CHECK(same);
    // segunda volta (formato 2): as amostras 16..63 iguais as da primeira passagem
    bool again = true;
    for (int i = 0; i < 48; ++i) again &= o[2 * size_t(64 + i)] == ref[size_t(16 + i)];
    HH_CHECK(again);
    p.pcms = 3;  // ADPCM longo: o estado nao volta, a segunda volta difere (o estado do fim nao e o do LSA)
    a.key_on(0, p);
    o          = render(a, 64 + 48);
    bool diff  = false;
    for (int i = 0; i < 48; ++i) diff |= o[2 * size_t(64 + i)] != ref[size_t(16 + i)];
    HH_CHECK(diff);
}

// curvas: TL 0,375 dB/passo, DISDL 3 dB/passo (0 = mudo), DIPAN 3 dB/passo no lado oposto (bit 4 = esquerda)
// ADPCM longo (formato 3) num anel com LEA = tamanho: a volta continua o estado sem decodificar a guarda (o nibble depois
// do LEA). Um anel de 32 amostras tocado 3 voltas e igual a decodificar as 96 amostras em sequencia (o codificador do CRI
// mantem o estado entre as voltas). Falha com LEA inclusivo (a guarda entra na decodificacao).
HH_TEST(hh14_channels_adpcm_long_ring) {
    std::vector<int16_t> pcm(96);
    for (int i = 0; i < 96; ++i) pcm[size_t(i)] = int16_t(9000 * std::sin(i * 0.21));
    na::AicaAdpcmState   enc;
    const auto           adpcm = na::aica_adpcm_encode(pcm, enc);  // 48 bytes
    std::vector<int16_t> ref;
    na::AicaAdpcmState   st;
    na::aica_adpcm_decode(adpcm, ref, st);
    std::vector<uint8_t> ram(2u << 20);
    na::AicaChannels  a(ram);
    na::AicaChannelParams p;
    p.sa = 0x8000, p.pcms = 3, p.loop = true, p.lsa = 0, p.lea = 32;
    // o anel recebe cada volta antes de ela tocar (como o CRI escreve atras da leitura)
    std::memcpy(&ram[0x8000], adpcm.data(), 16);
    ram[0x8000 + 16] = adpcm[0];  // guarda: copia do inicio, nunca decodificada
    a.key_on(0, p);
    std::vector<int16_t> out;
    // o canal antecipa uma amostra (interpolacao): a amostra 0 da volta seguinte e decodificada quando o CA chega a 31,
    // entao a volta seguinte entra no anel depois de 30 amostras (as 32 desta volta ja decodificadas)
    for (int lap = 0; lap < 3; ++lap) {
        auto o = render(a, 30);
        for (int i = 0; i < 30; ++i) out.push_back(o[2 * size_t(i)]);
        if (lap < 2) std::memcpy(&ram[0x8000], adpcm.data() + 16 * (lap + 1), 16);
        o = render(a, 2);
        out.push_back(o[0]), out.push_back(o[2]);
    }
    bool same = true;
    for (int i = 0; i < 96; ++i) same &= out[size_t(i)] == ref[size_t(i)];
    HH_CHECK(same);
}

HH_TEST(hh14_channels_gains) {
    HH_CHECK(std::abs(na::aica_tl_gain(16) - std::pow(10.0f, -6.0f / 20.0f)) < 1e-4f);
    HH_CHECK(na::aica_level_gain(0) == 0.0f && na::aica_level_gain(15) == 1.0f);
    HH_CHECK(std::abs(na::aica_level_gain(13) - std::pow(10.0f, -6.0f / 20.0f)) < 1e-4f);
    float l, r;
    na::aica_pan_gains(0x00, l, r);
    HH_CHECK(l == 1.0f && r == 1.0f);
    na::aica_pan_gains(0x0F, l, r);  // direita: o esquerdo mudo
    HH_CHECK(l == 0.0f && r == 1.0f);
    na::aica_pan_gains(0x1F, l, r);  // esquerda
    HH_CHECK(l == 1.0f && r == 0.0f);
    std::vector<uint8_t> ram(2u << 20);
    const int16_t        v = 10000;
    for (int i = 0; i < 8; ++i) std::memcpy(&ram[2 * size_t(i)], &v, 2);
    na::AicaChannels  a(ram);
    na::AicaChannelParams p;
    p.lea = 8, p.disdl = 0;
    a.key_on(1, p);
    auto o = render(a, 2);
    HH_CHECK(o[0] == 0 && o[1] == 0);  // DISDL 0: mudo
    p.disdl = 15, p.dipan = 0x0F;
    a.key_on(1, p);
    o = render(a, 2);
    HH_CHECK(o[0] == 0 && o[1] == 10000);
}
