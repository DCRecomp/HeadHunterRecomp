// SPDX-License-Identifier: MIT
// Derivado de recomp-BStinger native/audio/sound_file.cpp (MIT, Copyright (c) 2026 Blue Stinger
// Recompiled contributors; ver third_party/licenses/recomp-BStinger-LICENSE.txt).
#include "audio/sound_file.h"

#include "audio/aica_adpcm.h"

#include <cctype>
#include <cstring>
#include <fstream>
#include <iterator>
#include <vector>

namespace fs = std::filesystem;

namespace hh::native::audio {

namespace {
uint32_t u32(std::span<const uint8_t> b, size_t o) { return uint32_t(b[o]) | b[o + 1] << 8 | b[o + 2] << 16 | uint32_t(b[o + 3]) << 24; }
uint16_t u16(std::span<const uint8_t> b, size_t o) { return uint16_t(b[o] | b[o + 1] << 8); }
std::shared_ptr<SoundData> fail(std::string* err, const std::string& m) {
    if (err) *err = m;
    return nullptr;
}
} // namespace

std::shared_ptr<SoundData> parse_wav(std::span<const uint8_t> b, std::string* err) {
    if (b.size() < 12 || std::memcmp(b.data(), "RIFF", 4) != 0 || std::memcmp(b.data() + 8, "WAVE", 4) != 0)
        return fail(err, "nao e um WAV (RIFF/WAVE)");
    uint16_t tag = 0xFFFF, channels = 0, bits = 0;
    uint32_t rate = 0;
    std::span<const uint8_t> data;
    bool                     hasData = false;
    int64_t loopStart = -1, loopEnd = -1;
    for (size_t o = 12; o + 8 <= b.size();) {
        const uint32_t size = u32(b, o + 4);
        if (o + 8 + size > b.size()) return fail(err, "WAV truncado");
        const auto body = b.subspan(o + 8, size);
        if (std::memcmp(b.data() + o, "fmt ", 4) == 0 && size >= 16) {
            tag = u16(body, 0), channels = u16(body, 2), rate = u32(body, 4), bits = u16(body, 14);
        } else if (std::memcmp(b.data() + o, "data", 4) == 0) {
            data = body, hasData = true;
        } else if (std::memcmp(b.data() + o, "smpl", 4) == 0 && size >= 36 + 24 && u32(body, 28) > 0) {
            loopStart = u32(body, 36 + 8), loopEnd = int64_t(u32(body, 36 + 12)) + 1;  // smpl: fim inclusivo
        }
        o += 8 + size + (size & 1);
        // HH14: os WAV de ADPCM do Head Hunter declaram o fmt com 18 bytes e o gravam com 20 (2 bytes zero antes do
        // data): pula o preenchimento quando o bloco seguinte so aparece 2 bytes adiante
        if (size == 18 && std::memcmp(body.data() - 8, "fmt ", 4) == 0 && o + 10 <= b.size() && b[o] == 0 && b[o + 1] == 0 &&
            std::memcmp(b.data() + o + 2, "data", 4) == 0)
            o += 2;
    }
    if (tag == 0xFFFF || !channels || !rate) return fail(err, "WAV sem chunk fmt valido");
    if (channels > 8) return fail(err, "WAV com " + std::to_string(channels) + " canais: sem suporte");
    if (!hasData) return fail(err, "WAV sem chunk data (truncado?)");
    auto s       = std::make_shared<SoundData>();
    s->rate      = rate;
    s->channels  = uint8_t(channels);
    s->loopStart = loopStart, s->loopEnd = loopEnd;
    if (tag == 1 && bits == 16) {
        s->pcm.resize(data.size() / 2);
        std::memcpy(s->pcm.data(), data.data(), s->pcm.size() * 2);
    } else if (tag == 1 && bits == 8) {
        s->pcm.reserve(data.size());
        for (uint8_t v : data) s->pcm.push_back(int16_t((int(v) - 128) << 8));
    } else if (tag == 0 && bits == 4 && channels == 1) {  // ADPCM da AICA (os WAV de eventos do jogo)
        AicaAdpcmState st;
        aica_adpcm_decode(data, s->pcm, st);
    } else if (tag == 0 && bits == 4 && channels == 2) {
        // HH14: estereo do Head Hunter com bytes alternados (pares = esquerda, impares = direita; dois nibbles do mesmo
        // canal por byte), cada canal com o proprio estado. Evidencia: correlacao esquerda/direita de 0,96 a 0,999 nas
        // falas estereo das cenas, contra < 0,2 nos outros leiautes (docs/planos/HH14-PLANO.md)
        std::vector<uint8_t> half[2];
        for (size_t i = 0; i < data.size(); ++i) half[i & 1].push_back(data[i]);
        std::vector<int16_t> ch[2];
        for (int c = 0; c < 2; ++c) {
            AicaAdpcmState st;
            aica_adpcm_decode(half[c], ch[c], st);
        }
        const size_t n = std::min(ch[0].size(), ch[1].size());
        s->pcm.resize(2 * n);
        for (size_t i = 0; i < n; ++i) s->pcm[2 * i] = ch[0][i], s->pcm[2 * i + 1] = ch[1][i];
    } else {
        return fail(err, "formato de WAV sem suporte (tag " + std::to_string(tag) + ", " + std::to_string(bits) + " bits)");
    }
    s->pcm.resize(s->pcm.size() - s->pcm.size() % channels);  // quadro incompleto no fim: descartado
    return s;
}

std::vector<uint8_t> encode_aica_wav(const SoundData& s) {
    if (s.channels != 1 && s.channels != 2) return {};  // os streams do jogo sao mono ou estereo
    const uint32_t       ch = s.channels == 2 ? 2 : 1;
    std::vector<uint8_t> data;
    if (ch == 1) {
        AicaAdpcmState st;
        data = aica_adpcm_encode(s.pcm, st);
    } else {
        std::vector<int16_t> side[2];
        for (size_t i = 0; i + 1 < s.pcm.size(); i += 2) side[0].push_back(s.pcm[i]), side[1].push_back(s.pcm[i + 1]);
        AicaAdpcmState st[2];
        const auto     l = aica_adpcm_encode(side[0], st[0]), r = aica_adpcm_encode(side[1], st[1]);
        for (size_t i = 0; i < l.size(); ++i) data.push_back(l[i]), data.push_back(r[i]);  // bytes alternados
    }
    std::vector<uint8_t> out(0x30);
    auto put16 = [&](size_t o, uint32_t v) { out[o] = uint8_t(v), out[o + 1] = uint8_t(v >> 8); };
    auto put32 = [&](size_t o, uint32_t v) { put16(o, v & 0xFFFF), put16(o + 2, v >> 16); };
    std::memcpy(&out[0], "RIFF", 4), std::memcpy(&out[8], "WAVEfmt ", 8), std::memcpy(&out[0x28], "data", 4);
    put32(4, uint32_t(0x30 - 4 + data.size()));  // como a ferramenta original: tamanho do arquivo - 4 (o padrao seria - 8)
    put32(0x10, 18);                     // fmt declarado com 18 bytes (o original: + 2 bytes zero antes do data)
    put16(0x14, 0), put16(0x16, ch);     // tag 0 = ADPCM da AICA
    put32(0x18, s.rate), put32(0x1C, s.rate * ch);
    put16(0x20, 2 * ch), put16(0x22, 4);
    put32(0x2C, uint32_t(data.size()));
    out.insert(out.end(), data.begin(), data.end());
    return out;
}

std::shared_ptr<SoundData> load_sound(const fs::path& file, std::string* err) {
    std::ifstream in(file, std::ios::binary);
    if (!in) return fail(err, "nao abre " + file.string());
    in.seekg(0, std::ios::end);
    if (const auto n = in.tellg(); n < 0 || uint64_t(n) > kMaxSoundFileBytes) return fail(err, file.filename().string() + ": grande demais");
    in.seekg(0);
    const std::vector<uint8_t> bytes((std::istreambuf_iterator<char>(in)), {});
    std::string ext = file.extension().string();
    for (char& c : ext) c = char(std::tolower(static_cast<unsigned char>(c)));
    std::string why;
    std::shared_ptr<SoundData> s;
    if (ext == ".flac") s = parse_flac(bytes, &why);
    else if (ext == ".ogg") s = parse_ogg(bytes, &why);
    else s = parse_wav(bytes, &why);
    if (!s) return fail(err, file.filename().string() + ": " + why);
    return s;
}

} // namespace hh::native::audio
