// SPDX-License-Identifier: MIT
// Derivado de recomp-BStinger tests/unit/test_n0.cpp (MIT, Copyright (c) 2026 Blue Stinger
// Recompiled contributors; ver third_party/licenses/recomp-BStinger-LICENSE.txt).
// N0: mapa de regioes, varredura estatica, identificacao do SDK, fronteira/fechamento,
// metricas (catraca) e sonda dinamica.
#include "test.h"

#include "cpu/sh4_gen.h"
#include "dc_regions.h"
#include "debug/event_trace.h"
#include "debug/hw_probe.h"
#include "hwscan/boundary_io.h"
#include "hwscan/closure.h"
#include "hwscan/metrics.h"
#include "hwscan/probe.h"
#include "hwscan/sdkid.h"

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <initializer_list>
#include <stdexcept>

using namespace hh;
using namespace hh::hwscan;
using dcmap::Region;

namespace {

constexpr uint32_t kBase = 0x8C010000;

analysis::Image image(std::initializer_list<uint16_t> words) {
    analysis::Image img;
    img.base = kBase;
    for (uint16_t w : words) {
        img.bytes.push_back(uint8_t(w));
        img.bytes.push_back(uint8_t(w >> 8));
    }
    return img;
}

analysis::Analysis analyze(const analysis::Image& img) {
    analysis::Config cfg;
    cfg.entry              = kBase;
    cfg.pointer_candidates = cfg.data_pointers = false;
    return analysis::analyze(img, cfg);
}

bool hasRef(const Scan& s, uint32_t fn, uint32_t addr, Kind k, Region r) {
    return std::any_of(s.refs.begin(), s.refs.end(), [&](const HwRef& x) {
        return x.function == fn && x.address == addr && x.kind == k && x.region == r;
    });
}

} // namespace

HH_TEST(hh07_dcmap_regions_and_subsystems) {
    HH_CHECK(dcmap::classify(0xA05F8000) == Region::Pvr);
    HH_CHECK(dcmap::classify(0x805F6900) == Region::HollyIntc);
    HH_CHECK(dcmap::classify(0xA05F6C00) == Region::Maple);
    HH_CHECK(dcmap::classify(0xA05F7018) == Region::GdRom);
    HH_CHECK(dcmap::classify(0xA0702C00) == Region::AicaRegs);
    HH_CHECK(dcmap::classify(0xA0704820) == Region::None);  // alem dos registros do DSP
    HH_CHECK(dcmap::classify(0xA0800000) == Region::AicaRam && dcmap::classify(0xA0A00000) == Region::None);
    HH_CHECK(dcmap::classify(0xA4000000) == Region::Vram64 && dcmap::classify(0x85000000) == Region::Vram32);
    HH_CHECK(dcmap::classify(0x10000000) == Region::TaFifo && dcmap::classify(0x11000000) == Region::TaTexture);
    HH_CHECK(dcmap::classify(0xE0000020) == Region::StoreQueue && dcmap::classify(0xF4000000) == Region::CacheArrays);
    HH_CHECK(dcmap::classify(0xFF000038) == Region::Ccn && dcmap::classify(0xFFD80004) == Region::Tmu);
    HH_CHECK(dcmap::classify(0xFFD00004) == Region::Intc && dcmap::classify(0xFFE80000) == Region::Scif);
    HH_CHECK(dcmap::classify(0x8C0000BC) == Region::BiosGdVector && dcmap::classify(0x8C00F400) == Region::SystemArea);
    HH_CHECK(dcmap::classify(0x8C0000B8) == Region::BiosFlashVector && dcmap::classify(0x8C0000B0) == Region::BiosVectors);
    HH_CHECK(dcmap::region_subsystem(Region::BiosGdVector) == dcmap::Subsystem::Files);
    HH_CHECK(dcmap::region_subsystem(Region::BiosFlashVector) == dcmap::Subsystem::Saves);
    HH_CHECK(dcmap::classify(0x8C010000) == Region::None && dcmap::classify(0xAC123456) == Region::None);
    HH_CHECK(dcmap::region_subsystem(Region::GdRom) == dcmap::Subsystem::Files);
    HH_CHECK(dcmap::region_subsystem(Region::Maple) == dcmap::Subsystem::Input);
    HH_CHECK(dcmap::region_subsystem(Region::AicaRam) == dcmap::Subsystem::Audio);
    HH_CHECK(dcmap::region_subsystem(Region::TaFifo) == dcmap::Subsystem::Render);
    HH_CHECK(dcmap::region_subsystem(Region::Tmu) == dcmap::Subsystem::Time);
    HH_CHECK(dcmap::region_subsystem(Region::None) == dcmap::Subsystem::None);
}

