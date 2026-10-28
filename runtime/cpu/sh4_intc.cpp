// SPDX-License-Identifier: MIT
// Derivado de recomp-BStinger runtime/cpu/sh4_intc.cpp (MIT, Copyright (c) 2026 Blue Stinger
// Recompiled contributors; ver third_party/licenses/recomp-BStinger-LICENSE.txt).
// INTC (manual SH7750, cap. 19 "Interrupt Controller"; cap. 5 "Exceptions"):
//   ICR 0xFFD00000, IPRA/IPRB/IPRC 0xFFD00004/08/0C (16 bits): guardados.
//   INTEVT 0xFF000028: codigo da ultima interrupcao aceita.
//   Aceitacao: SR.BL = 0 e prioridade > SR.IMASK. Entrada: SSR = SR, SPC = PC de
//   retomada, SGR = R15, SR.MD = SR.RB = SR.BL = 1, PC = VBR + 0x600.
// IRL do Dreamcast: o Holly sinaliza os niveis 6, 4 e 2 (mascaras IML6/4/2), que o
// SH-4 recebe como interrupcoes IRL de prioridade 6/4/2 e INTEVT 0x320/0x360/0x3A0
// (0x200 + (15 - nivel) * 0x20).
#include "cpu/sh4_intc.h"
#include "debug/event_trace.h"

#include "cpu/sh4_runtime.h"
#include "cpu/time_hooks.h"
#include "pvr/holly_intc.h"

#include <cstdio>
#include <cstdlib>

