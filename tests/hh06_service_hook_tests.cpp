// SPDX-License-Identifier: MIT
// HH06 (revisao Codex): o gancho do poll central deixa o host parar o jogo mesmo sem VBlank.
#include "test.h"

#include "cpu/sh4_runtime.h"

#include <vector>

namespace {
struct Stop {};
int g_calls = 0;
} // namespace

HH_TEST(hh06_service_hook_can_stop_without_vblank) {
    std::vector<uint8_t> ram(16u << 20);
    hh::Sh4Context       c{};
    c.ram = ram.data();
    hh::sh4_set_service_hook([](hh::Sh4Context*) {
        if (++g_calls == 3) throw Stop{};
    });
    bool stopped = false;
    try {
        for (int i = 0; i < 10; ++i) hh::sh4_service(&c);  // guest preso: so polls, nenhum VBlank
    } catch (const Stop&) {
        stopped = true;
    }
    hh::sh4_set_service_hook(nullptr);
    HH_CHECK(stopped && g_calls == 3);
    hh::sh4_service(&c);  // sem gancho: nao chama mais
    HH_CHECK(g_calls == 3);
}
