// SPDX-License-Identifier: MIT
#pragma once
// HH15.7d3: a temporizacao do video (SPG) como modulo nativo compartilhado, com as contas do modelo anterior
// (pvr/pvr_regs.cpp ate o HH15.7c), sem mudanca: o campo dura `field_ticks` do relogio, dividido igualmente entre as
// linhas (SPG_LOAD); a fase do feixe no instante 0 vem do SPG_STATUS deixado pelo boot; os VBlank-In/Out acontecem nas
// linhas do SPG_VBLANK_INT; o vsync do SPG_STATUS vai de SPG_VBLANK inicio a fim; o campo do SPG_STATUS alterna com o
// entrelacado (SPG_CONTROL bit 4). Nenhum relogio, ordem ou prazo muda.
// Deslocamentos no banco grafico: SPG_VBLANK_INT 0x0CC (25-16 linha do VBlank-Out, 9-0 do VBlank-In), SPG_CONTROL 0x0D0
// (bit 4 entrelacado), SPG_LOAD 0x0D8 (25-16 linhas por campo - 1), SPG_VBLANK 0x0DC (25-16 fim, 9-0 inicio do vblank),
// SPG_STATUS 0x10C (9-0 linha, 10 campo, 13 vsync; so leitura). O modulo guarda 0x0C8-0x0E0.
#include <array>
#include <cstdint>

namespace hh::native::render::kamui {

struct SpgPosition {
    uint32_t line  = 0;  // 0 = inicio do vsync
    bool     vsync = false;
    bool     blank = false;
};

class SpgTiming {
public:
    static constexpr uint32_t kFirst = 0x0C8, kLast = 0x0E0;  // registradores guardados
    static constexpr uint32_t kVblankInt = 0x0CC, kControl = 0x0D0, kLoad = 0x0D8, kVblank = 0x0DC, kStatus = 0x10C;
    static constexpr uint32_t kDefaultLines = 263;  // NTSC nao entrelacado (SPG_LOAD ainda nao programado)
    static constexpr uint64_t kNever        = ~0ull;

    explicit SpgTiming(uint64_t field_ticks) : field_(field_ticks) {}

    static bool holds(uint32_t off) { return off >= kFirst && off <= kLast && off % 4 == 0; }

    void     reset();
    void     set(uint32_t off, uint32_t v) { regs_[(off - kFirst) / 4] = v; }  // off em [kFirst, kLast]
    uint32_t reg(uint32_t off) const { return regs_[(off - kFirst) / 4]; }
    // o estado deixado pelo boot: os registradores 0x0C8-0x0E0 do banco (`bank[off / 4]`) e a fase do feixe pelo
    // SPG_STATUS dele
    void load(const uint32_t* bank);

    SpgPosition position(uint64_t t) const;
    uint32_t    status(uint64_t t) const;  // SPG_STATUS no instante `t`
    // algum cruzamento da linha `line` em (from, to]?
    bool     crossed(uint64_t from, uint64_t to, uint32_t line) const;
    // primeiro cruzamento da linha `line` depois de `after` (a mesma conta de crossed); kNever se o campo nao chega a ela
    uint64_t next_crossing(uint64_t after, uint32_t line) const;
    // primeiro instante depois de `after` em que a linha do feixe muda (o SPG_STATUS so muda nesses instantes)
    uint64_t next_line_change(uint64_t after) const;
    uint32_t vblank_in_line() const { return reg(kVblankInt) & 0x3FF; }
    uint32_t vblank_out_line() const { return (reg(kVblankInt) >> 16) & 0x3FF; }
    uint64_t phase() const { return phase_; }

private:
    uint32_t field_lines() const;

    std::array<uint32_t, (kLast - kFirst) / 4 + 1> regs_{};
    uint64_t                                      field_;
    uint64_t                                      phase_ = 0;  // fase do feixe no instante 0 (dentro de dois campos)
};

} // namespace hh::native::render::kamui