// Literais do pool: mascaras, floats, cores e padroes nao contam como endereco de hardware.
HH_TEST(hh07_plausible_constant_filters_masks_floats_colors) {
    for (uint32_t ok : {0xA05F6800u, 0xA05F8044u, 0xFF00003Cu, 0xFFD80004u, 0xA0702C00u, 0xA4000000u, 0x8C0000BCu,
                        0x10800000u, 0xF4000000u, 0xE0000020u})
        HH_CHECK(plausible_constant(ok));
    for (uint32_t no : {0xE0000000u, 0xF0000000u, 0x80000000u, 0x40000000u, 0x4F000000u, 0xFF000000u, 0x80704820u,
                        0x80808080u, 0x10000000u, 0x04000000u, 0x005F6800u, 0xFFE7FFFFu, 0xFF0F00E4u, 0xF7FFFFFFu,
                        0x0D000001u, 0x80000060u, 0x8C010000u})
        HH_CHECK(!plausible_constant(no));
    // HH07: base pura de segmento (mascara de `or`/`and` para converter ponteiro), mesmo quando
    // o espelho cai numa regiao com regra propria (0xA0000000 = BIOS por P2)
    for (uint32_t no : {0xA0000000u, 0xC0000000u, 0x60000000u, 0x20000000u})
        HH_CHECK(!plausible_constant(no));
}

// mov.l #0xA05F8000,r1; mov.l r0,@(8,r1); bsr f; nop; rts; nop | pool | f: rts; nop
HH_TEST(hh07_scan_resolves_base_displacement_and_call_edges) {
    const auto img = image({
        0xD102,          // 8C010000 mov.l 0x8C01000C,r1
        0x1102,          // 8C010002 mov.l r0,@(8,r1)  -> 0xA05F8008
        0xB004,          // 8C010004 bsr 0x8C010010
        0x0009,          // 8C010006 nop
        0x000B,          // 8C010008 rts
        0x0009,          // 8C01000A nop
        0x8000, 0xA05F,  // 8C01000C pool: 0xA05F8000
        0x000B,          // 8C010010 f: rts
        0x0009,          // 8C010012 nop
    });
    const auto a = analyze(img);
    const Scan s = scan(img, a);
    HH_CHECK(hasRef(s, kBase, 0xA05F8008, Kind::Write, Region::Pvr));
    HH_CHECK(hasRef(s, kBase, 0xA05F8000, Kind::Constant, Region::Pvr));
    HH_CHECK(std::any_of(s.edges.begin(), s.edges.end(), [](const Edge& e) {
        return e.caller == kBase && e.callee == 0x8C010010 && e.kind == EdgeKind::Call;
    }));
}

// mov.l @r1+,r2 avanca a base conhecida; a chamada zera r0-r7 (convencao do SH-4).
HH_TEST(hh07_scan_postincrement_and_call_clobber) {
    const auto img = image({
        0xD103,          // 8C010000 mov.l 0x8C010010,r1  (0xA05F6900)
        0x6216,          // 8C010002 mov.l @r1+,r2        -> le 0xA05F6900
        0x6312,          // 8C010004 mov.l @r1,r3         -> le 0xA05F6904
        0xB005,          // 8C010006 bsr 0x8C010014
        0x0009,          // 8C010008 nop
        0x6312,          // 8C01000A mov.l @r1,r3         -> r1 desconhecido depois da chamada
        0x000B,          // 8C01000C rts
        0x0009,          // 8C01000E nop
        0x6900, 0xA05F,  // 8C010010 pool: 0xA05F6900
        0x000B,          // 8C010014 rts
        0x0009,          // 8C010016 nop
    });
    const auto a = analyze(img);
    const Scan s = scan(img, a);
    HH_CHECK(hasRef(s, kBase, 0xA05F6900, Kind::Read, Region::HollyIntc));
    HH_CHECK(hasRef(s, kBase, 0xA05F6904, Kind::Read, Region::HollyIntc));
    size_t reads = 0;
    for (const auto& r : s.refs) reads += r.kind == Kind::Read;
    HH_CHECK(reads == 2);
}

