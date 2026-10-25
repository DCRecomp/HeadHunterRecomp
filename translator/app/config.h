// SPDX-License-Identifier: MIT
// Derivado de recomp-BStinger translator/app/config.h (MIT, Copyright (c) 2026 Blue Stinger
// Recompiled contributors; ver third_party/licenses/recomp-BStinger-LICENSE.txt).
#pragma once
// Leitura de config/<jogo>.toml para o tradutor.
#include "analysis/analysis.h"

#include <filesystem>
#include <set>
#include <string>

namespace hh {

struct GameConfig {
    std::string        executable = "1ST_READ.BIN";
    std::vector<std::string> accepted_sha256;  // [input] sha256: versoes aceitas (vazio = sem verificacao)
    uint32_t           base       = 0;
    analysis::Config   analysis;
    std::vector<analysis::Relocated> relocated;  // [[relocated]]: copiado para a imagem
    bool               inline_pool_constants = true;
    uint32_t           funcs_per_file        = 400;
    std::set<uint32_t> setjmp_funcs, longjmp_funcs;  // [threads]
    std::set<uint32_t> native_slots;                 // [native] slots (fronteiras migraveis)
    std::set<uint32_t> native_waits;                 // [native] esperas (back-edges de lacos de espera, N2b)
    std::vector<std::string> sdk_game_markers;       // [sdkid] jogo: mensagens do proprio jogo (HH07)
    std::set<uint32_t> hw_not_hardware;              // [hwscan] constantes_nao_hw: pc de literais que nao sao endereco (HH07)
};

GameConfig loadConfig(const std::filesystem::path& path);

} // namespace hh
