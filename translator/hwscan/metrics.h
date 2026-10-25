// SPDX-License-Identifier: MIT
// Derivado de recomp-BStinger translator/hwscan/metrics.h (MIT, Copyright (c) 2026 Blue Stinger
// Recompiled contributors; ver third_party/licenses/recomp-BStinger-LICENSE.txt).
#pragma once
// N0: metricas de dependencia do Dreamcast e catraca (ratchet) contra a linha de base.
//
// As metricas saem do manifesto estatico (acessos ao hardware, camadas, familias), da
// fronteira (funcoes do SDK chamadas pelo jogo, por subsistema e por provedor legado/nativo)
// e do fechamento. A linha de base versionada (config/metricas_base_us.toml) so pode melhorar:
// as chaves de ratchet_keys() nao podem passar do valor da base. Cada etapa N1-N9 baixa a base
// depois de mover os subsistemas para o provedor nativo.
#include "closure.h"

#include <cstdint>
#include <filesystem>
#include <map>
#include <ostream>
#include <string>
#include <vector>

namespace hh::hwscan {

using Metrics = std::map<std::string, int64_t>;

// provedores: endereco da fronteira -> "legado" | "nativo" (ausente = legado)
Metrics compute_metrics(const Scan& s, const SdkId& id, const std::map<uint32_t, BoundaryEntry>& boundary,
                        const ClosureResult& closure, const std::map<uint32_t, std::string>& providers);

// Chaves em que menor e melhor (nao podem subir em relacao a base).
const std::vector<std::string>& ratchet_keys();

struct RatchetFailure {
    std::string key;
    int64_t     base = 0, now = 0;
};
std::vector<RatchetFailure> ratchet(const Metrics& base, const Metrics& now);

// Formato: TOML plano, uma chave entre aspas por linha ("hw.refs.total" = 625).
// Leituras de `->cycles`/`.cycles` nos fontes do runtime (`dir`), por categoria: progresso (o
// poll central e o HH_CYCLES do gerado), entrada de compatibilidade, diagnostico, teste (stub do
// HH04, fora do produto) e funcional. tempo.consumidores.ciclos = arquivos com leitura funcional
// (catraca). `tsv`: arquivo/linha/categoria de cada leitura.
Metrics runtime_cycle_metrics(const std::filesystem::path& dir, std::ostream* tsv = nullptr);

std::string metrics_toml(const Metrics& m);
Metrics     parse_metrics(const std::string& text);

} // namespace hh::hwscan