HH_TEST(hh07_message_owner_extraction) {
    std::string n;
    HH_CHECK(owner_of("E1225:sdMemBlkCreate return error in mwSndOpenPort", &n) == Family::Cri && n == "mwSndOpenPort");
    HH_CHECK(owner_of("E8101208: parameter error (ADXT_SetOutPan)", &n) == Family::Cri && n == "ADXT_SetOutPan");
    HH_CHECK(owner_of("W2151 PlyCreateWav Wave handle is not created", &n) == Family::Cri && n == "PlyCreateWav");
    HH_CHECK(owner_of("FT_LoadRoomData (): Unregistered URNr", &n) == Family::Game && n == "FT_LoadRoomData");
    HH_CHECK(owner_of("FI_LoadBinary( ): njOpenBinary() failed", &n) == Family::Game && n == "FI_LoadBinary");
    HH_CHECK(owner_of("sd_drv_get_info_.c", &n) == Family::Sound);
    HH_CHECK(owner_of("KAMUI") == Family::Kamui && owner_of("KATANA_FLASH____") == Family::Shinobi);
    // o jogo relatando o resultado de uma chamada nao faz da funcao uma funcao do SDK
    HH_CHECK(owner_of("buSaveFile NO_DISK") == Family::Unknown && owner_of("buLoadFileEx") == Family::Unknown);
    HH_CHECK(owner_of("PRESS START BUTTON") == Family::Unknown);
    // HH07: "X() failed" e o chamador relatando a falha de X; "(CONSTANTE)" nao e funcao
    HH_CHECK(owner_of("kmCreateVertexBuffer() failed!", &n) == Family::Unknown && n.empty());
    HH_CHECK(owner_of("(ADXT_STAT_PLAYEND) ", &n) == Family::Unknown && n.empty());
    HH_CHECK(owner_of("kmCreateVertexBuffer()", &n) == Family::Kamui && n == "kmCreateVertexBuffer");
    HH_CHECK(owner_of("E1226:acDigiClose fail in mwSndOpenPort", &n) == Family::Cri && n == "mwSndOpenPort");
}

// As strings de cada modulo vem antes do banner dele; o bloco termina no fim do banner.
HH_TEST(hh07_module_banners_bound_data_blocks) {
    const std::string blob = std::string("abc\0", 4) + "mwMng:too many libarary" + std::string("\0\n", 2) +
                             "mwMng Ver.1.10 Build:Dec 08 1998 15:29:32" + std::string("\n\0\xFF\xFF", 4) +
                             "KATANA_FLASH____" + std::string("\0\n", 2) + "fmMng Ver 1.01 Build:Oct 12 1998" +
                             std::string("\n\0", 2);
    analysis::Image img;
    img.base = kBase;
    img.bytes.assign(blob.begin(), blob.end());
    const auto mods = find_modules(img);
    HH_CHECK(mods.size() == 2);
    if (mods.size() == 2) {
        HH_CHECK(mods[0].name == "mwMng" && mods[0].family == Family::Cri && mods[0].bounded);
        HH_CHECK(mods[1].name == "fmMng" && mods[1].family == Family::Shinobi);
        const uint32_t flash = kBase + uint32_t(blob.find("KATANA_FLASH"));
        HH_CHECK(mods[1].dataBegin <= flash && flash < mods[1].dataEnd && mods[0].dataEnd <= flash);
    }
    HH_CHECK(family_of_module("Ninja") == Family::Ninja && family_of_module("syHw") == Family::Shinobi);
    HH_CHECK(family_of_module("sd") == Family::Sound && family_of_module("ADXF") == Family::Cri);
}

namespace {

// Grafo sintetico: G (jogo) -> B (fronteira) -> H (SDK, acessa PVR); G -> X (SDK fora da
// fronteira); G faz chamada indireta e toma o endereco de Y (SDK); G3 (jogo) acessa hardware.
struct Synthetic {
    static constexpr uint32_t G = 0x8C010000, G2 = 0x8C010100, G3 = 0x8C010200;
    static constexpr uint32_t B = 0x8C100000, H = 0x8C100100, X = 0x8C100200, Y = 0x8C100300;
    Scan  s;
    SdkId id;
    Synthetic() {
        for (uint32_t f : {G, G2, G3}) id.functions[f].layer = Layer::Game;
        for (uint32_t f : {B, H, X, Y}) id.functions[f].layer = Layer::Sdk;
        s.edges = {{G, B, 0x8C010004, EdgeKind::Call},
                   {B, H, 0x8C100004, EdgeKind::Call},
                   {G, X, 0x8C010008, EdgeKind::Call},
                   {G2, 0, 0x8C010104, EdgeKind::Indirect}};
        s.refs  = {{H, 0x8C100110, 0xA05F8044, 4, Kind::Write, Region::Pvr},
                   {G3, 0x8C010210, 0xA05F6900, 4, Kind::Read, Region::HollyIntc}};
        s.addressTaken = {Y};
        s.takenBy[Y]   = {G};
    }
};

bool hasViolation(const ClosureResult& r, Violation::Kind k, uint32_t fn) {
    return std::any_of(r.violations.begin(), r.violations.end(),
                       [&](const Violation& v) { return v.kind == k && v.function == fn; });
}

} // namespace

