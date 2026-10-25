// SPDX-License-Identifier: MIT
// Derivado de recomp-BStinger runtime/cpu/sh4_runtime.cpp (MIT, Copyright (c) 2026 Blue Stinger
// Recompiled contributors; ver third_party/licenses/recomp-BStinger-LICENSE.txt).
// Implementacao do contrato de sh4_runtime.h: despacho real e tratamento
// explicito (log + abort) de tudo que ainda nao existe. Os pontos de contato com
// o hardware (poll, esperas, store queue, memoria fora da RAM) ficam em
// sh4_hw_m3.cpp ate o HH05 trazer o hardware temporario.
#include "cpu/sh4_runtime.h"

#include "cpu/sh4_mem.h"

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <unordered_map>

namespace hh {
namespace {

const Sh4FuncEntry* g_table = nullptr;
size_t              g_count = 0;
Sh4FatalFn          g_fatal = nullptr;
std::unordered_map<uint32_t, Sh4Func> g_hle;

[[noreturn]] void fatal(Sh4Context* c, const char* what, uint32_t pc, uint32_t extra) {
    if (g_fatal) g_fatal(c, what, pc, extra);  // pode lancar
    std::fprintf(stderr, "[sh4] FATAL: %s em PC=%08X (valor %08X) PR=%08X R15=%08X\n", what, pc, extra,
                 c ? c->pr : 0, c ? c->r[15] : 0);
    std::fflush(stderr);
    std::abort();
}

} // namespace

Sh4TraceFn g_sh4_trace = nullptr;

void sh4_set_fatal_handler(Sh4FatalFn fn) { g_fatal = fn; }

void sh4_fatal(Sh4Context* c, const char* what, uint32_t pc, uint32_t extra) { fatal(c, what, pc, extra); }

void sh4_set_function_table(const Sh4FuncEntry* table, size_t count) {
    g_table = table;
    g_count = count;
}

// Substituicoes feitas por mods (runtime/mods): valem antes da tabela gerada.
static std::unordered_map<uint32_t, Sh4Func> g_override;

void sh4_override(uint32_t addr, Sh4Func fn) {
    if (fn) g_override[sh4_phys(addr)] = fn;
    else g_override.erase(sh4_phys(addr));
}

Sh4Func sh4_lookup(uint32_t addr) {
    const uint32_t p  = sh4_phys(addr);
    if (!g_override.empty())
        if (const auto o = g_override.find(p); o != g_override.end()) return o->second;
    const auto*    it = std::lower_bound(g_table, g_table + g_count, p,
                                         [](const Sh4FuncEntry& e, uint32_t v) { return e.addr < v; });
    if (it != g_table + g_count && it->addr == p) return it->fn;
    const auto h = g_hle.find(p);
    return h != g_hle.end() ? h->second : nullptr;
}

Sh4Func sh4_lookup_generated(uint32_t addr) {
    const uint32_t p  = sh4_phys(addr);
    const auto*    it = std::lower_bound(g_table, g_table + g_count, p,
                                         [](const Sh4FuncEntry& e, uint32_t v) { return e.addr < v; });
    if (it != g_table + g_count && it->addr == p) return it->fn;
    const auto h = g_hle.find(p);
    return h != g_hle.end() ? h->second : nullptr;
}

void sh4_register_hle(uint32_t addr, Sh4Func fn) { g_hle[sh4_phys(addr)] = fn; }

void sh4_dispatch(Sh4Context* c, uint32_t from_pc, uint32_t target) {
    if (Sh4Func f = sh4_lookup(target)) {
        f(c);
        return;
    }
    sh4_dispatch_miss(c, from_pc, target);
}

void sh4_dispatch_miss(Sh4Context* c, uint32_t from_pc, uint32_t target) {
    fatal(c, "desvio indireto para endereco sem funcao recompilada", from_pc, target);
}

void sh4_bad_return(Sh4Context* c, uint32_t pc, uint32_t target) {
    fatal(c, "retorno para endereco inesperado", pc, target);
}

void sh4_unreachable(Sh4Context* c, uint32_t pc) { fatal(c, "fluxo chegou em dado/codigo nao analisado", pc, 0); }

void sh4_illegal(Sh4Context* c, uint32_t pc, uint16_t opcode) { fatal(c, "instrucao ilegal", pc, opcode); }

void sh4_trapa(Sh4Context* c, uint32_t imm) { fatal(c, "trapa (ainda nao implementado)", c->pc, imm); }

void sh4_address_error(Sh4Context* c, uint32_t addr, bool write) {
    fatal(c, write ? "address error (escrita desalinhada)" : "address error (leitura desalinhada)", c->pc, addr);
}


void sh4_ldtlb(Sh4Context* c) { fatal(c, "ldtlb (MMU nao suportada)", c->pc, 0); }


void sh4_check_code(Sh4Context* c, const uint32_t* pcs, const uint16_t* ops, size_t n) {
    for (size_t k = 0; k < n; ++k)
        if (sh4_read16(c, pcs[k]) != ops[k]) fatal(c, "codigo em RAM difere do compilado (relocated)", pcs[k], ops[k]);
}

} // namespace hh
