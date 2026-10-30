// SPDX-License-Identifier: MIT
// HH15.7d3: a temporizacao do video (SPG) como servico nativo (native/render/kamui/spg_timing, system/spg_bus). As
// contas sao conferidas com uma copia independente das formulas do modelo anterior (pvr/pvr_regs.cpp ate o HH15.7d2)
// numa grade de configuracoes e instantes, com valores fixos num campo de conta exata, e o contrato pela API: o modelo
// dos registradores do PVR delega ao servico as leituras, as escritas, o banco impresso e o estado do boot, e so
// SPG_LOAD e SPG_VBLANK_INT avisam o provedor de tempo.
#include "test.h"

#include "cpu/sh4_runtime.h"
#include "cpu/time_hooks.h"
#include "pvr/holly_intc.h"
#include "pvr/pvr_regs.h"
#include "render/kamui/spg_timing.h"
#include "system/spg_bus.h"

#include <algorithm>
#include <vector>

using hh::native::render::kamui::SpgTiming;

namespace {

// copia das formulas do modelo anterior (pvr/pvr_regs.cpp no commit 4793d64), sobre um banco de 2048 palavras
struct Anterior {
    const uint32_t* bank;
    uint64_t        field, phase = 0;
    uint32_t        reg(uint32_t off) const { return bank[off / 4]; }
    uint32_t        lines() const {
        const uint32_t l = ((reg(0x0D8) >> 16) & 0x3FF) + 1;
        return l < 2 ? 263 : l;
    }
    void load() {
        const uint32_t status = reg(0x10C);
        const uint32_t l      = lines();
        const uint32_t line   = std::min<uint32_t>(status & 0x3FF, l - 1);
        phase                 = ((status >> 10) & 1) * field + (uint64_t(line) * field + l - 1) / l;
    }
    bool crossed(uint64_t from, uint64_t to, uint32_t line) const {
        if (to <= from) return false;
        from += phase, to += phase;
        const uint32_t l = lines();
        if (line >= l) return false;
        const uint64_t off   = uint64_t(line) * field / l;
        auto           count = [&](uint64_t t) { return t < off ? 0ull : (t - off) / field + 1; };
        return count(to) > count(from);
    }
    uint64_t next_crossing(uint64_t after, uint32_t line) const {
        const uint32_t l = lines();
        if (line >= l) return ~0ull;
        const uint64_t off = uint64_t(line) * field / l;
        const uint64_t x   = after + phase;
        if (x < off) return off - phase;
        return off + ((x - off) / field + 1) * field - phase;
    }
    uint64_t next_line_change(uint64_t after) const {
        const uint32_t l    = lines();
        const uint64_t x    = after + phase;
        const uint64_t base = x - x % field;
        const uint32_t line = uint32_t((x % field) * l / field);
        const uint64_t next = line + 1 >= l ? field : (uint64_t(line + 1) * field + l - 1) / l;
        return base + next - phase;
    }
    uint32_t status(uint64_t now) const {
        const uint64_t t       = now + phase;
        const uint32_t l       = lines();
        const uint32_t vbstart = reg(0x0DC) & 0x3FF, vbend = (reg(0x0DC) >> 16) & 0x3FF;
        const uint32_t line    = uint32_t((t % field) * l / field);
        const bool     vsync   = vbstart > vbend ? (line >= vbstart || line < vbend) : (line >= vbstart && line < vbend);
        const bool     fld     = (reg(0x0D0) & 0x10) && ((t / field) & 1);
        return (line & 0x3FF) | (fld ? 1u << 10 : 0) | (vsync ? 1u << 13 : 0);
    }
};

// provedor de tempo que conta os avisos de nova temporizacao e guarda o SPG que cada avaliacao dos eventos vencidos viu
struct Contador final : hh::TimeProvider {
    int                   changed = 0;
    std::vector<uint32_t> load, vblankInt;  // SPG_LOAD e SPG_VBLANK_INT em cada holly_due
    const char* name() const override { return "contador"; }
    void        reset(uint64_t) override {}
    uint64_t    clock(uint64_t cycles) override { return cycles; }
    uint32_t    holly_due(uint64_t) override {
        load.push_back(hh::spg_bus_reg(0x0D8)), vblankInt.push_back(hh::spg_bus_reg(0x0CC));
        return 0;
    }
    void        spg_changed() override { ++changed; }
    void        render_scheduled(uint64_t) override {}
    void        maple_scheduled(uint64_t) override {}
    void        maple_cancel() override {}
    void        disc_scheduled(uint64_t) override {}
    bool        disc_due(uint64_t) override { return false; }
    void        disc_cancel() override {}
    uint32_t    rtc_seconds(uint64_t) override { return 0; }
    void        rtc_set(uint32_t, uint64_t) override {}
};

} // namespace

