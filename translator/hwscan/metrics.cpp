// SPDX-License-Identifier: MIT
// Derivado de recomp-BStinger translator/hwscan/metrics.cpp (MIT, Copyright (c) 2026 Blue Stinger
// Recompiled contributors; ver third_party/licenses/recomp-BStinger-LICENSE.txt).
#include "metrics.h"
#include <vector>
#include <iterator>
#include <cctype>
#include <algorithm>
#include <fstream>
#include <filesystem>

#include <sstream>

namespace hh::hwscan {

Metrics compute_metrics(const Scan& s, const SdkId& id, const std::map<uint32_t, BoundaryEntry>& boundary,
                        const ClosureResult& closure, const std::map<uint32_t, std::string>& providers) {
    Metrics m;
    m["funcoes.total"] = int64_t(id.functions.size());
    for (const auto& [fn, f] : id.functions) {
        m[std::string("camada.") + layer_name(f.layer)]++;
        m[std::string("familia.") + family_name(f.family)]++;
    }
    for (const char* l : {"jogo", "sdk", "desconhecida", "conflito"}) m.try_emplace(std::string("camada.") + l, 0);

    // hw.*: analise estatica (deterministica, com catraca); sonda.*: execucao (informativo)
    m["hw.refs.total"] = 0;
    m.try_emplace("hw.refs.camada.jogo", 0);
    m.try_emplace("hw.refs.acessos", 0);
    m.try_emplace("hw.refs.constantes", 0);
    for (const auto& r : s.refs) {
        if (r.dynamic) {
            const auto  it = id.functions.find(r.function);
            const Layer l  = it == id.functions.end() ? Layer::Unknown : it->second.layer;
            m["sonda.acessos.total"]++;
            m[std::string("sonda.acessos.camada.") + layer_name(l)]++;
            m[std::string("sonda.subsistema.") + dcmap::subsystem_name(dcmap::region_subsystem(r.region))]++;
            continue;
        }
        m["hw.refs.total"]++;
        m[r.kind == Kind::Constant ? "hw.refs.constantes" : "hw.refs.acessos"]++;
        m[std::string("hw.subsistema.") + dcmap::subsystem_name(dcmap::region_subsystem(r.region))]++;
        if (!r.function) {
            m["hw.refs.dado"]++;
            continue;
        }
        const auto it = id.functions.find(r.function);
        if (it != id.functions.end() && it->second.layer == Layer::Game) m["hw.refs.camada.jogo"]++;
    }

    auto layerName = [&](uint32_t fn) {
        const auto it = id.functions.find(fn);
        return std::string(layer_name(it == id.functions.end() ? Layer::Unknown : it->second.layer));
    };
    for (const char* l : {"jogo", "sdk"}) {
        m.try_emplace(std::string("pref.nao_resolvido.funcoes.") + l, 0);
        m.try_emplace(std::string("sq.literal.funcoes.") + l, 0);
    }
    for (const auto& [fn, n] : s.prefUnresolved) {
        m["pref.nao_resolvido.funcoes." + layerName(fn)]++;
        m["pref.nao_resolvido.instrucoes"] += n;
    }
    for (const auto& [fn, n] : s.prefUnresolved)
        if (s.sqLiteral.count(fn)) m["pref.sq_provavel.funcoes"]++;
    for (const auto& [fn, n] : s.sqLiteral) {
        m["sq.literal.funcoes." + layerName(fn)]++;
        m["sq.literal.cargas"] += n;
    }

    m["fronteira.total"] = int64_t(boundary.size());
    m.try_emplace("fronteira.direta", 0);
    m.try_emplace("fronteira.indireta", 0);
    m.try_emplace("fronteira.provedor.legado", 0);
    m.try_emplace("fronteira.provedor.nativo", 0);
    for (const auto& [addr, e] : boundary) {
        m[std::string("fronteira.") + boundary_kind_name(e.kind)]++;
        const auto        p    = providers.find(addr);
        const std::string prov = p == providers.end() ? "legado" : p->second;
        m["fronteira.provedor." + prov]++;
        for (auto sub : e.subsystems) {
            m[std::string("fronteira.subsistema.") + dcmap::subsystem_name(sub)]++;
            if (prov == "legado") m[std::string("legado.subsistema.") + dcmap::subsystem_name(sub)]++;
        }
    }

    m["indireto.sdk_so_em_dado"] = int64_t(data_only_sdk_targets(s, id).size());
    for (const auto& e : s.edges) {
        if (e.kind != EdgeKind::Dynamic) continue;
        const auto fa = id.functions.find(e.caller), fb = id.functions.find(e.callee);
        const Layer la = fa == id.functions.end() ? Layer::Unknown : fa->second.layer;
        const Layer lb = fb == id.functions.end() ? Layer::Unknown : fb->second.layer;
        m["sonda.indiretas.total"]++;
        if (la == Layer::Game && lb != Layer::Game) m["sonda.indiretas.jogo_para_sdk"]++;
        if (la != Layer::Game && lb == Layer::Game) m["sonda.indiretas.sdk_para_jogo"]++;
        if (fb == id.functions.end()) m["sonda.indiretas.fora_da_imagem"]++;
    }
    m["fechamento.alcancadas"] = int64_t(closure.reached);
    m.try_emplace("fechamento.violacoes.sdk", 0);
    m.try_emplace("fechamento.violacoes.hw", 0);
    m["fechamento.indireto_nao_resolvido"] = int64_t(closure.unresolved.size());
    m["fechamento.dependencias_hw_declaradas"] = int64_t(closure.declaredHw.size());
    m["fechamento.dependencias_hw_regioes"]    = int64_t(closure.declaredHwRegions);
    for (const auto& v : closure.violations)
        m[v.kind == Violation::SdkBypass ? "fechamento.violacoes.sdk" : "fechamento.violacoes.hw"]++;
    return m;
}

const std::vector<std::string>& ratchet_keys() {
    static const std::vector<std::string> k = {
        "hw.refs.camada.jogo",        // jogo acessando hardware diretamente
        "fechamento.violacoes.sdk",   // chamadas que contornam a fronteira declarada
        "fechamento.violacoes.hw",    // hardware alcancado sem passar pela fronteira
        "fechamento.dependencias_hw_declaradas",  // acessos do jogo ao hardware aceitos com dono (HH07)
        "fechamento.dependencias_hw_regioes",     // pares (funcao, regiao) dessas declaracoes
        "fechamento.indireto_nao_resolvido",  // alvos indiretos possiveis sem prova (so em dado)
        "camada.desconhecida",        // funcoes sem camada atribuida
        "pref.nao_resolvido.funcoes.jogo",  // funcoes do jogo com pref de endereco calculado
        "sq.literal.funcoes.jogo",          // funcoes do jogo com a base das store queues
        "camada.conflito",
        "fronteira.provedor.legado",  // fronteira ainda servida pelo runtime legado
        "tempo.consumidores.ciclos",  // modulos do runtime que medem tempo pelo contador de ciclos (N2a)
    };
    return k;
}

std::vector<RatchetFailure> ratchet(const Metrics& base, const Metrics& now) {
    std::vector<RatchetFailure> out;
    for (const auto& k : ratchet_keys()) {
        const auto b = base.find(k);
        const auto n = now.find(k);
        const int64_t bv = b == base.end() ? 0 : b->second, nv = n == now.end() ? 0 : n->second;
        if (b != base.end() && nv > bv) out.push_back({k, bv, nv});
    }
    return out;
}

std::string metrics_toml(const Metrics& m) {
    std::ostringstream o;
    o << "# Metricas de dependencia do Dreamcast (hh-hwscan). Chaves com catraca: nao podem subir.\n";
    for (const auto& k : ratchet_keys()) o << "# catraca: " << k << "\n";
    for (const auto& [k, v] : m) o << '"' << k << "\" = " << v << "\n";
    return o.str();
}

Metrics parse_metrics(const std::string& text) {
    Metrics            m;
    std::istringstream in(text);
    std::string        line;
    while (std::getline(in, line)) {
        if (line.empty() || line[0] != '"') continue;
        const size_t q = line.find('"', 1);
        const size_t eq = line.find('=', q == std::string::npos ? 0 : q);
        if (q == std::string::npos || eq == std::string::npos) continue;
        try {
            m[line.substr(1, q - 1)] = std::stoll(line.substr(eq + 1));
        } catch (...) {
        }
    }
    return m;
}

Metrics runtime_cycle_metrics(const std::filesystem::path& dir, std::ostream* tsv) {
    namespace fs = std::filesystem;
    Metrics m;
    for (const char* k : {"tempo.consumidores.ciclos", "tempo.ciclos.funcionais", "tempo.ciclos.diagnostico",
                          "tempo.ciclos.progresso", "tempo.ciclos.entrada_compat", "tempo.ciclos.teste"})
        m[k] = 0;
    std::ostringstream out;
    out << "arquivo\tlinha\tcategoria\n";
    // o runtime e, ao lado dele, transitional/ (adaptadores do produto hibrido; revisao Codex HH09):
    // caminhos relativos ao runtime, os de transitional/ com o prefixo "transitional/"
    std::vector<std::pair<fs::path, std::string>> files;
    const fs::path trans = dir.parent_path() / "transitional";
    for (const auto& [root, prefix] : {std::pair<fs::path, std::string>{dir, ""}, {trans, "transitional/"}}) {
        std::error_code ec;
        if (!fs::is_directory(root, ec)) continue;
        for (const auto& e : fs::recursive_directory_iterator(root))
            if (e.is_regular_file() && (e.path().extension() == ".cpp" || e.path().extension() == ".h"))
                files.push_back({e.path(), prefix + fs::relative(e.path(), root).generic_string()});
    }
    std::sort(files.begin(), files.end());
    for (const auto& [path, rel] : files) {
        std::ifstream     in(path, std::ios::binary);
        std::string       text((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
        // comentarios viram espacos (posicoes e linhas preservadas)
        for (size_t i = 0; i + 1 < text.size(); ++i) {
            if (text[i] == '"') {  // literal: pula
                for (++i; i < text.size() && text[i] != '"' && text[i] != '\n'; ++i)
                    if (text[i] == '\\') ++i;
            } else if (text[i] == '/' && text[i + 1] == '/') {
                for (; i < text.size() && text[i] != '\n'; ++i) text[i] = ' ';
            } else if (text[i] == '/' && text[i + 1] == '*') {
                for (; i + 1 < text.size() && !(text[i] == '*' && text[i + 1] == '/'); ++i)
                    if (text[i] != '\n') text[i] = ' ';
                if (i + 1 < text.size()) text[i] = text[i + 1] = ' ';
            }
        }
        bool functionalFile = false;
        for (size_t p = text.find("cycles"); p != std::string::npos; p = text.find("cycles", p + 1)) {
            const bool member = (p >= 2 && text.compare(p - 2, 2, "->") == 0) || (p >= 1 && text[p - 1] == '.');
            const bool word   = p + 6 >= text.size() || !(std::isalnum(uint8_t(text[p + 6])) || text[p + 6] == '_');
            if (!member || !word) continue;
            // comando que contem a leitura: do ultimo ; { } ate o proximo ; (ou o fim da linha,
            // nas macros)
            const size_t      b    = text.find_last_of(";{}", p);
            const size_t      e    = text.find_first_of(";\n", p);
            const std::string stmt = text.substr(b == std::string::npos ? 0 : b, e == std::string::npos ? std::string::npos : e - b);
            const size_t      ls   = text.rfind('\n', p);
            const std::string line = text.substr(ls == std::string::npos ? 0 : ls + 1, text.find('\n', p) - (ls == std::string::npos ? 0 : ls + 1));
            std::string cat;
            // so as expressoes conhecidas contam como entrada de compatibilidade e progresso; outra
            // leitura nesses arquivos e consumo funcional (revisao Codex 2026-10-15)
            if (rel == "cpu/time_hooks.h" && stmt.find("clock(c->cycles)") != std::string::npos) cat = "entrada_compat";
            else if (rel == "cpu/sh4_gen.h" &&
                     (line.find("#define HH_CYCLES(n) (c->cycles += (n))") != std::string::npos ||
                      line.find("if (c->cycles >= c->next_event)") != std::string::npos))
                cat = "progresso";
            // o poll central reprograma o quantum: no BS fica em sh4_runtime.cpp, no HH em
            // sh4_service.cpp (HH09.0: a catraca contava esta linha como consumo funcional)
            else if ((rel == "cpu/sh4_runtime.cpp" || rel == "cpu/sh4_service.cpp") &&
                     stmt.find("c->next_event = c->cycles + kServiceQuantum") != std::string::npos)
                cat = "progresso";
            // hardware_wait do adaptador de tempo: anda o contador de compatibilidade como o laco de
            // espera gerado (lista fechada; qualquer outro uso no adaptador e funcional)
            else if (rel == "transitional/time_adapter.cpp" &&
                     (stmt.find("const uint64_t end = c->cycles + ticks") != std::string::npos ||
                      stmt.find("while (c->cycles < end && !c->redirect)") != std::string::npos ||
                      stmt.find("c->cycles = std::min(end, std::max(c->cycles, c->next_event))") != std::string::npos ||
                      stmt.find("if (c->cycles >= c->next_event) sh4_service(c)") != std::string::npos))
                cat = "progresso";
            // stub de hardware dos testes do HH04 (hh_runtime_hw_m3, fora do produto): a mesma linha
            else if (rel == "cpu/sh4_hw_m3.cpp" && stmt.find("c->next_event = c->cycles + kServiceQuantum") != std::string::npos)
                cat = "teste";
            else if (rel.rfind("debug/", 0) == 0) cat = "diagnostico";
            else if (stmt.find("printf") != std::string::npos || stmt.find("event_trace(") != std::string::npos)
                cat = "diagnostico";
            else cat = "funcional";
            if (cat == "funcional") {
                functionalFile = true;
                m["tempo.ciclos.funcionais"]++;
            } else m["tempo.ciclos." + cat]++;
            out << rel << '\t' << 1 + std::count(text.begin(), text.begin() + p, '\n') << '\t' << cat << '\n';
        }
        if (functionalFile) m["tempo.consumidores.ciclos"]++;
    }
    if (tsv) *tsv << out.str();
    return m;
}


} // namespace hh::hwscan
