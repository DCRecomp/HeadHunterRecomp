// SPDX-License-Identifier: MIT
// HH15.2: diagnosticos exclusivos de validacao (ver validacao.h). So no executavel de verificacao.
#include "debug/validacao.h"

#include "pvr/holly_intc.h"

#include <cstdio>
#include <cstdlib>
#include <string>

namespace hh {
namespace {

const uint64_t g_noise = [] { const char* v = std::getenv("HH_CICLOS_RUIDO"); return v ? std::strtoull(v, nullptr, 10) : 0ull; }();
uint64_t       g_noiseState = 0x9E3779B97F4A7C15ull;

bool envIs(const char* name, const char* value) {
    const char* v = std::getenv(name);
    return v && std::string(v) == value;
}

} // namespace

bool validation_build() { return true; }

void validation_cycle_noise(Sh4Context* c) {
    if (!g_noise) return;
    g_noiseState = g_noiseState * 6364136223846793005ull + 1442695040888963407ull;
    c->cycles += (g_noiseState >> 33) % g_noise;
}

bool validation_cadence_quantum() {
    static const bool q = envIs("HH_CADENCIA", "quantum");
    return q;
}

bool validation_tmu_stopped() {
    static const bool s = envIs("HH_TMU_MUTACAO", "parado");
    return s;
}

void sh4_irq_quantum_note(Sh4Context* c, uint32_t intevt) {
    static const bool on = std::getenv("HH_IRQ_QUANTUM") != nullptr;
    if (!on) return;
    uint32_t nrm = 0, ext = 0, m4n = 0, m4e = 0, m6n = 0;
    holly_intc_read(c, 0x005F6900, 4, nrm), holly_intc_read(c, 0x005F6904, 4, ext);
    holly_intc_read(c, 0x005F6920, 4, m4n), holly_intc_read(c, 0x005F6924, 4, m4e), holly_intc_read(c, 0x005F6930, 4, m6n);
    std::fprintf(stderr, "[irq] quantum: intevt=%03X spc=%08X vblank=%llu imask=%u istnrm=%08X istext=%08X iml4nrm=%08X "
                 "iml4ext=%08X iml6nrm=%08X\n", intevt, c->pc, static_cast<unsigned long long>(holly_vblank_count()),
                 (sh4_get_sr(*c) & kSrImask) >> 4, nrm, ext, m4n, m4e, m6n);
}

} // namespace hh
