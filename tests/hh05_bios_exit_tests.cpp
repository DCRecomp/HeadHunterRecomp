// SPDX-License-Identifier: MIT
// HH05.4: a rotina de saida do jogo (MISC r4 = 1/3 ou salto ao bootstrap do IP.BIN em
// 0x8C008300) encerra o jogo nativo pelo gancho do host; sem gancho, ou com outro codigo
// do MISC, e erro fatal explicito.
#include "test.h"

#include "cpu/sh4_mem.h"
#include "cpu/sh4_runtime.h"
#include "hle/bios.h"

#include <stdexcept>
#include <vector>

namespace {

struct Exit { uint32_t code; };
struct Fatal : std::runtime_error { using std::runtime_error::runtime_error; };

uint32_t call(hh::Sh4Context& c, uint32_t addr) {
    c.pr = c.ret_expect = 0x8C010000;
    try {
        hh::sh4_lookup(addr)(&c);
    } catch (const Exit& e) {
        return e.code;
    } catch (const Fatal&) {
        return 0xFA7A1u;
    }
    return 0;
}

} // namespace

HH_TEST(hh05_game_exit_requests_end_the_program) {
    std::vector<uint8_t> ram(16u << 20);
    hh::Sh4Context       c{};
    c.ram = ram.data();
    hh::sh4_set_fatal_handler([](hh::Sh4Context*, const char* w, uint32_t, uint32_t) { throw Fatal(w); });
    hh::bios_hle_install(&c, true);
    HH_CHECK(hh::sh4_read32(&c, hh::kBiosVecMisc) == 0x8C001008u);
    const uint32_t misc = hh::sh4_read32(&c, hh::kBiosVecMisc);

    // sem gancho: fatal
    c.r[4] = 1;
    HH_CHECK(call(c, misc) == 0xFA7A1u);
    HH_CHECK(call(c, hh::kBiosExitBootstrap) == 0xFA7A1u);

    hh::bios_set_exit_hook([](hh::Sh4Context*, uint32_t code) { throw Exit{code}; });
    c.r[4] = 1;
    HH_CHECK(call(c, misc) == 1);
    c.r[4] = 3;
    HH_CHECK(call(c, misc) == 3);
    HH_CHECK(call(c, hh::kBiosExitBootstrap) == hh::kBiosExitBootstrap);
    c.r[4] = 7;  // codigo nao observado: continua fatal
    HH_CHECK(call(c, misc) == 0xFA7A1u);

    hh::bios_set_exit_hook(nullptr);
    hh::sh4_set_fatal_handler(nullptr);
}
