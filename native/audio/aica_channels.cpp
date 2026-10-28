// SPDX-License-Identifier: MIT
#include "audio/aica_channels.h"

#include <algorithm>
#include <cmath>

namespace hh::native::audio {

float aica_tl_gain(uint8_t tl) { return float(std::pow(10.0, -0.375 * tl / 20.0)); }

float aica_level_gain(uint8_t level4) {
    level4 &= 15;
    return level4 == 0 ? 0.0f : float(std::pow(10.0, -3.0 * (15 - level4) / 20.0));
}

void aica_pan_gains(uint8_t dipan, float& left, float& right) {
    const int   level = dipan & 15;
    const float other = level == 15 ? 0.0f : float(std::pow(10.0, -3.0 * level / 20.0));
    // bit 4 = som a esquerda (o lado direito atenuado); 0x00 e 0x10 = centro
    left = 1.0f, right = 1.0f;
    (dipan & 0x10) ? right = other : left = other;
}

AicaChannels::AicaChannels(std::span<const uint8_t> ram) : ram_(ram) {}

void AicaChannels::key_on(int ch, const AicaChannelParams& p) {
    Channel& c = ch_[size_t(ch)];
    c          = Channel{};
    c.p        = p;
    c.on       = true;
    gains(c);
    c.cur      = fetch(c, 0);
    c.nxt      = fetch(c, next_index(c, 0, c.nxtValid));
}

void AicaChannels::key_off(int ch) { ch_[size_t(ch)].on = false; }

void AicaChannels::update(int ch, const AicaChannelParams& p) {
    Channel& c = ch_[size_t(ch)];
    const bool restart = c.p.sa != p.sa || c.p.pcms != p.pcms;
    c.p = p;
    gains(c);
    if (restart && c.on) key_on(ch, p);
}

void AicaChannels::set_master(uint8_t mvol, bool mono) {
    master_ = aica_level_gain(mvol);
    mono_   = mono;
}

void AicaChannels::set_dsp(std::shared_ptr<const AicaDspProgram> p) {
    dsp_.reset();
    if (!p) return;
    dsp_ = std::make_unique<AicaDsp>(*p);
    for (size_t o = 0; o < 16; ++o) {
        float l, r;
        aica_pan_gains(p->efpan[o], l, r);
        const float g = aica_level_gain(p->efsdl[o]);
        dspOut_[o] = {g * l, g * r};
    }
}

void AicaChannels::gains(Channel& c) {
    const float tl = aica_tl_gain(c.p.tl), dir = tl * aica_level_gain(c.p.disdl);
    float       pl, pr;
    aica_pan_gains(c.p.dipan, pl, pr);
    c.gl = dir * pl, c.gr = dir * pr;
    c.gs = c.p.imxl ? tl * aica_level_gain(c.p.imxl) : 0.0f;
    const uint64_t base = uint64_t(1024 + (c.p.fns & 1023)) << 6;  // (1024 + fns) / 1024 em 16.16
    c.step = c.p.oct >= 0 ? base << c.p.oct : base >> -c.p.oct;
}

uint32_t AicaChannels::next_index(const Channel& c, uint32_t i, bool& valid) const {
    valid = true;
    if (i + 1 < c.p.lea) return i + 1;  // LEA exclusivo
    if (c.p.loop) return c.p.lsa;
    valid = false;  // sem laco: o canal acaba depois de LEA
    return i;
}

int32_t AicaChannels::fetch(Channel& c, uint32_t i) {
    const uint32_t sa = c.p.sa & 0x1FFFFF;
    auto           byte = [&](uint32_t a) -> uint8_t { return ram_[(sa + a) & 0x1FFFFF]; };
    switch (c.p.pcms) {
    case 0: return int16_t(uint16_t(byte(2 * i) | byte(2 * i + 1) << 8));
    case 1: return int32_t(int8_t(byte(i))) * 256;
    default: {
        // ADPCM sequencial: a amostra i depende do estado depois da i - 1. No retorno ao LSA, o formato 2 restaura o
        // estado guardado na primeira passagem pelo LSA; o formato 3 (ADPCM longo) continua com o estado corrente.
        if (i != c.decoded) {
            if (i == c.p.lsa && c.p.pcms == 2 && c.loopStSaved) c.st = c.loopSt;
            c.decoded = i;
        }
        if (i == c.p.lsa && !c.loopStSaved) c.loopSt = c.st, c.loopStSaved = true;
        const uint8_t b = byte(i / 2);
        c.decoded       = i + 1;
        return aica_adpcm_step((i & 1) ? uint8_t(b >> 4) : uint8_t(b & 15), c.st);
    }
    }
}

void AicaChannels::render(size_t n, int16_t* out) {
    std::array<int32_t, 16> mixs{};
    std::array<int16_t, 16> ef{};
    std::array<float, 16>   send{};
    for (size_t f = 0; f < n; ++f) {
        float l = 0, r = 0;
        send.fill(0.0f);
        for (Channel& c : ch_) {
            if (!c.on) continue;
            const float s    = float(c.cur) + float(c.nxt - c.cur) * float(c.frac) / 65536.0f;
            l += s * c.gl, r += s * c.gr;
            energy_[size_t(&c - ch_.data())] += double(s * c.gl) * (s * c.gl) + double(s * c.gr) * (s * c.gr);
            if (dsp_ && c.gs != 0.0f) send[c.p.isel & 15] += s * c.gs;
            uint64_t acc = uint64_t(c.frac) + c.step;
            while (acc >= 65536 && c.on) {
                acc -= 65536;
                bool valid;
                const uint32_t ni = next_index(c, c.ca, valid);
                if (!valid) {  // fim sem laco: a AICA para no LEA
                    c.on = false;
                    break;
                }
                c.ca  = ni;
                c.cur = c.nxt;
                bool nv;
                const uint32_t nn = next_index(c, c.ca, nv);
                c.nxt = nv ? fetch(c, nn) : c.cur;
            }
            c.frac = uint32_t(acc & 0xFFFF);
        }
        if (dsp_) {
            for (size_t b = 0; b < 16; ++b) {
                const long long w = std::llround(double(send[b]) * 16.0);
                mixs[b]           = int32_t(uint32_t(w) << 12) >> 12;  // barramento de 20 bits
            }
            dsp_->sample(mixs, ef);
            for (size_t o = 0; o < 16; ++o) l += float(ef[o]) * dspOut_[o][0], r += float(ef[o]) * dspOut_[o][1];
        }
        l *= master_, r *= master_;
        if (mono_) l = r = 0.5f * (l + r);
        if (out) {
            out[2 * f]     = int16_t(std::clamp(std::lround(l), -32768L, 32767L));
            out[2 * f + 1] = int16_t(std::clamp(std::lround(r), -32768L, 32767L));
        }
    }
}

} // namespace hh::native::audio
