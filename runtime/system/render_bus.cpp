// SPDX-License-Identifier: MIT
// HH15.7d4a: o pedido de render como servico nativo (system/render_bus.h). Movido de pvr/pvr_regs.cpp sem mudanca.
#include "system/render_bus.h"

#include "cpu/sh4_runtime.h"
#include "cpu/time_hooks.h"

#include <algorithm>
#include <cstdio>
#include <cstdlib>

namespace hh {
namespace {

RenderStartHook g_startHook  = nullptr;
uint64_t        g_renderDue  = 0;  // instante do fim de render pendente (0: nenhum)
int             g_startCount = 0;

// diagnostico HH_TA_DUMP=<n>: os parametros do TA e os registradores de render no n-esimo STARTRENDER. true = este e o
// pedido (o cabecalho e os registradores ja foram impressos; o chamador imprime os parametros).
bool dumpRequested(size_t params, uint32_t bytes, const uint32_t* bank) {
    static const char* dump = std::getenv("HH_TA_DUMP");
    if (!dump || g_startCount != std::atoi(dump)) return false;
    std::fprintf(stderr, "[render] STARTRENDER %d: %zu parametros, %u bytes\n", g_startCount, params, bytes);
    for (uint32_t r : {0x044u, 0x048u, 0x04Cu, 0x050u, 0x054u, 0x05Cu, 0x060u, 0x064u, 0x068u, 0x06Cu, 0x074u, 0x078u,
                       0x07Cu, 0x080u, 0x084u, 0x088u, 0x08Cu, 0x098u, 0x0B0u, 0x0B4u, 0x0B8u, 0x0E4u, 0x108u, 0x11Cu,
                       0x13Cu, 0x140u})
        std::fprintf(stderr, "[render]   reg %08X = %08X\n", 0x005F8000 + r, bank ? bank[r / 4] : 0);
    return true;
}

} // namespace

void render_bus_reset() {
    g_renderDue = 0;
    time_provider().render_scheduled(0);  // nenhum fim de render pendente
}

void render_bus_dump_param(uint32_t list, const uint32_t* w, uint32_t words) {
    std::fprintf(stderr, "[render]   L%u", list);
    for (uint32_t i = 0; i < words; ++i) std::fprintf(stderr, " %08X", w[i]);
    std::fprintf(stderr, "\n");
}

void render_bus_start(Sh4Context* c, uint32_t taBytes, size_t params, void (*render)(Sh4Context*),
                      void (*dumpParams)(), const uint32_t* bank) {
    ++g_startCount;
    if (g_startHook) g_startHook(c, g_startCount);
    static const char* stop = std::getenv("HH_STOP_ON_STARTRENDER");
    if (stop && g_startCount == std::atoi(stop))
        sh4_fatal(c, "parada pedida: STARTRENDER", c->pc, uint32_t(g_startCount));
    if (dumpRequested(params, taBytes, bank) && dumpParams) dumpParams();
    render(c);  // o render inteiro aqui (a CPU nao observa estados intermediarios)
    g_renderDue = time_now(c) + std::min<uint64_t>(450000 + uint64_t(taBytes) * 100, 1500000);
    time_provider().render_scheduled(g_renderDue);
}

int      render_bus_count() { return g_startCount; }
uint64_t render_bus_done_due() { return g_renderDue; }
void     render_bus_done_take() { g_renderDue = 0; }
void     render_bus_set_start_hook(RenderStartHook hook) { g_startHook = hook; }

} // namespace hh
