// SPDX-License-Identifier: MIT
// Derivado de recomp-BStinger translator/app/recomp.cpp (MIT, Copyright (c) 2026 Blue Stinger
// Recompiled contributors; ver third_party/licenses/recomp-BStinger-LICENSE.txt).
#include "app/recomp.h"

#include "analysis/analysis.h"
#include "codegen/codegen.h"
#include "hh_extract/hash.hpp"

#include <algorithm>

#include <cstdio>
#include <fstream>
#include <iterator>
#include <ostream>
#include <stdexcept>
#include <string>

namespace fs = std::filesystem;

namespace hh {
namespace {

std::string hex(uint32_t v) {
    char b[16];
    std::snprintf(b, sizeof(b), "0x%08X", v);
    return b;
}

std::string text(const fs::path& p) {
    const std::u8string u = p.u8string();  // path::string() lanca com caracteres fora do ANSI
    return std::string(u.begin(), u.end());
}

void writeReport(const analysis::Image& img, const analysis::Analysis& a, const fs::path& dir) {
    fs::create_directories(dir);
    const auto& s = a.stats;
    std::ofstream r(dir / "report.txt", std::ios::trunc);
    r << "funcoes: " << s.functions << " (chamada " << s.by_call << ", ponteiro no pool " << s.by_pointer
      << ", ponteiro em dados " << s.by_data_pointer << ", tabela de ponteiros " << s.by_pointer_table << ", prologo " << s.by_prologue << ", config "
      << s.by_config << ")\n"
      << "instrucoes unicas alcancadas: " << s.unique_instructions << "\n"
      << "instrucoes emitidas (com duplicacao entre funcoes): " << s.instructions << "\n"
      << "tabelas de salto: " << s.jump_tables << " (heuristicas: " << s.jump_tables_heuristic << ")\n"
      << "desvios indiretos resolvidos: " << s.direct_indirects << ", via despacho: " << s.unresolved_indirects << "\n"
      << "fins mortos (dado/invalido alcancado): " << s.dead_ends << "\n"
      << "palavras de dado no codigo: " << s.pool_words << "\n"
      << "instrucoes orfas (deve ser 0): " << s.orphans << "\n"
      << "candidatas por prologo nao incluidas: " << a.prologue_candidates.size() << " (prologue_candidates.tsv)\n\navisos:\n";
    for (const auto& w : a.warnings) r << "  " << w << "\n";

    std::ofstream f(dir / "functions.tsv", std::ios::trunc);
    f << "entrada\torigem\tinstrucoes\tlabels\ttail_calls\tdiretos\tdespacho\tfins_mortos\tprimeiro_fim_morto\n";
    static const char* kOrigin[] = {"entry", "call", "pointer", "config", "data_pointer", "pointer_table", "prologue"};
    for (const auto& [addr, fn] : a.functions)
        f << hex(addr) << '\t' << kOrigin[int(fn.origin)] << '\t' << fn.insns.size() + fn.slots.size() << '\t'
          << fn.labels.size() << '\t' << fn.tail_calls.size() << '\t' << fn.direct.size() << '\t'
          << fn.unresolved.size() << '\t' << fn.dead_ends.size() << '\t'
          << (fn.dead_ends.empty() ? std::string("-") : hex(*fn.dead_ends.begin())) << '\n';

    // Mapa de cobertura: 1 byte por halfword da imagem principal, a partir de img.base
    // (0 = desconhecido, 1 = codigo, 2 = dado). Codigo relocado (destino fora da imagem,
    // p.ex. vetores em VBR+0x100) fica de fora: antes a base vinha do menor endereco de
    // codigo e deslocava o mapa inteiro quando havia [[relocated]] abaixo da imagem (HH04).
    {
        std::vector<char> map(img.bytes.size() / 2, 0);
        for (uint32_t x : a.code) if (img.inMain(x, 2)) map[(x - img.base) / 2] = 1;
        for (uint32_t x : a.data) if (img.inMain(x, 2) && !map[(x - img.base) / 2]) map[(x - img.base) / 2] = 2;
        std::ofstream cov(dir / "coverage.bin", std::ios::binary | std::ios::trunc);
        cov.write(map.data(), std::streamsize(map.size()));
    }

    std::ofstream pc(dir / "prologue_candidates.tsv", std::ios::trunc);
    pc << "entrada\n";
    for (uint32_t x : a.prologue_candidates) pc << hex(x) << '\n';

    std::ofstream j(dir / "jump_tables.tsv", std::ios::trunc);
    j << "insn\ttabela\tentrada\tlimitada\tcasos\n";
    for (const auto& [pc, jt] : a.jump_tables)
        j << hex(pc) << '\t' << hex(jt.table) << '\t' << jt.entry_size << '\t' << (jt.bounded ? "sim" : "nao") << '\t'
          << jt.targets.size() << '\n';
}

} // namespace

RecompResult recomp_run(const RecompOptions& opt, std::ostream& log) {
    const GameConfig cfg = loadConfig(opt.config);
    analysis::Image  img;
    img.base = cfg.base;
    {
        std::ifstream f(opt.executable, std::ios::binary);
        if (!f) throw std::runtime_error("nao foi possivel abrir " + text(opt.executable) + " (rode hh-extract antes)");
        img.bytes.assign(std::istreambuf_iterator<char>(f), {});
    }
    // HH04.1: o codigo gerado so vale para as versoes declaradas no config (os dois discos).
    if (!cfg.accepted_sha256.empty()) {
        Sha256 sha;
        sha.update(img.bytes.data(), img.bytes.size());
        const std::string got = sha.hexdigest();
        if (std::find(cfg.accepted_sha256.begin(), cfg.accepted_sha256.end(), got) == cfg.accepted_sha256.end())
            throw std::runtime_error("executavel com SHA-256 " + got + " nao e uma versao aceita pelo config");
        log << "executavel: SHA-256 " << got << " (versao aceita)\n";
    }
    for (const auto& r : cfg.relocated) {
        if (!img.inMain(r.source, r.size)) throw std::runtime_error("relocated: origem fora do executavel");
        img.relocated.push_back(r);
    }
    const analysis::Analysis a = analysis::analyze(img, cfg.analysis);
    writeReport(img, a, opt.analysisDir);
    const auto&  s = a.stats;
    RecompResult res;
    res.functions = s.functions;
    res.warnings  = a.warnings.size();
    char line[400];
    std::snprintf(line, sizeof line,
                  "funcoes: %zu | instrucoes: %zu unicas, %zu emitidas | tabelas de salto: %zu | despacho: %zu | "
                  "fins mortos: %zu | orfas: %zu | avisos: %zu\n",
                  s.functions, s.unique_instructions, s.instructions, s.jump_tables, s.unresolved_indirects,
                  s.dead_ends, s.orphans, a.warnings.size());
    log << line << std::flush;
    if (opt.analyzeOnly) return res;
    codegen::Options gen;
    gen.out_dir        = opt.out;
    gen.inline_pool    = cfg.inline_pool_constants;
    gen.funcs_per_file = cfg.funcs_per_file;
    gen.setjmp_funcs   = cfg.setjmp_funcs;
    gen.longjmp_funcs  = cfg.longjmp_funcs;
    gen.always_slot    = cfg.native_slots;
    gen.wait_points    = cfg.native_waits;
    const auto r       = codegen::generate(img, a, gen);
    res.files          = r.files;
    res.lines          = r.lines;
    log << "gerado: " << r.files << " arquivos, " << r.functions << " funcoes, " << r.lines << " linhas em "
        << text(opt.out) << "\n" << std::flush;
    return res;
}

} // namespace hh