// O fechamento detecta chamada direta e indireta que contornam a fronteira e acesso ao
// hardware fora dela; o que fica atras da fronteira (H) nao conta.
HH_TEST(hh07_closure_detects_bypasses) {
    Synthetic t;
    const auto r = check_closure(t.s, t.id, {Synthetic::B});
    HH_CHECK(hasViolation(r, Violation::SdkBypass, Synthetic::X));
    HH_CHECK(hasViolation(r, Violation::SdkBypass, Synthetic::Y));
    HH_CHECK(hasViolation(r, Violation::HardwareAccess, Synthetic::G3));
    HH_CHECK(!hasViolation(r, Violation::HardwareAccess, Synthetic::H));
    HH_CHECK(!hasViolation(r, Violation::SdkBypass, Synthetic::B));
    const auto y = std::find_if(r.violations.begin(), r.violations.end(),
                                [](const Violation& v) { return v.function == Synthetic::Y; });
    HH_CHECK(y != r.violations.end() && y->viaIndirect);

    const auto ok = check_closure(t.s, t.id, {Synthetic::B, Synthetic::X, Synthetic::Y});
    HH_CHECK(ok.violations.size() == 1 && hasViolation(ok, Violation::HardwareAccess, Synthetic::G3));
}

// A fronteira derivada tem as chamadas diretas do jogo e os enderecos que o jogo toma, com
// os subsistemas alcancaveis por tras de cada uma.
HH_TEST(hh07_boundary_derivation_and_hw_reach) {
    Synthetic  t;
    const auto b = derive_boundary(t.s, t.id);
    HH_CHECK(b.size() == 3 && b.count(Synthetic::B) && b.count(Synthetic::X) && b.count(Synthetic::Y));
    HH_CHECK(b.at(Synthetic::B).kind == BoundaryKind::Direct && b.at(Synthetic::B).callSites == 1);
    HH_CHECK(b.at(Synthetic::B).subsystems.count(dcmap::Subsystem::Render));
    HH_CHECK(b.at(Synthetic::Y).kind == BoundaryKind::Indirect);
    HH_CHECK(b.at(Synthetic::X).subsystems.empty());
}

HH_TEST(hh07_boundary_toml_roundtrip_and_errors) {
    Synthetic                      t;
    const auto                     b    = derive_boundary(t.s, t.id);
    std::map<uint32_t, std::string> prov = {{Synthetic::X, "nativo"}};
    const auto                     d    = parse_boundary(boundary_toml(b, prov));
    HH_CHECK(d.entries.size() == b.size());
    HH_CHECK(d.providers.at(Synthetic::X) == "nativo" && d.providers.at(Synthetic::B) == "legado");
    HH_CHECK(d.entries.at(Synthetic::B).subsystems == b.at(Synthetic::B).subsystems);
    HH_CHECK(d.entries.at(Synthetic::Y).kind == BoundaryKind::Indirect);
    auto throws = [](const std::string& text) {
        try {
            parse_boundary(text);
        } catch (const std::runtime_error&) {
            return true;
        }
        return false;
    };
    HH_CHECK(throws("[[fronteira]]\nendereco = 0x8C100000\nprovedor = \"emulado\"\n"));
    HH_CHECK(throws("[[fronteira]]\nendereco = 1\n[[fronteira]]\nendereco = 1\n"));
    HH_CHECK(throws("[[fronteira]]\ntipo = \"direta\"\n"));
    HH_CHECK(throws("[[fronteira\n"));
}

// A catraca so olha as chaves de dependencia: subir e regressao, descer e progresso.
HH_TEST(hh07_metrics_ratchet) {
    Synthetic  t;
    const auto b  = derive_boundary(t.s, t.id);
    const auto cr = check_closure(t.s, t.id, {Synthetic::B});
    Metrics    m  = compute_metrics(t.s, t.id, b, cr, {{Synthetic::X, "nativo"}});
    HH_CHECK(m.at("hw.refs.camada.jogo") == 1 && m.at("fronteira.provedor.nativo") == 1);
    HH_CHECK(m.at("fronteira.provedor.legado") == 2 && m.at("fechamento.violacoes.sdk") == 2);
    HH_CHECK(m.at("fechamento.violacoes.hw") == 1 && m.at("legado.subsistema.render") == 1);
    const Metrics back = parse_metrics(metrics_toml(m));
    HH_CHECK(back == m);
    Metrics worse = m;
    worse["fronteira.provedor.legado"] += 1;
    worse["hw.refs.total"] += 100;  // informativo: nao e catraca
    const auto f = ratchet(m, worse);
    HH_CHECK(f.size() == 1 && f[0].key == "fronteira.provedor.legado");
    Metrics better = m;
    better["fronteira.provedor.legado"] = 0;
    better["camada.desconhecida"]       = 0;
    HH_CHECK(ratchet(m, better).empty());
}