namespace hh {
namespace {

constexpr uint32_t kIcr = 0xFFD00000, kIpra = 0xFFD00004, kIprb = 0xFFD00008, kIprc = 0xFFD0000C;
constexpr uint32_t kIntevt = 0xFF000028;

uint16_t g_icr = 0, g_ipr[3] = {0, 0, 0};
uint32_t g_intevt = 0;
uint64_t g_taken  = 0;  // diagnostico (sh4_irq_taken_count)
uint64_t  g_byOrigin[size_t(IrqOrigin::Count)] = {};  // HH15.1: entregas por origem
IrqOrigin g_origin      = IrqOrigin::SrCheck;          // origem das entregas agora
IrqOrigin g_scope       = IrqOrigin::Quantum;          // IrqOriginScope ativo
bool      g_scopeActive = false;

void take(Sh4Context* c, uint32_t intevt) {
    g_intevt     = intevt;
    ++g_taken;
    ++g_byOrigin[size_t(g_origin)];
    // HH15.1 (diagnostico): HH_IRQ_QUANTUM=1 lista as entregas feitas pelo quantum de ciclos
    static const bool listQuantum = std::getenv("HH_IRQ_QUANTUM") != nullptr;
    if (listQuantum && g_origin == IrqOrigin::Quantum) {
        uint32_t nrm = 0, ext = 0, m4n = 0, m4e = 0, m6n = 0;
        holly_intc_read(c, 0x005F6900, 4, nrm), holly_intc_read(c, 0x005F6904, 4, ext);
        holly_intc_read(c, 0x005F6920, 4, m4n), holly_intc_read(c, 0x005F6924, 4, m4e), holly_intc_read(c, 0x005F6930, 4, m6n);
        std::fprintf(stderr, "[irq] quantum: intevt=%03X spc=%08X vblank=%llu imask=%u istnrm=%08X istext=%08X iml4nrm=%08X "
                     "iml4ext=%08X iml6nrm=%08X\n", intevt, c->pc, static_cast<unsigned long long>(holly_vblank_count()),
                     (sh4_get_sr(*c) & kSrImask) >> 4, nrm, ext, m4n, m4e, m6n);
    }
    if (g_event_trace) event_trace(TraceEvent::Irq, c->cycles, intevt, c->pc);
    const uint32_t sr = sh4_get_sr(*c);
    c->ssr       = sr;
    c->spc       = c->pc;
    c->sgr       = c->r[15];
    if (std::getenv("HH_IRQ_DEBUG"))
        std::fprintf(stderr, "[irq] aceita intevt=%03X spc=%08X ssr=%08X r15=%08X pr=%08X\n", intevt, c->spc, sr,
                     c->r[15], c->pr);
    sh4_set_sr(*c, sr | kSrMD | kSrRB | kSrBL);
    const uint32_t saved = c->ret_expect, savedSp = c->ret_sp, savedFn = c->cur_fn;
    c->ret_expect        = c->spc;  // o rte do tratador volta para SPC (HH_RTE confere)
    c->ret_sp            = c->sgr;  // com a pilha da thread interrompida
    c->cur_fn            = 0;       // sonda: o despacho do tratador vem do runtime, nao da funcao interrompida
    sh4_dispatch(c, c->spc, c->vbr + 0x600);
    c->ret_expect = saved;
    c->ret_sp     = savedSp;
    c->cur_fn     = savedFn;
}

// Entrega a interrupcao pendente de maior prioridade, se SR permitir.
bool deliverOne(Sh4Context* c) {
    const uint32_t sr = sh4_get_sr(*c);
    if (sr & kSrBL) return false;
    const uint32_t imask = (sr & kSrImask) >> 4;
    const uint32_t level = holly_irl_level(time_now(c));
    // Diagnostico: HH_IRQ_DEBUG=1 lista as primeiras checagens com nivel pendente.
    static const bool debug = std::getenv("HH_IRQ_DEBUG") != nullptr;
    static int        dbg   = 0;
    if (debug && dbg < 40 && (level || (dbg < 20 && imask < 15))) {
        ++dbg;
        std::fprintf(stderr, "[irq] pc=%08X sr=%08X imask=%u level=%u cycles=%llu\n", c->pc, sr, imask, level,
                     (unsigned long long)c->cycles);
    }
    if (level <= imask) return false;
    take(c, 0x200 + (15 - level) * 0x20);
    return true;
}

} // namespace

uint64_t sh4_irq_taken_count() { return g_taken; }

void sh4_intc_adopt(uint16_t icr, const uint16_t ipr[3]) {
    g_icr = icr;
    for (int i = 0; i < 3; ++i) g_ipr[i] = ipr[i];
}
uint64_t sh4_irq_origin_count(IrqOrigin o) { return o < IrqOrigin::Count ? g_byOrigin[size_t(o)] : 0; }

IrqOriginScope::IrqOriginScope(IrqOrigin o) : saved_(g_scope), savedSet_(g_scopeActive) {
    g_scope       = o;
    g_scopeActive = true;
}
IrqOriginScope::~IrqOriginScope() {
    g_scope       = saved_;
    g_scopeActive = savedSet_;
}
IrqOrigin sh4_irq_service_origin(bool forced) {
    return g_scopeActive ? g_scope : forced ? IrqOrigin::Forced : IrqOrigin::Quantum;
}
IrqOrigin sh4_irq_set_origin(IrqOrigin o) {
    const IrqOrigin prev = g_origin;
    g_origin             = o;
    return prev;
}

void sh4_intc_reset() {
    g_icr = 0;
    g_ipr[0] = g_ipr[1] = g_ipr[2] = 0;
    g_intevt = 0;
}

void sh4_irq_check(Sh4Context* c) {
    // Depois do rte do tratador, uma fonte ainda pendente (ou outra) e aceita antes da
    // instrucao em SPC (manual SH7750, 19.6): por isso reavalia ate nao haver nada.
    for (int n = 0; deliverOne(c); ++n)
        if (n == 1000) sh4_fatal(c, "interrupcao nunca reconhecida pelo tratador", c->pc, g_intevt);
}

void sh4_irq_check_sr(Sh4Context* c) {
    const IrqOrigin prev = sh4_irq_set_origin(IrqOrigin::SrCheck);
    sh4_irq_check(c);
    sh4_irq_set_origin(prev);
}

bool sh4_intc_read(uint32_t a, unsigned size, uint32_t& v) {
    if (a == kIntevt && size == 4) {
        v = g_intevt;
        return true;
    }
    if (size != 2) return false;
    if (a == kIcr) v = g_icr;
    else if (a >= kIpra && a <= kIprc && !(a & 3)) v = g_ipr[(a - kIpra) / 4];
    else return false;
    return true;
}

bool sh4_intc_write(uint32_t a, unsigned size, uint32_t v) {
    if (a == kIntevt && size == 4) {
        g_intevt = v;
        return true;
    }
    if (size != 2) return false;
    if (a == kIcr) g_icr = uint16_t(v);
    else if (a >= kIpra && a <= kIprc && !(a & 3)) g_ipr[(a - kIpra) / 4] = uint16_t(v);
    else return false;
    return true;
}

} // namespace hh
