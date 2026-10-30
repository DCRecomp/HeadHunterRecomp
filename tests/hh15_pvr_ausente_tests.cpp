// SPDX-License-Identifier: MIT
// HH15.7d4c: o lugar dos modelos graficos no produto (runtime/system/pvr_ausente.cpp, compilado no executavel como no
// HeadHunter.exe): o SPG e o unico registrador atendido (pelo servico nativo), o banco do boot fica para a instalacao do
// Kamui, e o resto do banco grafico, a VRAM, a area 4 pela store queue, o render pelo caminho do TA, a saida de video do
// modelo e os emprestimos da verificacao sao erro explicito.
#include "test.h"

#include "cpu/sh4_runtime.h"
#include "memory/sh4_sq.h"
#include "pvr/pvr_regs.h"
#include "pvr/pvr_video.h"
#include "pvr/ta.h"
#include "pvr/vram.h"
#include "system/spg_bus.h"

#include <functional>
#include <stdexcept>
#include <string>
#include <vector>

namespace {

struct Fatal : std::runtime_error {
    using std::runtime_error::runtime_error;
};

// true se `f` parou com o erro explicito (o texto contem `what`)
bool fatal(const std::function<void()>& f, const char* what) {
    hh::sh4_set_fatal_handler([](hh::Sh4Context*, const char* w, uint32_t, uint32_t) { throw Fatal(w); });
    bool hit = false;
    try {
        f();
    } catch (const Fatal& e) {
        hit = std::string(e.what()).find(what) != std::string::npos;
    }
    hh::sh4_set_fatal_handler(nullptr);
    return hit;
}

} // namespace

HH_TEST(hh15_pvr_ausente_so_o_spg) {
    std::vector<uint8_t> ram(16u << 20);
    hh::Sh4Context       c{};
    c.ram = ram.data();
    std::vector<uint32_t> bank(2048, 0);
    bank[0x0D0 / 4] = 0x150;       // SPG_CONTROL
    bank[0x0DC / 4] = 0x00150104;  // SPG_VBLANK
    bank[0x044 / 4] = 0x0080000D;  // FB_R_CTRL: fora do SPG, so para a instalacao do Kamui
    hh::pvr_regs_reset();
    hh::pvr_regs_load(bank.data());
    HH_CHECK(hh::pvr_reg(0x005F8044) == 0x0080000D && hh::pvr_reg(0xA05F80DC) == 0x00150104);
    uint32_t v = 0;
    HH_CHECK(hh::pvr_reg_read(&c, 0xA05F80DC, 4, v) && v == hh::spg_bus_reg(0x0DC));
    HH_CHECK(hh::pvr_reg_read(&c, 0xA05F810C, 4, v) && v == hh::spg_bus_read(&c, hh::kSpgStatusOff));
    HH_CHECK(hh::pvr_reg_write(&c, 0xA05F80DC, 4, 0x00160105) && hh::spg_bus_reg(0x0DC) == 0x00160105);
    HH_CHECK(hh::pvr_reg(0x005F80DC) == 0x00160105);  // o SPG vem do servico
    HH_CHECK(hh::pvr_reg_write(&c, 0xA05F810C, 4, 0xFFFFFFFF));  // SPG_STATUS e so leitura: a escrita e ignorada
    HH_CHECK(hh::pvr_reg_read(&c, 0xA05F810C, 4, v) && v == hh::spg_bus_read(&c, hh::kSpgStatusOff));
    HH_CHECK(!hh::pvr_reg_read(&c, 0xA05F80DC, 2, v) && !hh::pvr_reg_write(&c, 0xA05F80DC, 2, 0));  // so 32 bits
    HH_CHECK(!hh::pvr_reg_read(&c, 0xA05F6900, 4, v) && !hh::pvr_reg_read(&c, 0xFFA00000, 4, v));  // fora do banco
    HH_CHECK(fatal([&] { hh::pvr_reg_read(&c, 0xA05F8000, 4, v); }, "leitura fora do contrato"));  // ID
    HH_CHECK(fatal([&] { hh::pvr_reg_write(&c, 0xA05F8014, 4, 1); }, "escrita fora do contrato"));  // STARTRENDER
    HH_CHECK(fatal([&] { hh::pvr_reg_write(&c, 0xA05F8144, 4, 0x80000000); }, "escrita fora do contrato"));  // LIST_INIT
    HH_CHECK(fatal([&] { hh::pvr_start_render(&c); }, "so na verificacao"));
    HH_CHECK(fatal([&] { hh::pvr_pal_entry(0); }, "so na verificacao"));
    HH_CHECK(fatal([&] { hh::pvr_regs_set_override(bank.data()); }, "so na verificacao"));
    hh::pvr_regs_set_override(nullptr);  // devolver o banco nao faz nada
    hh::pvr_regs_reset();
    HH_CHECK(hh::pvr_reg(0x005F8044) == 0);
}

HH_TEST(hh15_pvr_ausente_sem_vram_nem_area_4) {
    std::vector<uint8_t> ram(16u << 20);
    hh::Sh4Context       c{};
    c.ram = ram.data();
    uint32_t v = 0;
    HH_CHECK(hh::vram_data() == nullptr);  // a memoria das texturas do Kamui parte zerada
    HH_CHECK(!hh::vram_read(&c, 0x8C000000, 4, v) && !hh::vram_write(&c, 0x0C000000, 4, 0));  // area 3: nao e VRAM
    HH_CHECK(fatal([&] { hh::vram_read(&c, 0xA5000000, 4, v); }, "VRAM (area 1)"));
    HH_CHECK(fatal([&] { hh::vram_write(&c, 0x04000000, 2, 0); }, "VRAM (area 1)"));
    HH_CHECK(fatal([&] { hh::vram_set_override(ram.data()); }, "so na verificacao"));
    hh::vram_set_override(nullptr);
    std::vector<uint32_t> argb;
    uint32_t              w = 0, h = 0;
    HH_CHECK(fatal([&] { hh::pvr_video_frame(argb, w, h); }, "so na verificacao"));
    const uint32_t d[8] = {};
    HH_CHECK(!hh::ta_fifo_write(&c, 0x10000000, d));
    HH_CHECK(hh::ta_params().empty() && hh::ta_bytes() == 0);
    const std::vector<hh::TaParam> params(1);
    HH_CHECK(fatal([&] { hh::ta_set_override(&params, nullptr); }, "so na verificacao"));
    // store queue: para a RAM e semantica da CPU; para a area 4, erro explicito
    hh::sh4_sq_reset();
    for (uint32_t i = 0; i < 8; ++i) HH_CHECK(hh::sh4_sq_write(0xE0000000 + 4 * i, 4, 0x11111111u * i));
    HH_CHECK(hh::sh4_sq_write(0xFF000038, 4, 0x10) && hh::sh4_sq_target(0xE0000000) == 0x10000000);  // QACR0: area 4
    HH_CHECK(fatal([&] { hh::sh4_sq_flush(&c, 0xE0000000); }, "store queue"));
}