// TSV da sonda: acessos viram HwRef dinamicos; desvio indireto vira aresta Dynamic, que o
// fechamento segue como chamada.
HH_TEST(hh07_probe_merge_feeds_closure) {
    Synthetic t;
    analysis::Image img;
    img.base = kBase;
    img.bytes.resize(0x200000);
    const std::string tsv = "# comentario\ntipo\tfuncao\tendereco\ttamanho\tregiao\tcontagem\n"
                            "w\t0x8C010000\t0xA05F8044\t4\tpvr\t7\n"
                            "q\t0x8C100100\t0x10000000\t32\tta_fifo\t3\n"
                            "i\t0x8C010000\t0x8C100300\t0\t-\t2\n"
                            "i\t0x00000000\t0x8C010000\t0\t-\t1\n";
    const auto st = merge_probe(t.s, img, tsv);
    HH_CHECK(st.accesses == 2 && st.edges == 1 && st.skipped == 1);
    HH_CHECK(std::any_of(t.s.refs.begin(), t.s.refs.end(), [](const HwRef& r) {
        return r.dynamic && r.function == Synthetic::G && r.count == 7 && r.region == Region::Pvr;
    }));
    // G passa a acessar hardware (dinamico) e a chamar Y de fato
    const auto r = check_closure(t.s, t.id, {Synthetic::B, Synthetic::X});
    HH_CHECK(hasViolation(r, Violation::HardwareAccess, Synthetic::G));
    HH_CHECK(hasViolation(r, Violation::SdkBypass, Synthetic::Y));
    const auto b = derive_boundary(t.s, t.id);  // a chamada observada entra na fronteira
    HH_CHECK(b.count(Synthetic::Y) && b.at(Synthetic::Y).observed && b.at(Synthetic::Y).kind == BoundaryKind::Indirect);
    HH_CHECK(parse_boundary(boundary_toml(b)).entries.at(Synthetic::Y).observed);
    bool threw = false;
    try {
        merge_probe(t.s, img, "x\t1\t2\n");
    } catch (const std::runtime_error&) {
        threw = true;
    }
    HH_CHECK(threw);
}

namespace {
void probeCallee(Sh4Context* c) {
    HH_FN_ENTER(0x8C000200);
    hw_probe_access(c, 0xA05F8008, 4, ProbeKind::Write);
}
void probeCaller(Sh4Context* c) {
    HH_FN_ENTER(0x8C000100);
    HH_CALL(probeCallee(c), 0x8C000104);
    hw_probe_access(c, 0xFF000038, 4, ProbeKind::Read);  // de volta ao chamador
    hw_probe_access(c, 0xA4123456, 4, ProbeKind::Write);  // VRAM: pagina de 64 KB
    hw_probe_access(c, 0xE1234564, 4, ProbeKind::Write);  // SQ: agrega por fila/palavra
    hw_probe_access(c, 0xE3FFFFE4, 4, ProbeKind::Write);  // mesma fila/palavra
    hw_probe_dispatch(c, 0x8C000300);
}
} // namespace

// A sonda atribui cada acesso a funcao do guest em execucao: HH_CALL restaura c->cur_fn.
HH_TEST(hh07_probe_attributes_accesses_through_hh_call) {
    hw_probe_init("hh07_probe_teste.tsv");
    Sh4Context c{};
    c.next_event = ~0ull;  // sem servico de eventos no retorno da chamada
    probeCaller(&c);
    const std::string dump = hw_probe_dump();
    hw_probe_init("");  // desliga (nada e gravado na saida)
    HH_CHECK(!g_hw_probe);
    HH_CHECK(dump.find("w\t0x8C000200\t0xA05F8008\t4\tpvr\t1") != std::string::npos);
    HH_CHECK(dump.find("r\t0x8C000100\t0xFF000038\t4\tccn\t1") != std::string::npos);
    HH_CHECK(dump.find("w\t0x8C000100\t0xA4120000\t4\tvram64\t1") != std::string::npos);
    HH_CHECK(dump.find("i\t0x8C000100\t0x8C000300\t0\t-\t1") != std::string::npos);
    HH_CHECK(dump.find("w\t0x8C000100\t0xE0000024\t4\tstore_queue\t2") != std::string::npos);
    HH_CHECK(c.cur_fn == 0x8C000100);
}

