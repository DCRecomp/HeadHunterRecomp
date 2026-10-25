// SPDX-License-Identifier: MIT
// Derivado de recomp-BStinger runtime/debug/hw_probe.h (MIT, Copyright (c) 2026 Blue Stinger
// Recompiled contributors; ver third_party/licenses/recomp-BStinger-LICENSE.txt).
#pragma once
// N0: sonda dinamica de dependencias do hardware no runtime legado.
//
// Com HH_HW_PROBE=<arquivo.tsv>, registra cada acesso do guest ao hardware (todo acesso fora
// da RAM principal passa pelo caminho lento de memoria; as store queues por sh4_sq_flush) e
// cada desvio indireto despachado, atribuidos a funcao do guest em execucao (c->cur_fn). Ao
// sair, grava contagens agregadas; hh-hwscan --sonda junta isso ao manifesto estatico, o que
// cobre o que a analise estatica nao resolve (enderecos calculados, ponteiros em dado).
// Desligada, custa um teste de bool no caminho lento e no despacho; nao muda o comportamento.
#include "cpu/sh4_context.h"

#include <cstdint>
#include <string>

namespace hh {

extern bool g_hw_probe;

enum class ProbeKind : char { Read = 'r', Write = 'w', StoreQueue = 'q', Indirect = 'i' };

void hw_probe_access(const Sh4Context* c, uint32_t addr, unsigned size, ProbeKind kind);
void hw_probe_dispatch(const Sh4Context* c, uint32_t target);

// Liga a sonda (path vazio desliga) e grava o que foi coletado. hw_probe_init e chamado na
// inicializacao estatica a partir de HH_HW_PROBE; os testes chamam diretamente.
void        hw_probe_init(const std::string& path);
bool        hw_probe_flush();
std::string hw_probe_dump();  // o mesmo TSV, em memoria (testes)

} // namespace hh
