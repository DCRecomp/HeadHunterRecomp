// SPDX-License-Identifier: MIT
// HH15.7d4a: o pedido de render (system/render_bus) e o aviso de VBlank (system/video_bus) como servicos nativos, com os
// valores do modelo anterior (pvr/pvr_regs.cpp e pvr/pvr_video.cpp ate o HH15.7d3): a contagem e o gancho antes do
// render, o render na hora, o prazo do fim min(450000 + bytes * 100, 1500000) avisado ao provedor de tempo depois do
// render, o consumo pelo banco de status e o reset. Compilado no hh_runtime_tests e no hh15_produto_sb_tests.
#include "test.h"

#include "cpu/sh4_runtime.h"
#include "cpu/time_hooks.h"
#include "system/render_bus.h"
#include "system/video_bus.h"

#include <vector>

namespace {

// provedor de tempo que guarda os prazos de fim de render avisados
struct Prazos final : hh::TimeProvider {
    std::vector<uint64_t> scheduled;
    const char*           name() const override { return "prazos"; }
    void                  reset(uint64_t) override {}
    uint64_t              clock(uint64_t cycles) override { return cycles; }
    uint32_t              holly_due(uint64_t) override { return 0; }
    void                  spg_changed() override {}
    void                  render_scheduled(uint64_t due) override { scheduled.push_back(due); }
    void                  maple_scheduled(uint64_t) override {}
    void                  maple_cancel() override {}
    void                  disc_scheduled(uint64_t) override {}
    bool                  disc_due(uint64_t) override { return false; }
    void                  disc_cancel() override {}
    uint32_t              rtc_seconds(uint64_t) override { return 0; }
    void                  rtc_set(uint32_t, uint64_t) override {}
};

Prazos*          g_t = nullptr;
std::vector<int> g_order;  // 1 = gancho (com o numero do pedido em g_n), 2 = render
int              g_n = 0;
size_t           g_scheduledInRender = 0;

} // namespace

HH_TEST(hh15_render_bus_pedido_prazo_e_consumo) {
    hh::Sh4Context c{};
    Prazos         t;
    g_t = &t;
    hh::time_set_provider(&t);
    hh::render_bus_reset();
    HH_CHECK(t.scheduled.size() == 1 && t.scheduled[0] == 0 && hh::render_bus_done_due() == 0);
    hh::render_bus_set_start_hook([](hh::Sh4Context*, int n) { g_order.push_back(1), g_n = n; });
    const int n0 = hh::render_bus_count();
    auto      render = [](hh::Sh4Context*) { g_order.push_back(2), g_scheduledInRender = g_t->scheduled.size(); };
    c.cycles         = 1000;
    hh::render_bus_start(&c, 5000, 3, render, nullptr, nullptr);
    HH_CHECK(g_order.size() == 2 && g_order[0] == 1 && g_order[1] == 2 && g_n == n0 + 1);
    HH_CHECK(hh::render_bus_count() == n0 + 1 && g_scheduledInRender == 1);  // o prazo so depois do render
    HH_CHECK(hh::render_bus_done_due() == 1000 + 950000 && t.scheduled.back() == 1000 + 950000);
    c.cycles = 2000000;
    hh::render_bus_start(&c, 0, 0, render, nullptr, nullptr);  // o minimo
    HH_CHECK(hh::render_bus_done_due() == 2000000 + 450000);
    hh::render_bus_start(&c, 20000, 0, render, nullptr, nullptr);  // o teto
    HH_CHECK(hh::render_bus_done_due() == 2000000 + 1500000 && hh::render_bus_count() == n0 + 3);
    hh::render_bus_done_take();  // o banco de status consome quando ISTNRM 0/1/2 acendem
    HH_CHECK(hh::render_bus_done_due() == 0 && t.scheduled.size() == 4);
    hh::render_bus_start(&c, 0, 0, render, nullptr, nullptr);
    hh::render_bus_reset();  // reset: nenhum pendente, avisado; a contagem e do processo
    HH_CHECK(hh::render_bus_done_due() == 0 && t.scheduled.back() == 0 && hh::render_bus_count() == n0 + 4);
    hh::render_bus_set_start_hook(nullptr);
    hh::time_set_provider(&hh::time_legacy_provider());
    g_order.clear();
}

HH_TEST(hh15_video_bus_aviso_de_vblank) {
    int calls = 0;
    hh::video_bus_vblank();  // sem gancho: nada
    hh::video_bus_set_vblank_hook([](void* arg) { ++*static_cast<int*>(arg); }, &calls);
    hh::video_bus_vblank(), hh::video_bus_vblank();
    HH_CHECK(calls == 2);
    hh::video_bus_set_vblank_hook(nullptr, nullptr);
    hh::video_bus_vblank();
    HH_CHECK(calls == 2);
}