// Trace de ordem: eventos na ordem em que acontecem, com ciclos e argumentos.
HH_TEST(hh07_event_trace_records_order) {
    event_trace_init("hh07_ordem_teste.tsv");
    event_trace(TraceEvent::VBlank, 100, 1);
    event_trace(TraceEvent::Irq, 120, 0x320, 0x8C010000);
    event_trace(TraceEvent::ThreadSwitch, 130, 0, 1);
    const std::string d = event_trace_dump();
    event_trace_init("");
    HH_CHECK(!g_event_trace);
    const size_t v = d.find("0\t100\tV\t00000001\t00000000");
    const size_t i = d.find("1\t120\tI\t00000320\t8C010000");
    const size_t t = d.find("2\t130\tT\t00000000\t00000001");
    HH_CHECK(v != std::string::npos && i != std::string::npos && t != std::string::npos && v < i && i < t);
}

// Revisao Codex 2026-10-15 [ALTA]: funcao do SDK com endereco so em dado, possivel alvo de
// chamada indireta do jogo, nao pode sumir do fechamento: fica como "nao resolvida".
HH_TEST(hh07_closure_reports_data_only_indirect_targets) {
    constexpr uint32_t G = 0x8C010000, T = 0x8C100000;
    Scan               s;
    SdkId              id;
    id.functions[G].layer = Layer::Game;
    id.functions[T].layer = Layer::Sdk;
    s.edges               = {{G, 0, 0x8C010004, EdgeKind::Indirect}};
    s.refs                = {{T, 0x8C100010, 0xA05F8044, 4, Kind::Write, Region::Pvr}};
    s.addressTaken        = {T};
    s.takenBy[T]          = {0};
    const auto r = check_closure(s, id, {});
    HH_CHECK(r.violations.empty() && r.unresolved.size() == 1 && r.unresolved[0] == T);
    HH_CHECK(check_closure(s, id, {T}).unresolved.empty());  // declarada: resolvida
    const auto m = compute_metrics(s, id, {}, r, {});
    HH_CHECK(m.at("fechamento.indireto_nao_resolvido") == 1);
    HH_CHECK(std::find(ratchet_keys().begin(), ratchet_keys().end(), "fechamento.indireto_nao_resolvido") !=
             ratchet_keys().end());
}

// Revisao Codex [MEDIA]: pref de endereco calculado + base das SQ conta como render.
HH_TEST(hh07_unresolved_pref_with_sq_literal_reaches_render) {
    constexpr uint32_t G = 0x8C010000, B = 0x8C100000;
    Scan               s;
    SdkId              id;
    id.functions[G].layer = Layer::Game;
    id.functions[B].layer = Layer::Sdk;
    s.edges               = {{G, B, 0x8C010004, EdgeKind::Call}};
    s.prefUnresolved[B]   = 1;
    HH_CHECK(derive_boundary(s, id).at(B).subsystems.empty());  // so pref: pode ser prefetch
    s.sqLiteral[B] = 1;
    HH_CHECK(derive_boundary(s, id).at(B).subsystems.count(dcmap::Subsystem::Render));
}

// Revisao Codex [MEDIA]: fmov transfere 4 ou 8 bytes (FPSCR.SZ); o acesso e registrado com 8
// e o pos-incremento invalida a base (o proximo acesso por r1 nao e resolvido).
HH_TEST(hh07_scan_fmov_size_depends_on_fpscr_sz) {
    const auto img = image({
        0xD102,          // 8C010000 mov.l 0x8C01000C,r1  (0xA05F8000)
        0xF019,          // 8C010002 fmov.s @r1+,fr0   -> 4 ou 8 bytes
        0x6212,          // 8C010004 mov.l @r1,r2      -> r1 = base+4 ou base+8: desconhecido
        0x000B,          // 8C010006 rts
        0x0009,          // 8C010008 nop
        0x0009,          // 8C01000A
        0x8000, 0xA05F,  // 8C01000C pool
    });
    const auto a = analyze(img);
    const Scan s = scan(img, a);
    HH_CHECK(std::any_of(s.refs.begin(), s.refs.end(), [](const HwRef& r) {
        return r.address == 0xA05F8000 && r.kind == Kind::Read && r.size == 8;
    }));
    HH_CHECK(!hasRef(s, kBase, 0xA05F8004, Kind::Read, Region::Pvr));
    HH_CHECK(!hasRef(s, kBase, 0xA05F8008, Kind::Read, Region::Pvr));
}

