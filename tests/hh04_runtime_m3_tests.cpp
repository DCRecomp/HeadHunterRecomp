// SPDX-License-Identifier: MIT
// HH04 (revisao Codex): sem hardware, nenhum ponto de contato pode retornar em silencio
// quando o efeito real dependeria de hardware ausente. Cada caso precisa terminar no
// tratador fatal.
#include "test.h"

#include "cpu/sh4_intc.h"
#include "cpu/sh4_mem.h"
#include "cpu/sh4_runtime.h"

#include <stdexcept>
#include <vector>

namespace {

struct Fatal : std::runtime_error {
    using std::runtime_error::runtime_error;
};

template <class F>
bool fatal_on(F&& fn) {
    hh::sh4_set_fatal_handler([](hh::Sh4Context*, const char* what, uint32_t, uint32_t) { throw Fatal(what); });
    bool hit = false;
    try {
        fn();
    } catch (const Fatal&) {
        hit = true;
    }
    hh::sh4_set_fatal_handler(nullptr);
    return hit;
}

} // namespace

HH_TEST(hh04_m3_contact_points_are_fatal) {
    std::vector<uint8_t> ram(16u << 20);
    hh::Sh4Context       c{};
    c.ram = ram.data();
    HH_CHECK(fatal_on([&] { hh::sh4_sleep(&c); }));
    HH_CHECK(fatal_on([&] { hh::sh4_wait_point(&c, 0x8C010000); }));
    HH_CHECK(fatal_on([&] { hh::sh4_wait_end(&c, 0x8C010000); }));
    const uint32_t pts[] = {0x8C010000};
    HH_CHECK(fatal_on([&] { hh::sh4_set_reentry_points(pts, 1); }));
    HH_CHECK(!fatal_on([&] { hh::sh4_set_reentry_points(pts, 0); }));
    HH_CHECK(fatal_on([&] { hh::sh4_sq_flush(&c, 0xE0000000); }));
    HH_CHECK(fatal_on([&] { (void)hh::sh4_read32(&c, 0xA05F8000); }));  // registrador do PVR
    HH_CHECK(fatal_on([&] { hh::sh4_write32(&c, 0x8C000002, 1); }));    // desalinhado
    HH_CHECK(fatal_on([&] { hh::sh4_dispatch(&c, 0x8C010000, 0x8C123456); }));
    // Exatos sem fontes de evento: nao sao fatais.
    HH_CHECK(!fatal_on([&] { hh::sh4_service(&c); hh::sh4_irq_check(&c); }));
    HH_CHECK(c.next_event == c.cycles + hh::kServiceQuantum);
    // RAM principal em P0/P1/P2: caminho rapido, sem hardware.
    HH_CHECK(!fatal_on([&] { hh::sh4_write32(&c, 0xAC001000, 0x12345678); }));
    HH_CHECK(hh::sh4_read32(&c, 0x8C001000) == 0x12345678 && hh::sh4_read32(&c, 0x0C001000) == 0x12345678);
}