HH_TEST(hh15_spg_contas_iguais_as_do_modelo_anterior) {
    std::vector<uint32_t> bank(2048);
    const uint64_t        fields[] = {hh::kCompatTicksPerField, 263000, 1000, 7};
    const uint32_t        loads[]  = {0, 0x00010000, 0x01060000, 0x020C0000, 0x03FF0000};  // 263, 2, 263, 525, 1024 linhas
    const uint32_t        statuses[] = {0, 0x15, 0x105, 0x400 | 0x0F0, 0x3FF};
    const uint32_t        vblanks[]  = {0x00150104, 0x01040015, 0};  // fim > inicio, fim < inicio, vazio
    const uint32_t        lines[]    = {0, 1, 0x15, 0x104, 262, 263, 524, 1023};
    uint64_t              checked = 0, mismatches = 0;
    for (uint64_t f : fields)
        for (uint32_t ld : loads)
            for (uint32_t st : statuses)
                for (uint32_t vb : vblanks)
                    for (uint32_t ctl : {0u, 0x10u}) {
                        std::fill(bank.begin(), bank.end(), 0);
                        bank[0x0CC / 4] = 0x01040015, bank[0x0D0 / 4] = ctl, bank[0x0D8 / 4] = ld;
                        bank[0x0DC / 4] = vb, bank[0x10C / 4] = st;
                        SpgTiming s(f);
                        s.load(bank.data());
                        Anterior a{bank.data(), f};
                        a.load();
                        mismatches += s.phase() != a.phase;
                        for (uint64_t t0 : {0ull, 1ull, f / 3, f - 1, f, f + 1, 2 * f + f / 2, 1000 * f + 17}) {
                            mismatches += s.status(t0) != a.status(t0);
                            mismatches += s.next_line_change(t0) != a.next_line_change(t0);
                            for (uint32_t l : lines) {
                                mismatches += s.next_crossing(t0, l) != a.next_crossing(t0, l);
                                for (uint64_t d : {0ull, 1ull, f / 263 + 1, f, 3 * f})
                                    mismatches += s.crossed(t0, t0 + d, l) != a.crossed(t0, t0 + d, l);
                            }
                            checked += 2 + std::size(lines) * 6;
                        }
                        mismatches += s.vblank_in_line() != 0x15 || s.vblank_out_line() != 0x104;
                    }
    HH_CHECK(checked > 100000);
    HH_CHECK(mismatches == 0);
}

// campo de 263000 instantes com 263 linhas: 1000 por linha; o boot deixou a linha 10 (fase 10000)
HH_TEST(hh15_spg_valores_fixos) {
    std::vector<uint32_t> bank(2048);
    bank[0x0CC / 4] = 0x01040015, bank[0x0D8 / 4] = 0x01060000, bank[0x0DC / 4] = 0x00150104, bank[0x10C / 4] = 10;
    SpgTiming s(263000);
    s.load(bank.data());
    HH_CHECK(s.phase() == 10000);
    constexpr uint32_t kVsync = 1u << 13;  // o vblank vai da linha 0x104 ate a 0x15 do campo seguinte
    HH_CHECK(s.status(0) == (10 | kVsync) && s.status(999) == (10 | kVsync) && s.status(1000) == (11 | kVsync));
    HH_CHECK(s.status(11000) == 0x15 && s.status(10999) == (0x14 | kVsync));
    HH_CHECK(s.status(250000) == (0x104 | kVsync) && s.status(249999) == 0x103);  // linha 260: inicio do vblank
    HH_CHECK(s.next_line_change(0) == 1000 && s.next_line_change(1000) == 2000);
    HH_CHECK(s.next_crossing(0, 0x15) == 11000 && s.next_crossing(11000, 0x15) == 274000);
    HH_CHECK(s.crossed(10999, 11000, 0x15) && !s.crossed(11000, 11001, 0x15));
    HH_CHECK(s.next_crossing(0, 263) == SpgTiming::kNever && !s.crossed(0, 3000000, 263));
    s.set(SpgTiming::kControl, 0x10);  // entrelacado: o campo alterna no SPG_STATUS
    HH_CHECK((s.status(253000) & (1u << 10)) && !(s.status(252999) & (1u << 10)));
}

