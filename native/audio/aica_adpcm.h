// SPDX-License-Identifier: MIT
// Derivado de recomp-BStinger native/audio/aica_adpcm.h (MIT, Copyright (c) 2026 Blue Stinger
// Recompiled contributors; ver third_party/licenses/recomp-BStinger-LICENSE.txt).
#pragma once
// N6: ADPCM de 4 bits da AICA (Yamaha), o formato dos 233 WAV de eventos do Blue Stinger (RIFF com
// format tag 0, mono, 22 050 Hz). Portavel. Nibble baixo primeiro; preditor e passo comecam em 0 e 127.
// O codificador existe para os testes (ida e volta) e para quem quiser gravar no formato original.
#include <cstdint>
#include <span>
#include <vector>

namespace hh::native::audio {

struct AicaAdpcmState {
    int32_t signal = 0;
    int32_t step   = 127;
};

// Decodifica `nibbles` amostras (2 por byte) e acrescenta em `out`.
void aica_adpcm_decode(std::span<const uint8_t> data, std::vector<int16_t>& out, AicaAdpcmState& st,
                       size_t samples = SIZE_MAX);
std::vector<uint8_t> aica_adpcm_encode(std::span<const int16_t> pcm, AicaAdpcmState& st);

} // namespace hh::native::audio
