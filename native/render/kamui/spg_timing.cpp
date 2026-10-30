// SPDX-License-Identifier: MIT
// HH15.7d3: as contas sao as do modelo anterior (pvr/pvr_regs.cpp), movidas sem mudanca.
#include "render/kamui/spg_timing.h"

#include <algorithm>

namespace hh::native::render::kamui {

void SpgTiming::reset() {
    regs_.fill(0);
    phase_ = 0;
}

uint32_t SpgTiming::field_lines() const {
    const uint32_t lines = ((reg(kLoad) >> 16) & 0x3FF) + 1;
    return lines < 2 ? kDefaultLines : lines;
}

void SpgTiming::load(const uint32_t* bank) {
    for (uint32_t off = kFirst; off <= kLast; off += 4) set(off, bank[off / 4]);
    const uint32_t st    = bank[kStatus / 4];
    const uint32_t lines = field_lines();
    const uint32_t line  = std::min<uint32_t>(st & 0x3FF, lines - 1);
    phase_               = ((st >> 10) & 1) * field_ + (uint64_t(line) * field_ + lines - 1) / lines;
}

bool SpgTiming::crossed(uint64_t from, uint64_t to, uint32_t line) const {
    if (to <= from) return false;
    from += phase_;
    to += phase_;
    const uint32_t lines = field_lines();
    if (line >= lines) return false;  // linha que o campo nunca alcanca
    const uint64_t off = uint64_t(line) * field_ / lines;
    // eventos em off + k*campo; existe algum em (from, to]?
    auto count = [&](uint64_t t) { return t < off ? 0ull : (t - off) / field_ + 1; };
    return count(to) > count(from);
}

uint64_t SpgTiming::next_crossing(uint64_t after, uint32_t line) const {
    const uint32_t lines = field_lines();
    if (line >= lines) return kNever;
    const uint64_t off = uint64_t(line) * field_ / lines;
    // mesma conta de crossed: eventos em t com t + fase = off + k*campo, k >= 0
    const uint64_t x = after + phase_;
    if (x < off) return off - phase_;
    const uint64_t k = (x - off) / field_ + 1;
    return off + k * field_ - phase_;
}

uint64_t SpgTiming::next_line_change(uint64_t after) const {
    // a mesma conta de position: a linha muda na primeira posicao do campo com (posicao * linhas / campo) >= linha + 1,
    // ou seja, em teto((linha + 1) * campo / linhas). (O inicio de linha de next_crossing e truncado: no limite, a linha
    // ainda nao mudou.)
    const uint32_t lines = field_lines();
    const uint64_t x     = after + phase_;
    const uint64_t base  = x - x % field_;
    const uint32_t line  = uint32_t((x % field_) * lines / field_);
    const uint64_t next  = line + 1 >= lines ? field_ : (uint64_t(line + 1) * field_ + lines - 1) / lines;
    return base + next - phase_;
}

SpgPosition SpgTiming::position(uint64_t t) const {
    t += phase_;
    const uint32_t lines   = field_lines();
    const uint32_t vbstart = reg(kVblank) & 0x3FF;
    const uint32_t vbend   = (reg(kVblank) >> 16) & 0x3FF;
    SpgPosition    p;
    p.line  = uint32_t((t % field_) * lines / field_);
    p.vsync = vbstart > vbend ? (p.line >= vbstart || p.line < vbend) : (p.line >= vbstart && p.line < vbend);
    p.blank = p.vsync;
    return p;
}

uint32_t SpgTiming::status(uint64_t t) const {
    const SpgPosition s = position(t);
    // bit 10: campo (alterna a cada campo com SPG_CONTROL.interlace, bit 4); bit 11 nunca aceso no Flycast
    const bool interlace = reg(kControl) & 0x10;
    const bool field     = interlace && (((t + phase_) / field_) & 1);
    return (s.line & 0x3FF) | (field ? 1u << 10 : 0) | (s.vsync ? 1u << 13 : 0);
}

} // namespace hh::native::render::kamui
