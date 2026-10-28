// SPDX-License-Identifier: MIT
#pragma once
// HH14: os 64 canais de som com a semantica dos canais da AICA que o driver do jogo programa (docs/planos/HH14-PLANO.md,
// "O que o driver faz"): cada canal le amostras direto da RAM de som enquanto toca (o ADX das musicas escreve PCM num anel
// ali durante a reproducao), em PCM 16 bits, PCM 8 bits ou ADPCM de 4 bits, com laco LSA..LEA, pitch OCT/FNS, nivel
// TL + DISDL, pan DIPAN e envio ao DSP de efeitos (ISEL/IMXL). Portavel e deterministico: o tempo anda so por render(n),
// a 44 100 amostras por segundo; quem chama decide n pelo relogio logico. Reconstrucao do comportamento, nao execucao
// do ARM7 nem da AICA (regra "recompilar, nunca emular"): sem envelope, filtro nem LFO, que o driver deixa em repouso
// (modelo do canal: AR = RR = 31, D1R = D2R = 0, filtro aberto, LFO parado).
#include "audio/aica_adpcm.h"
#include "audio/aica_dsp.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <span>

namespace hh::native::audio {

struct AicaChannelParams {
    uint32_t sa   = 0;      // endereco inicial na RAM de som (bytes)
    uint8_t  pcms = 0;      // 0 PCM 16 bits, 1 PCM 8 bits, 2 ADPCM, 3 ADPCM longo (o laco nao restaura o estado)
    bool     loop = false;  // LPCTL
    // laco e fim, em amostras desde SA. LEA e exclusivo: o canal toca LSA..LEA-1 e volta ao LSA (sem laco, para no LEA).
    // Evidencias (docs/planos/HH14-PLANO.md): o CRI programa LEA = tamanho do anel (0x8000 amostras de ADPCM ou 0x2000 de
    // PCM 16 em 16 KB) e copia o inicio do anel para depois do fim (guarda da interpolacao); com LEA inclusivo o ADPCM
    // longo decodifica um nibble a mais em cada volta e o estado se perde (estereo descorrelacionado na primeira volta).
    uint32_t lsa = 0, lea = 0;
    int8_t   oct = 0;           // -8..7
    uint16_t fns = 0;           // 0..1023: passo = 2^oct * (1024 + fns) / 1024 amostras por amostra de saida
    uint8_t  tl = 0, disdl = 15, dipan = 0, isel = 0, imxl = 0;
};

// Ganhos da AICA (as curvas do N6 do BS, conferidas com o oraculo): TL 0,375 dB por passo; DISDL/IMXL/EFSDL 3 dB por
// passo abaixo de 15, 0 = mudo; DIPAN: bits 3-0 = atenuacao do lado oposto (3 dB por passo, 15 = mudo), bit 4 = lado.
float aica_tl_gain(uint8_t tl);
float aica_level_gain(uint8_t level4);
void  aica_pan_gains(uint8_t dipan, float& left, float& right);

class AicaChannels {
public:
    static constexpr int      kChannels = 64;
    static constexpr uint32_t kRate     = 44100;

    explicit AicaChannels(std::span<const uint8_t> ram);

    void key_on(int ch, const AicaChannelParams& p);  // reinicia na amostra 0
    void key_off(int ch);                             // AR/RR 31: para sem rampa
    void update(int ch, const AicaChannelParams& p);  // muda nivel, pan, pitch, laco sem reiniciar
    const AicaChannelParams& params(int ch) const { return ch_[size_t(ch)].p; }
    bool     playing(int ch) const { return ch_[size_t(ch)].on; }
    uint32_t position(int ch) const { return ch_[size_t(ch)].ca; }  // CA: amostra atual desde SA

    void set_master(uint8_t mvol, bool mono);
    // programa do DSP de efeitos (SFPB/SFOB); nullptr desliga o envio
    void set_dsp(std::shared_ptr<const AicaDspProgram> p);

    // avanca n amostras de saida; `out` (se nao nulo) recebe 2n amostras int16 intercaladas (esquerda, direita)
    void render(size_t n, int16_t* out);

    // diagnostico: energia (soma dos quadrados da saida direta, esquerda + direita) de cada canal desde o inicio
    double energy(int ch) const { return energy_[size_t(ch)]; }

private:
    struct Channel {
        AicaChannelParams p;
        bool              on = false;
        uint32_t          ca = 0;           // amostra atual
        uint32_t          frac = 0;         // fracao do passo (16 bits)
        int32_t           cur = 0, nxt = 0; // amostra atual e a seguinte (interpolacao linear)
        bool              nxtValid = false;
        AicaAdpcmState    st, loopSt;       // ADPCM: estado antes da proxima amostra a decodificar; o do LSA
        bool              loopStSaved = false;
        uint32_t          decoded = 0;      // proxima amostra que o ADPCM decodifica (sequencial)
        float             gl = 0, gr = 0, gs = 0;  // ganhos direto (esquerda, direita) e do envio ao DSP
        uint64_t          step = 65536;            // passo em 16.16 (OCT/FNS)
    };
    static void gains(Channel& c);
    int32_t  fetch(Channel& c, uint32_t i);  // amostra i (sequencial no ADPCM)
    uint32_t next_index(const Channel& c, uint32_t i, bool& valid) const;

    std::span<const uint8_t>                ram_;
    std::array<Channel, kChannels>          ch_{};
    float                                   master_ = 1.0f;
    bool                                    mono_   = false;
    std::unique_ptr<AicaDsp>                dsp_;
    std::array<std::array<float, 2>, 16>    dspOut_{};
    std::array<double, kChannels>           energy_{};
};

} // namespace hh::native::audio
