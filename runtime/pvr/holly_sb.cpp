// SPDX-License-Identifier: MIT
// Derivado de recomp-BStinger runtime/pvr/holly_sb.cpp (MIT, Copyright (c) 2026 Blue Stinger
// Recompiled contributors; ver third_party/licenses/recomp-BStinger-LICENSE.txt).
// System block do Holly (documentacao do KallistiOS, referencia de comportamento).
//   SB_SBREV 0x005F689C: revisao do barramento, 0x0000000B (so leitura; valor lido
//                        pelo boot do jogo em 8C1C41DC no Flycast)
// O controlador de interrupcoes (0x005F6900-0x005F693F) fica em holly_intc.cpp e e
// consultado antes deste arquivo.
//
// Registrador sem modelo proprio devolve o ultimo valor escrito. A primeira leitura
// de cada um e registrada: um registrador de status que precise de comportamento
// real (DMA em andamento, Maple) aparece aqui antes de virar divergencia.
// HH15.7d: o modelo e so da verificacao; o contrato medido (o DMA do canal 2 pelo SB_C2DST, SB_FFST, SB_SBREV e as
// escritas guardadas do sistema, G1, G2 e DMA do PVR) vive no servico nativo (system/sb_bus, o do produto), entao as
// referencias do hh-run exercitam o codigo do produto. O modelo guarda o resto, os negativos e os diagnosticos.
#include "pvr/holly_sb.h"

#include "maple/maple.h"
#include "pvr/holly_intc.h"
#include "system/sb_bus.h"

#include <array>
#include <cstdio>
#include <cstdlib>
#include <cstring>

namespace hh {
namespace {

constexpr uint32_t kBase  = 0x005F6800;
constexpr uint32_t kSize  = 0x1800;
constexpr uint32_t kSbRev = 0x005F689C;
constexpr uint32_t kC2Dst = 0x005F6808;

std::array<uint32_t, kSize / 4> g_regs{};
std::array<bool, kSize / 4>     g_reported{};
bool                            g_c2dstWritten = false;  // HH_SB_MUTACAO=c2dst_ocupado

uint32_t canon(uint32_t a) { return a < 0xE0000000 ? (a & 0x1FFFFFFF) : 0; }

} // namespace

void holly_sb_reset() {
    g_regs.fill(0);
    g_reported.fill(false);
    sb_bus_reset();
    maple_reset();
}

bool holly_sb_read(Sh4Context* c, uint32_t a, unsigned size, uint32_t& v) {
    const uint32_t p = canon(a);
    if (p < kBase || p >= kBase + kSize || size != 4) return false;
    if (maple_read(c, a, v)) return true;
    const uint32_t i = (p - kBase) / 4;
    v                = g_regs[i];
    // N2b-2, negativo (so testes): HH_SB_MUTACAO=c2dst_ocupado faz a primeira leitura de C2DST
    // depois de cada escrita dizer "ocupado". O laco sincrono 8C1C1DC2 da uma volta: no tempo
    // experimental isso tem de ser erro explicito (nenhum evento libera a espera).
    // HH_SB_MUTACAO=ffst_ocupado: a primeira leitura de SB_FFST (0x005F688C) diz "FIFO ocupada"
    // (bits 0 e 5): os lacos sincronos 8C096EE0/8C09724C dao uma volta.
    static const char* mut      = std::getenv("HH_SB_MUTACAO");
    static const bool  busyOnce = mut && std::strcmp(mut, "c2dst_ocupado") == 0;
    static bool        ffstBusy = mut && std::strcmp(mut, "ffst_ocupado") == 0;
    if (busyOnce && p == kC2Dst && g_c2dstWritten) {
        g_c2dstWritten = false;
        v              = 1;
        return true;
    }
    if (ffstBusy && p == 0x005F688C) {
        ffstBusy = false;
        v        = 0x21;
        return true;
    }
    if (sb_bus_read(c, a, size, v)) return true;  // o contrato do servico nativo
    if (!g_reported[i]) {
        g_reported[i] = true;
        std::fprintf(stderr, "[sb] leitura sem modelo: %08X -> %08X (ultimo valor escrito)\n", p, v);
    }
    return true;
}

bool holly_sb_write(Sh4Context* c, uint32_t a, unsigned size, uint32_t v) {
    const uint32_t p = canon(a);
    if (p < kBase || p >= kBase + kSize || size != 4) return false;
    if (maple_write(c, a, v)) return true;
    if (p != kSbRev) g_regs[(p - kBase) / 4] = v;
    if (p == kC2Dst) g_c2dstWritten = true;
    // Diagnostico: HH_PVR_DEBUG=1 registra o disparo do DMA do canal 2 (SB_C2DST)
    static const bool debug = std::getenv("HH_PVR_DEBUG") != nullptr;
    static int        n     = 0;
    if (debug && p == 0x005F6808 && n++ < 20)
        std::fprintf(stderr, "[sb] C2DST=%08X C2DSTAT=%08X C2DLEN=%08X\n", v, g_regs[0], g_regs[1]);
    // Diagnostico: HH_SB_LOG=1 registra as escritas nos registradores de DMA do G2
    // (0x005F7800-0x005F78FF: AICA, ext1, ext2, dev), ainda sem modelo
    static const bool g2log = std::getenv("HH_SB_LOG") != nullptr;
    static int        g2n   = 0;
    if (g2log && p >= 0x005F7800 && p < 0x005F7900 && g2n++ < 400)
        std::fprintf(stderr, "[sb] G2 escrita %08X = %08X (ciclo %llu)\n", p, v, (unsigned long long)c->cycles);
    // o contrato (SB_C2DST = 1 dispara o DMA do canal 2: system/sb_bus) no servico nativo
    sb_bus_write(c, a, size, v);
    return true;
}

} // namespace hh
