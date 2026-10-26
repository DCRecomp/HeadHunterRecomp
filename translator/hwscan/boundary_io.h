// SPDX-License-Identifier: MIT
// Derivado de recomp-BStinger translator/hwscan/boundary_io.h (MIT, Copyright (c) 2026 Blue Stinger
// Recompiled contributors; ver third_party/licenses/recomp-BStinger-LICENSE.txt).
#pragma once
// N0: fronteira declarada em TOML (config/fronteiras_us.toml).
//
//   [[fronteira]]
//   endereco    = 0x8C08BD00
//   tipo        = "direta"            # ou "indireta"
//   familia     = "shinobi"
//   subsistemas = ["sistema"]
//   provedor    = "legado"            # "nativo" quando um subsistema N1-N9 assumir
//   nome        = ""
//
//   [[dependencia_hw]]                 # HH07: acesso direto do jogo ao hardware, aceito com dono
//   funcao      = 0x8C0C4CE0
//   subsistema  = "render"
//   regioes     = ["store_queue"]      # regioes (dcmap) que a funcao pode acessar; outra = violacao
//   dono        = "HH13"               # etapa que o substitui pelo subsistema nativo
//   evidencia   = "..."                # comando/endereco que prova o acesso
#include "closure.h"

#include <filesystem>
#include <map>
#include <set>
#include <string>

namespace hh::hwscan {

struct HwDependency {
    uint32_t                 function = 0;
    std::string              subsystem, owner, evidence;
    std::set<dcmap::Region>  regions;
};

struct DeclaredBoundary {
    std::map<uint32_t, BoundaryEntry> entries;
    std::map<uint32_t, std::string>   providers;  // endereco -> "legado" | "nativo"
    std::map<uint32_t, HwDependency>  hwDeps;     // funcao do jogo -> dependencia declarada
};

std::string      boundary_toml(const std::map<uint32_t, BoundaryEntry>& b,
                               const std::map<uint32_t, std::string>& providers = {},
                               const std::map<uint32_t, HwDependency>& hwDeps = {});
DeclaredHw       declared_hw(const DeclaredBoundary& d);  // entrada de check_closure
DeclaredBoundary parse_boundary(const std::string& text);  // lanca std::runtime_error
DeclaredBoundary load_boundary(const std::filesystem::path& p);

} // namespace hh::hwscan