// HH07: acesso do jogo ao hardware declarado na fronteira ([[dependencia_hw]]) sai das
// violacoes e conta a parte; sem a declaracao continua violacao (caso negativo); declarar
// uma funcao que nao acessa hardware nao esconde nada.
HH_TEST(hh07_declared_hw_dependency_is_counted_not_hidden) {
    Synthetic  t;
    const std::set<uint32_t> all = {Synthetic::B, Synthetic::X, Synthetic::Y};
    const auto sem = check_closure(t.s, t.id, all);
    HH_CHECK(hasViolation(sem, Violation::HardwareAccess, Synthetic::G3) && sem.declaredHw.empty());
    const auto com = check_closure(t.s, t.id, all, {{Synthetic::G3, {Region::HollyIntc}}});
    HH_CHECK(com.violations.empty());
    HH_CHECK(com.declaredHw.size() == 1 && com.declaredHw[0] == Synthetic::G3 && com.declaredHwRegions == 1);
    // revisao Codex HH07 [ALTA]: a declaracao vale so para as regioes listadas
    const auto regiaoErrada = check_closure(t.s, t.id, all, {{Synthetic::G3, {Region::Pvr}}});
    HH_CHECK(hasViolation(regiaoErrada, Violation::HardwareAccess, Synthetic::G3) && regiaoErrada.declaredHw.empty());
    const auto outra = check_closure(t.s, t.id, all, {{Synthetic::B, {Region::HollyIntc}}});
    HH_CHECK(hasViolation(outra, Violation::HardwareAccess, Synthetic::G3) && outra.declaredHw.empty());

    const auto b  = derive_boundary(t.s, t.id);
    Metrics    m0 = compute_metrics(t.s, t.id, b, com, {});
    HH_CHECK(m0.at("fechamento.dependencias_hw_declaradas") == 1 && m0.at("fechamento.violacoes.hw") == 0);
    HH_CHECK(m0.at("fechamento.dependencias_hw_regioes") == 1);
    Metrics mais = m0;
    mais["fechamento.dependencias_hw_declaradas"] += 1;  // a lista nao pode crescer
    const auto f = ratchet(m0, mais);
    HH_CHECK(f.size() == 1 && f[0].key == "fechamento.dependencias_hw_declaradas");
    Metrics maisRegioes = m0;
    maisRegioes["fechamento.dependencias_hw_regioes"] += 1;  // nem ganhar regioes
    const auto fr = ratchet(m0, maisRegioes);
    HH_CHECK(fr.size() == 1 && fr[0].key == "fechamento.dependencias_hw_regioes");
}

// Revisao Codex HH07 [MEDIA]: so a sonda confirma uma dependencia declarada; a referencia
// estatica de G3 nao basta.
HH_TEST(hh07_declared_hw_dependency_needs_probe) {
    Synthetic        t;
    const DeclaredHw d = {{Synthetic::G3, {Region::HollyIntc}}};
    auto             u = unconfirmed_hw_deps(t.s, d);
    HH_CHECK(u.size() == 1 && u[0].first == Synthetic::G3 && u[0].second == Region::HollyIntc);
    t.s.refs.push_back({Synthetic::G3, 0, 0xA05F6900, 4, Kind::Read, Region::HollyIntc, 7, true});
    HH_CHECK(unconfirmed_hw_deps(t.s, d).empty());
    const DeclaredHw duas = {{Synthetic::G3, {Region::HollyIntc, Region::Pvr}}};
    u = unconfirmed_hw_deps(t.s, duas);  // regiao declarada a mais: obsoleta
    HH_CHECK(u.size() == 1 && u[0].second == Region::Pvr);
}

