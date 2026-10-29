// SPDX-License-Identifier: MIT
// HH15.2: o lugar dos diagnosticos de validacao no produto (HeadHunter.exe; ver validacao.h). Sem efeito; as variaveis
// de teste (sondas, ruido de ciclos, cadencia de referencia e mutacoes dos portoes) sao recusadas na partida, com saida
// 2: um portao nunca passa no vazio com o executavel do produto.
#include "debug/hw_probe.h"
#include "debug/validacao.h"
#include "debug/validacao_vars.h"

#include <cstdio>
#include <cstdlib>

namespace hh {
namespace {

const bool g_checked = [] {
    for (const char* n : validation_vars::kTestVars)
        if (const char* v = std::getenv(n); v && *v) {
            std::fprintf(stderr, "ERRO: %s e uma variavel de teste e o executavel do produto nao a aceita (use o hh-run)\n", n);
            std::exit(2);
        }
    return true;
}();

} // namespace

bool g_hw_probe = false;
void hw_probe_access(const Sh4Context*, uint32_t, unsigned, ProbeKind) {}
void hw_probe_dispatch(const Sh4Context*, uint32_t) {}
void hw_probe_init(const std::string&) {}
bool hw_probe_flush() { return false; }
std::string hw_probe_dump() { return {}; }

bool validation_build() { return g_checked && false; }
void validation_cycle_noise(Sh4Context*) {}
bool validation_cadence_quantum() { return false; }
bool validation_tmu_stopped() { return false; }
void sh4_irq_quantum_note(Sh4Context*, uint32_t) {}

} // namespace hh
