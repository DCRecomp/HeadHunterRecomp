// SPDX-License-Identifier: MIT
// Derivado de recomp-BStinger translator/hwscan/boundary_io.cpp (MIT, Copyright (c) 2026 Blue Stinger
// Recompiled contributors; ver third_party/licenses/recomp-BStinger-LICENSE.txt).
#include "boundary_io.h"

#include <toml++/toml.hpp>

#include <cstdio>
#include <fstream>
#include <sstream>
#include <stdexcept>

namespace hh::hwscan {

namespace {

std::string hex(uint32_t v) {
    char b[16];
    std::snprintf(b, sizeof b, "0x%08X", v);
    return b;
}

Family familyByName(const std::string& n) {
    for (int i = 0; i <= int(Family::Crt); ++i)
        if (n == family_name(Family(i))) return Family(i);
    return Family::Unknown;
}

std::string regionList(const std::set<dcmap::Region>& rs) {
    std::string out;
    for (auto r : rs) out += (out.empty() ? "\"" : ", \"") + std::string(dcmap::region_name(r)) + "\"";
    return out;
}

bool regionByName(const std::string& n, dcmap::Region& out) {
    for (int i = 1; i <= int(dcmap::Region::P4Other); ++i)
        if (n == dcmap::region_name(dcmap::Region(i))) {
            out = dcmap::Region(i);
            return true;
        }
    return false;
}

} // namespace

DeclaredHw declared_hw(const DeclaredBoundary& d) {
    DeclaredHw out;
    for (const auto& [fn, h] : d.hwDeps) out[fn] = h.regions;
    return out;
}

std::string boundary_toml(const std::map<uint32_t, BoundaryEntry>& b, const std::map<uint32_t, std::string>& providers,
                          const std::map<uint32_t, HwDependency>& hwDeps) {
    std::ostringstream o;
    o << "# Fronteira declarada entre o jogo e o SDK/hardware (N0, docs/N0-RELATORIO.md).\n"
         "# Gerada por hh-hwscan --fronteira-gerar; conferida por hh-hwscan --fronteira (teste\n"
         "# n0_fechamento). Uma chamada do jogo para fora desta lista e uma violacao.\n"
         "# provedor: \"legado\" (runtime atual) ou \"nativo\" (subsistema N1-N9 ja migrado).\n";
    for (const auto& [addr, e] : b) {
        const auto p = providers.find(addr);
        o << "\n[[fronteira]]\nendereco = " << hex(addr) << "\ntipo = \"" << boundary_kind_name(e.kind)
          << "\"\nfamilia = \"" << family_name(e.family) << "\"\nsubsistemas = [";
        bool first = true;
        for (auto sub : e.subsystems) {
            o << (first ? "" : ", ") << '"' << dcmap::subsystem_name(sub) << '"';
            first = false;
        }
        o << "]\nprovedor = \"" << (p == providers.end() ? "legado" : p->second) << "\"\n";
        if (!e.name.empty()) o << "nome = \"" << e.name << "\"\n";
        if (e.callSites) o << "chamadas = " << e.callSites << "\n";
        if (e.observed) o << "observada = true\n";
    }
    for (const auto& [fn, d] : hwDeps)
        o << "\n[[dependencia_hw]]\nfuncao = " << hex(fn) << "\nsubsistema = \"" << d.subsystem << "\"\ndono = \""
          << d.owner << "\"\nregioes = [" << regionList(d.regions) << "]\nevidencia = \"" << d.evidence << "\"\n";
    return o.str();
}

DeclaredBoundary parse_boundary(const std::string& text) {
    DeclaredBoundary d;
    toml::table      t;
    try {
        t = toml::parse(text);
    } catch (const toml::parse_error& e) {
        throw std::runtime_error(std::string("fronteira: TOML invalido: ") + std::string(e.description()));
    }
    if (const toml::array* deps = t["dependencia_hw"].as_array())
        for (const auto& node : *deps) {
            const toml::table* e = node.as_table();
            if (!e) throw std::runtime_error("dependencia_hw: entrada nao e tabela");
            const auto fn = (*e)["funcao"].value<int64_t>();
            if (!fn || *fn <= 0 || *fn > 0xFFFFFFFFll) throw std::runtime_error("dependencia_hw: funcao ausente/invalida");
            HwDependency h;
            h.function  = uint32_t(*fn);
            h.subsystem = (*e)["subsistema"].value_or(std::string());
            h.owner     = (*e)["dono"].value_or(std::string());
            h.evidence  = (*e)["evidencia"].value_or(std::string());
            // sem dono (etapa HHxx) e evidencia, a dependencia nao e aceita (criterio de saida do HH07)
            if (h.subsystem.empty() || h.evidence.empty() || h.owner.size() != 4 || h.owner.rfind("HH", 0) != 0)
                throw std::runtime_error("dependencia_hw: subsistema, dono (HHxx) e evidencia obrigatorios em " + hex(h.function));
            const toml::array* regs = (*e)["regioes"].as_array();
            if (!regs || regs->empty())
                throw std::runtime_error("dependencia_hw: regioes obrigatorias em " + hex(h.function));
            for (const auto& rn : *regs) {
                dcmap::Region reg{};
                if (!regionByName(rn.value_or(std::string()), reg))
                    throw std::runtime_error("dependencia_hw: regiao desconhecida em " + hex(h.function));
                h.regions.insert(reg);
            }
            if (d.hwDeps.count(h.function)) throw std::runtime_error("dependencia_hw: funcao repetida " + hex(h.function));
            d.hwDeps[h.function] = h;
        }
    const toml::array* arr = t["fronteira"].as_array();
    if (!arr) return d;
    for (const auto& node : *arr) {
        const toml::table* e = node.as_table();
        if (!e) throw std::runtime_error("fronteira: entrada nao e tabela");
        const auto addr = (*e)["endereco"].value<int64_t>();
        if (!addr || *addr <= 0 || *addr > 0xFFFFFFFFll) throw std::runtime_error("fronteira: endereco ausente/invalido");
        BoundaryEntry be;
        be.address = uint32_t(*addr);
        be.kind    = (*e)["tipo"].value_or(std::string("direta")) == "indireta" ? BoundaryKind::Indirect : BoundaryKind::Direct;
        be.family  = familyByName((*e)["familia"].value_or(std::string()));
        be.name    = (*e)["nome"].value_or(std::string());
        be.callSites = int((*e)["chamadas"].value_or(int64_t(0)));
        be.observed  = (*e)["observada"].value_or(false);
        if (const toml::array* subs = (*e)["subsistemas"].as_array())
            for (const auto& sn : *subs)
                for (int i = 0; i <= int(dcmap::Subsystem::System); ++i)
                    if (sn.value_or(std::string()) == dcmap::subsystem_name(dcmap::Subsystem(i)))
                        be.subsystems.insert(dcmap::Subsystem(i));
        const std::string prov = (*e)["provedor"].value_or(std::string("legado"));
        if (prov != "legado" && prov != "nativo") throw std::runtime_error("fronteira: provedor invalido em " + hex(be.address));
        if (d.entries.count(be.address)) throw std::runtime_error("fronteira: endereco repetido " + hex(be.address));
        d.providers[be.address] = prov;
        d.entries[be.address]   = be;
    }
    return d;
}

DeclaredBoundary load_boundary(const std::filesystem::path& p) {
    std::ifstream f(p, std::ios::binary);
    if (!f) throw std::runtime_error("fronteira: nao foi possivel abrir o arquivo");
    std::ostringstream ss;
    ss << f.rdbuf();
    return parse_boundary(ss.str());
}

} // namespace hh::hwscan
