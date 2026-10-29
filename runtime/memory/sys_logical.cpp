// SPDX-License-Identifier: MIT
// HH15.1: servicos estreitos do tempo logico (ver sys_logical.h).
#include "memory/sys_logical.h"

#include "cpu/sh4_intc.h"
#include "cpu/sh4_runtime.h"
#include "cpu/time_hooks.h"
#include "debug/clock_log.h"
#include "memory/sh4_ccn.h"
#include "memory/sh4_port.h"
#include "memory/sh4_tmu.h"

#include <cstdio>
#include <cstdlib>

namespace hh {
namespace {

constexpr uint32_t kTocr = 0xFFD80000, kTstr = 0xFFD80004, kTcor0 = 0xFFD80008, kTcnt0 = 0xFFD8000C, kTcr0 = 0xFFD80010;
constexpr uint32_t kTmuEnd = 0xFFD8002F;
constexpr uint32_t kTcr0Contract = 2;     // P/64, sem UNIE nem fonte externa (0x8C15E840)
constexpr uint64_t kTicksDivider = 4 * 64;  // ciclos de 200 MHz por tick de P/64 (P = 50 MHz)
constexpr uint32_t kCcr = 0xFF00001C;
constexpr uint32_t kCcrWriteOnly = (1u << 11) | (1u << 3);  // ICI, OCI
constexpr uint32_t kOcAddrArray = 0xF4000000, kOcAddrArrayEnd = 0xF5000000;  // array de enderecos do cache de operandos
constexpr uint32_t kCacheArrays = 0xF0000000, kCacheArraysEnd = 0xF8000000;  // todos os arrays (o modelo atende a faixa)
constexpr uint32_t kPctra = 0x1F80002C, kPdtra = 0x1F800030;
constexpr uint32_t kIcr = 0xFFD00000, kIpra = 0xFFD00004, kIprc = 0xFFD0000C;
constexpr uint32_t kIntevt = 0xFF000028;

struct Tmu {
    bool     on = true;  // false: fora do contrato uma vez, o modelo atende dali em diante
    uint8_t  tocr = 0, tstr = 0;
    uint32_t tcor = 0xFFFFFFFF, tcr = 0, base = 0xFFFFFFFF;
    uint64_t baseAt = 0;  // instante logico (base de compatibilidade) em que TCNT0 valia `base`
};
struct State {
    Tmu      tmu;
    bool     ccnOn = true, portOn = true, intcOn = true;
    uint32_t ccr = 0;
    uint16_t icr = 0, ipr[3] = {0, 0, 0};
    SysLogicalStats stats;
};
State g;

uint32_t tcnt(uint64_t now) {
    const Tmu& t = g.tmu;
    if (!(t.tstr & 1) || now <= t.baseAt) return t.base;
    const uint64_t ticks = (now - t.baseAt) / kTicksDivider;
    if (ticks <= t.base) return t.base - uint32_t(ticks);
    const uint64_t period = uint64_t(t.tcor) + 1;  // underflow: recarga de TCOR (como o modelo)
    return t.tcor - uint32_t((ticks - t.base - 1) % period);
}

void rebase(uint64_t now) {
    g.tmu.base   = tcnt(now);
    g.tmu.baseAt = now;
}

// Fora do contrato: conta, avisa uma vez e entrega o estado ao modelo da familia.
void outside(const char* fam, uint32_t a, unsigned size, bool write, uint32_t v) {
    if (!g.stats.outside++) {
        char b[160];
        std::snprintf(b, sizeof b, "%s %s %08X (tamanho %u, valor %08X)", fam, write ? "escrita" : "leitura", a, size, v);
        g.stats.firstOutside = b;
        std::fprintf(stderr, "[sys] tempo logico: acesso fora do contrato: %s; o modelo atende\n", b);
    }
}

bool tmuOut(Sh4Context* c, uint32_t a, unsigned size, bool write, uint32_t v) {
    outside("TMU", a, size, write, v);
    Tmu& t = g.tmu;
    sh4_tmu_adopt(t.tocr, t.tstr, t.tcor, t.tcr, t.base, t.baseAt);
    t.on = false;
    (void)c;
    return false;
}

bool tmuRead(Sh4Context* c, uint32_t a, unsigned size, uint32_t& v) {
    if (a == kTstr && size == 1) v = g.tmu.tstr;
    else if (a == kTcnt0 && size == 4) {
        // negativo (HH15.1): HH_TMU_MUTACAO=parado congela o TCNT0 (a conferencia das referencias tem de acusar)
        static const bool stopped = [] { const char* m = std::getenv("HH_TMU_MUTACAO"); return m && std::string(m) == "parado"; }();
        v = stopped ? g.tmu.base : tcnt(time_now(c));
        if (g_clock_log) clock_log_read(ClockRead::Tmu, v);
    } else return tmuOut(c, a, size, false, 0);
    ++g.stats.tmu;
    return true;
}

bool tmuWrite(Sh4Context* c, uint32_t a, unsigned size, uint32_t v) {
    const uint64_t now = time_now(c);
    Tmu&           t   = g.tmu;
    if (a == kTocr && size == 1 && (v & 0xFF) == 0) t.tocr = 0;
    else if (a == kTstr && size == 1 && (v & 0xFE) == 0) rebase(now), t.tstr = uint8_t(v & 1);
    else if (a == kTcr0 && size == 2 && (v & 0xFFFF) == kTcr0Contract) rebase(now), t.tcr = kTcr0Contract;
    else if (a == kTcor0 && size == 4 && v == 0xFFFFFFFF) t.tcor = v;
    else if (a == kTcnt0 && size == 4 && v == 0xFFFFFFFF) t.base = v, t.baseAt = now;
    else return tmuOut(c, a, size, true, v);
    ++g.stats.tmu;
    return true;
}

uint32_t portCanon(uint32_t a) { return (a >= 0xE0000000 || (a >> 29) == 0) ? (a & 0x1FFFFFFF) : 0; }

} // namespace

void sys_logical_reset() { g = State{}; }

bool sys_logical_read(Sh4Context* c, uint32_t a, unsigned size, uint32_t& v) {
    if (!sh4_logical_cadence()) return false;
    if (a >= kTocr && a <= kTmuEnd) return g.tmu.on && tmuRead(c, a, size, v);
    if (a == kCcr && g.ccnOn) {
        if (size != 4) {
            outside("CCN", a, size, false, 0), sh4_ccn_adopt(g.ccr), g.ccnOn = false;
            return false;
        }
        v = g.ccr, ++g.stats.ccn;
        return true;
    }
    if (a >= kCacheArrays && a < kCacheArraysEnd && g.ccnOn) {  // leitura de array: fora do contrato (so escritas)
        outside("CCN", a, size, false, 0), sh4_ccn_adopt(g.ccr), g.ccnOn = false;
        return false;
    }
    if (const uint32_t p = portCanon(a); (p == kPctra || p == kPdtra) && g.portOn) {
        if (p != kPdtra || size != 2) {
            outside("porta", a, size, false, 0), g.portOn = false;  // o modelo tem o mesmo estado (PCTRA/PDTRA em 0)
            return false;
        }
        v = uint16_t(0x0300 & (uint32_t(sh4_port_cable()) << 8));  // PCTRA = 0: todos os pinos de entrada
        ++g.stats.port;
        return true;
    }
    if (a == kIntevt && g.intcOn) {  // o tratador 0x8C00FA00 le o codigo da interrupcao aceita
        if (size != 4) {
            outside("INTC", a, size, false, 0), sh4_intc_adopt(g.icr, g.ipr), g.intcOn = false;
            return false;
        }
        v = sh4_irq_intevt(), ++g.stats.intc;
        return true;
    }
    if (a >= kIcr && a <= kIprc && g.intcOn) {  // ICR/IPR nunca sao lidos no contrato
        outside("INTC", a, size, false, 0), sh4_intc_adopt(g.icr, g.ipr), g.intcOn = false;
        return false;
    }
    return false;
}

bool sys_logical_write(Sh4Context* c, uint32_t a, unsigned size, uint32_t v) {
    if (!sh4_logical_cadence()) return false;
    if (a >= kTocr && a <= kTmuEnd) return g.tmu.on && tmuWrite(c, a, size, v);
    if (a == kCcr && g.ccnOn) {
        if (size != 4) {
            outside("CCN", a, size, true, v), sh4_ccn_adopt(g.ccr), g.ccnOn = false;
            return false;
        }
        g.ccr = v & ~kCcrWriteOnly, ++g.stats.ccn;
        return true;
    }
    if (a >= kCacheArrays && a < kCacheArraysEnd && g.ccnOn) {
        if (a < kOcAddrArray || a >= kOcAddrArrayEnd || size != 4) {  // so o array de enderecos do cache de operandos
            outside("CCN", a, size, true, v), sh4_ccn_adopt(g.ccr), g.ccnOn = false;
            return false;
        }
        ++g.stats.ccn;  // sem efeito no host (nao ha cache a invalidar)
        return true;
    }
    if (const uint32_t p = portCanon(a); (p == kPctra || p == kPdtra) && g.portOn) {
        outside("porta", a, size, true, v), g.portOn = false;
        return false;
    }
    if (a == kIntevt && g.intcOn) {  // escrita no INTEVT: fora do contrato (o jogo so le)
        outside("INTC", a, size, true, v), sh4_intc_adopt(g.icr, g.ipr), g.intcOn = false;
        return false;
    }
    if (a >= kIcr && a <= kIprc && g.intcOn) {
        if (size != 2 || (a & 3)) {
            outside("INTC", a, size, true, v), sh4_intc_adopt(g.icr, g.ipr), g.intcOn = false;
            return false;
        }
        if (a == kIcr) g.icr = uint16_t(v);
        else g.ipr[(a - kIpra) / 4] = uint16_t(v);
        ++g.stats.intc;
        return true;
    }
    return false;
}

SysLogicalStats sys_logical_stats() { return g.stats; }

std::string sys_logical_summary() {
    const SysLogicalStats& s = g.stats;
    if (!s.tmu && !s.ccn && !s.port && !s.intc && !s.outside) return {};
    char b[320];
    std::snprintf(b, sizeof b, "[sys] tempo logico: TMU %llu, CCN %llu, porta %llu, INTC %llu; fora do contrato %llu%s%s\n",
                  (unsigned long long)s.tmu, (unsigned long long)s.ccn, (unsigned long long)s.port,
                  (unsigned long long)s.intc, (unsigned long long)s.outside, s.outside ? " (primeiro: " : "",
                  s.outside ? (s.firstOutside + ")").c_str() : "");
    return b;
}

} // namespace hh