// o modelo dos registradores do PVR delega ao servico; so SPG_LOAD e SPG_VBLANK_INT entregam os eventos vencidos com a
// configuracao anterior (holly_intc_sync antes do valor novo; revisao Codex 7d3) e avisam o provedor de tempo depois
HH_TEST(hh15_spg_servico_no_contrato) {
    std::vector<uint8_t> ram(16u << 20);
    hh::Sh4Context       c{};
    c.ram = ram.data();
    Contador t;
    hh::time_set_provider(&t);
    hh::holly_intc_reset(0);
    hh::pvr_regs_reset();
    HH_CHECK(t.changed == 1 && hh::spg_bus_reg(0x0D8) == 0);
    c.cycles = 1;  // instante novo: a escrita avalia os eventos vencidos
    hh::pvr_reg_write(&c, 0xA05F80D8, 4, 0x01060000);  // SPG_LOAD
    HH_CHECK(t.changed == 2 && hh::spg_bus_reg(0x0D8) == 0x01060000 && hh::pvr_reg(0x005F80D8) == 0x01060000);
    HH_CHECK(t.load.size() == 1 && t.load[0] == 0);  // avaliados com o SPG_LOAD anterior
    c.cycles = 2;
    hh::pvr_reg_write(&c, 0xA05F80CC, 4, 0x01040015);  // SPG_VBLANK_INT
    HH_CHECK(t.changed == 3 && hh::spg_vblank_in_line() == 0x15 && hh::spg_vblank_out_line() == 0x104);
    HH_CHECK(t.vblankInt.size() == 2 && t.vblankInt[1] == 0 && t.load[1] == 0x01060000);
    c.cycles = 3;
    hh::pvr_reg_write(&c, 0xA05F80D0, 4, 0x10);  // SPG_CONTROL: sem avaliacao nem nova temporizacao
    hh::spg_bus_write(&c, 0x0DC, 0x00150104);    // pelo funil do provedor nativo do Kamui
    HH_CHECK(t.changed == 3 && t.load.size() == 2 && hh::pvr_reg(0x005F80DC) == 0x00150104);
    uint32_t v = 0;
    HH_CHECK(hh::pvr_reg_read(&c, 0xA05F80DC, 4, v) && v == 0x00150104);
    hh::pvr_reg_write(&c, 0xA05F810C, 4, 0xFFFFFFFF);  // SPG_STATUS e so leitura
    c.cycles = (5 * hh::kCompatTicksPerField + 262) / 263;  // inicio da linha 5
    HH_CHECK(hh::pvr_reg_read(&c, 0xA05F810C, 4, v) && v == hh::spg_bus_read(&c, hh::kSpgStatusOff) && (v & 0x3FF) == 5);
    std::vector<uint32_t> dump(2048);
    hh::pvr_regs_dump(&c, dump.data());
    HH_CHECK(dump[0x0D8 / 4] == 0x01060000 && dump[0x0D0 / 4] == 0x10 && dump[0x10C / 4] == v);
    // o estado do boot: a temporizacao e a fase do feixe vem do banco
    std::vector<uint32_t> bank(2048);
    bank[0x0CC / 4] = 0x00200010, bank[0x0D8 / 4] = 0x01060000, bank[0x10C / 4] = 100;
    hh::pvr_regs_load(bank.data());
    SpgTiming s(hh::kCompatTicksPerField);
    s.load(bank.data());
    HH_CHECK(t.changed == 4 && hh::spg_vblank_in_line() == 0x10 && hh::spg_next_crossing(0, 0x10) == s.next_crossing(0, 0x10));
    HH_CHECK(hh::spg_next_line_change(7) == s.next_line_change(7) && hh::spg_crossed(0, 3, 100) == s.crossed(0, 3, 100));
    hh::pvr_regs_reset();
    hh::time_set_provider(&hh::time_legacy_provider());
}