HH_TEST(hh07_declared_hw_dependency_toml) {
    std::map<uint32_t, HwDependency> deps;
    deps[0x8C100000] = {0x8C100000, "render", "HH14", "sonda: teste", {Region::StoreQueue, Region::Ccn}};
    const auto d = parse_boundary(boundary_toml({}, {}, deps));
    HH_CHECK(d.hwDeps.size() == 1 && d.hwDeps.at(0x8C100000).owner == "HH14");
    HH_CHECK(d.hwDeps.at(0x8C100000).regions == std::set<Region>({Region::StoreQueue, Region::Ccn}));
    HH_CHECK(declared_hw(d).at(0x8C100000).size() == 2);
    HH_CHECK(d.hwDeps.at(0x8C100000).subsystem == "render" && d.hwDeps.at(0x8C100000).evidence == "sonda: teste");
    auto throws = [](const std::string& text) {
        try {
            parse_boundary(text);
        } catch (const std::runtime_error&) {
            return true;
        }
        return false;
    };
    const std::string ok = "funcao = 1\nsubsistema = \"render\"\ndono = \"HH14\"\nregioes = [\"ccn\"]\nevidencia = \"e\"\n";
    HH_CHECK(!throws("[[dependencia_hw]]\n" + ok));
    const std::string semRegioes = "funcao = 1\nsubsistema = \"render\"\ndono = \"HH14\"\nevidencia = \"e\"\n";
    HH_CHECK(throws("[[dependencia_hw]]\n" + semRegioes));
    HH_CHECK(throws("[[dependencia_hw]]\nregioes = []\n" + semRegioes));
    HH_CHECK(throws("[[dependencia_hw]]\nregioes = [\"nada\"]\n" + semRegioes));
    HH_CHECK(throws("[[dependencia_hw]]\nfuncao = 1\nsubsistema = \"render\"\nevidencia = \"e\"\n"));  // sem dono
    HH_CHECK(throws("[[dependencia_hw]]\nfuncao = 1\nsubsistema = \"render\"\ndono = \"HH14\"\n"));    // sem evidencia
    HH_CHECK(throws("[[dependencia_hw]]\nfuncao = 1\nsubsistema = \"render\"\ndono = \"depois\"\nevidencia = \"e\"\n"));
    HH_CHECK(throws("[[dependencia_hw]]\n" + ok + "[[dependencia_hw]]\n" + ok));  // repetida
}

// HH09.0: a catraca tempo.consumidores.ciclos conta so consumo funcional. O poll central do HH
// (cpu/sh4_service.cpp) e o stub de testes do HH04 (cpu/sh4_hw_m3.cpp) reprogramam o quantum:
// progresso e teste. Uma leitura nova de ->cycles num modulo volta a contar (caso negativo).
HH_TEST(hh09_cycle_ratchet_classifier) {
    namespace fs  = std::filesystem;
    const fs::path top  = fs::temp_directory_path() / "hh_hh09_ciclos";
    const fs::path root = top / "runtime";  // transitional/ fica ao lado (varrido tambem)
    std::error_code ec;
    fs::remove_all(top, ec);
    auto put = [&](const char* rel, const std::string& text) {
        fs::create_directories((root / rel).parent_path());
        std::ofstream(root / rel, std::ios::binary) << text;
    };
    const std::string poll = "void sh4_service(Sh4Context* c) {\n    c->next_event = c->cycles + kServiceQuantum;\n}\n";
    put("cpu/sh4_service.cpp", poll);
    put("cpu/sh4_hw_m3.cpp", poll);
    put("aica/aica.cpp", "// le c->cycles so em comentario\nint f() { return 0; }\n");
    Metrics m = runtime_cycle_metrics(root);
    HH_CHECK(m.at("tempo.consumidores.ciclos") == 0 && m.at("tempo.ciclos.funcionais") == 0);
    HH_CHECK(m.at("tempo.ciclos.progresso") == 1 && m.at("tempo.ciclos.teste") == 1);
    put("aica/aica.cpp", "uint64_t agora(Sh4Context* c) { return c->cycles; }\n");
    m = runtime_cycle_metrics(root);
    HH_CHECK(m.at("tempo.consumidores.ciclos") == 1 && m.at("tempo.ciclos.funcionais") == 1);
    // a mesma expressao fora do poll central nao e progresso
    put("aica/aica.cpp", poll);
    m = runtime_cycle_metrics(root);
    HH_CHECK(m.at("tempo.consumidores.ciclos") == 1);
    put("aica/aica.cpp", "int f() { return 0; }\n");
    // transitional/ (revisao Codex HH09): o hardware_wait do adaptador e progresso (lista fechada);
    // outra leitura no adaptador e funcional
    fs::create_directories(top / "transitional");
    const std::string hw = "bool hardware_wait(Sh4Context* c, uint64_t ticks) {\n"
                           "    const uint64_t end = c->cycles + ticks;\n"
                           "    while (c->cycles < end && !c->redirect) {\n"
                           "        c->cycles = std::min(end, std::max(c->cycles, c->next_event));\n"
                           "        if (c->cycles >= c->next_event) sh4_service(c);\n    }\n    return true;\n}\n";
    std::ofstream(top / "transitional" / "time_adapter.cpp", std::ios::binary) << hw;
    m = runtime_cycle_metrics(root);
    HH_CHECK(m.at("tempo.consumidores.ciclos") == 0 && m.at("tempo.ciclos.progresso") == 6);
    std::ofstream(top / "transitional" / "time_adapter.cpp", std::ios::binary)
        << hw << "uint64_t agora(Sh4Context* c) { return c->cycles; }\n";
    m = runtime_cycle_metrics(root);
    HH_CHECK(m.at("tempo.consumidores.ciclos") == 1);
    fs::remove_all(top, ec);
}
