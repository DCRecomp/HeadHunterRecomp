// SPDX-License-Identifier: MIT
// Derivado de recomp-BStinger transitional/pad_adapter.cpp (MIT, Copyright (c) 2026 Blue Stinger
// Recompiled contributors; ver third_party/licenses/recomp-BStinger-LICENSE.txt).
#include "pad_adapter.h"

#include "guest_abi.h"
#include "input_bridge.h"
#include "pad_contract.h"

#include "cpu/sh4_runtime.h"
#include "hle/bios.h"
#include "maple/maple.h"
#include "mods/mod_api.h"

#include <array>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <map>
#include <mutex>
#include <sstream>
#include <stdexcept>

namespace hh::transitional::pad {
namespace ni = native::input;
namespace {

// O que a camada per teria entregue ao servidor para a porta A (modelo da presenca, ver o .h).
enum class Request : uint8_t { None, Identify, Read };
enum class Got : uint8_t { Nothing, Identified, Data };

struct State {
    ni::InputSystem*   sys       = nullptr;
    uint32_t           mutations = kNatNone;
    Sh4Func            trigger   = nullptr;  // provedor anterior do disparo (gerado ou mod)
    // modelo da porta A
    bool               known = false, present = false;
    uint32_t           serversSinceTrigger = 0;
    Request            req   = Request::None;
    Got                got   = Got::Nothing;
    ni::ControllerState pending, collected;
    uint32_t           nativeOld = 0;  // on do servidor anterior (so para o negativo old_host)
    // info sintetizada na area nativa do BIOS HLE
    uint8_t            info[kInfoBytes] = {};
    bool               infoWritten      = false;
    Stats              stats;
};

State& st() {
    static State s;
    return s;
}

uint32_t bswap(uint32_t v) { return (v >> 24) | ((v >> 8) & 0xFF00) | ((v << 8) & 0xFF0000) | (v << 24); }

uint32_t rd32(GuestMemory& m, Sh4Context* c, uint32_t a) {
    const auto v = m.load<uint32_t>(a);
    if (!v) sh4_fatal(c, "pd nativo: leitura fora da RAM", c->pc, a);
    return *v;
}

uint8_t rd8(GuestMemory& m, Sh4Context* c, uint32_t a) {
    const auto v = m.load<uint8_t>(a);
    if (!v) sh4_fatal(c, "pd nativo: leitura fora da RAM", c->pc, a);
    return *v;
}

// 8C15DA94: ((v >> 10) & 0x3FFF00) | (v & 0x20000 ? 0xF0 : 0) | (v & 0x10000 ? 0x0F : 0)
uint32_t convertSupport(uint32_t v) {
    uint32_t r = (v >> 10) & 0x3FFF00u;
    if (v & 0x20000u) r |= 0xF0u;
    if (v & 0x10000u) r |= 0x0Fu;
    return r;
}

// Coleta: o resultado do pedido deste disparo passa a valer, no proprio disparo. Regra medida no HH
// (o Blue Stinger coletava depois do servidor ou no disparo seguinte): ver "Coleta" no .h. Nao depende
// do modo de tempo.
void collect(State& s) {
    switch (s.req) {
    case Request::Read:
        if (s.present) s.got = Got::Data, s.collected = s.pending;
        else {
            s.got = Got::Nothing;
            if (s.known) ++s.stats.lost;
            s.known = false;
        }
        break;
    case Request::Identify:
        if (s.present) s.got = Got::Identified, s.known = true, ++s.stats.identified;
        else s.got = Got::Nothing;
        break;
    case Request::None: s.got = Got::Nothing; break;
    }
}

// Pedido deste disparo e a amostra (o instante do GET_CONDITION legado).
void request(State& s) {
    if (s.mutations & kNatPresenceNow) s.known = true;
    const ni::SampleContext ctx{s.stats.reads + ((s.mutations & kNatSampleEarly) ? 1u : 0u)};
    if (s.known) {
        s.req                   = Request::Read;
        const ni::ControllerState v = s.sys->latch(ctx);
        ++s.stats.latches;
        s.present = v.connected;
        if (s.present) s.pending = v, ++s.stats.reads;
    } else {
        s.req     = Request::Identify;
        s.present = s.sys->connected(ctx);
        ++s.stats.presence;
    }
}

void writeInfo(State& s, Sh4Context* c, GuestMemory& m);

// O original roda primeiro (a camada per sonda as portas; nenhum controle responde). Ele devolve < 0
// sem disparar nada (estado da camada != 1: -3; transferencia em curso: -1; trava tas.b: -2): ai nao ha
// amostra, como no legado, que amostrava na escrita de MDST. Entre essa escrita e o retorno nao roda
// codigo do jogo, entao amostrar aqui e o mesmo instante.
void fTrigger(Sh4Context* c) {
    State& s = st();
    ++s.stats.triggers;
    s.trigger(c);
    if (int32_t(c->r[0]) < 0) return;
    ++s.stats.dmaTriggers;
    s.serversSinceTrigger = 0;
    request(s);
    collect(s);
    GuestMemory m(c);
    writeInfo(s, c, m);
}

// per_info(i) (8C16F14A -> 8C17F470): buffer de info da camada per para a unidade i.
uint32_t perInfo(Sh4Context* c, GuestMemory& m, int i) {
    const uint32_t ptr   = rd32(m, c, kPerInfoPtrLiteral);
    const auto     off16 = m.load<uint16_t>(kPerInfoOffLiteral);
    if (!off16) sh4_fatal(c, "pd nativo: literal de per_info fora da RAM", c->pc, kPerInfoOffLiteral);
    return rd32(m, c, ptr) + uint32_t(int32_t(int16_t(*off16))) + kPerInfoStride * uint32_t(i);
}

// Info da porta A na area nativa do BIOS HLE. No legado o PDS aponta sempre para o mesmo buffer da
// camada per e o CONTEUDO muda na coleta: info do controle quando ele foi identificado, "(none)" quando
// a porta fica vazia (o jogo le info+0 e ve a desconexao antes do servidor). Aqui o ponteiro tambem e
// fixo e o conteudo segue o modelo: controle identificado -> info sintetizada (DEVICE_INFO legado);
// senao -> copia do que a propria camada per escreve para a porta A vazia. Conferida antes de cada
// escrita (outro codigo escrevendo la = erro) e atualizada a cada coleta e a cada passada do servidor.
void writeInfo(State& s, Sh4Context* c, GuestMemory& m) {
    uint8_t cur[kInfoBytes];
    if (!m.read(kHleNativePadInfo, std::as_writable_bytes(std::span(cur))))
        sh4_fatal(c, "pd nativo: area nativa do BIOS HLE fora da RAM", c->pc, kHleNativePadInfo);
    if (!s.infoWritten) {
        // primeira escrita: a faixa tem de estar como o boot a deixou (0xFF, sem dono)
        for (uint8_t b : cur)
            if (b != 0xFF) {
                ++s.stats.guardFaults;
                sh4_fatal(c, "pd nativo: a area nativa do BIOS HLE (info do controle) ja estava ocupada", c->pc,
                          kHleNativePadInfo);
            }
        s.infoWritten = true;
    } else if (std::memcmp(cur, s.info, kInfoBytes) != 0) {
        ++s.stats.guardFaults;
        sh4_fatal(c, "pd nativo: outro codigo escreveu na area nativa do BIOS HLE (info do controle)", c->pc,
                  kHleNativePadInfo);
    }
    if (s.known) controller_info(s.info, kControllerFunction, (s.mutations & kNatInfoNoConnect) ? kMutInfoNoConnect : kMutNone);
    else if (!m.read(perInfo(c, m, 0), std::as_writable_bytes(std::span(s.info))))
        sh4_fatal(c, "pd nativo: buffer de info da camada per fora da RAM", c->pc, perInfo(c, m, 0));
    m.write(kHleNativePadInfo, std::as_bytes(std::span(s.info)));
}

// Servidor pd (8C15DB3A) pelo contrato do HH10.1, sobre o modelo da porta A.
void fServer(Sh4Context* c) {
    State&      s = st();
    GuestMemory m(c);
    ++s.stats.servers;
    // Diagnostico: passadas do servidor alem da primeira entre dois disparos. No BS (coleta depois do
    // servidor) isso divergiria do legado; no HH a coleta e no disparo, entao a segunda passada ve a mesma
    // amostra, como o servidor original relendo o mesmo buffer da camada per (medido: 27 na rota
    // newgame_area1, com o traco do PDS identico ao legado nesses passos; portao C1).
    if (s.stats.dmaTriggers && ++s.serversSinceTrigger > 1) ++s.stats.extraServers;
    writeInfo(s, c, m);
    const uint32_t mode    = rd32(m, c, kModeAddr);
    const uint32_t perBase = perInfo(c, m, 0);
    const auto     cfgOff  = m.load<uint16_t>(kPerCfg5OffLiteral);
    if (!cfgOff) sh4_fatal(c, "pd nativo: literal da configuracao per fora da RAM", c->pc, kPerCfg5OffLiteral);
    const uint32_t perCfg5 = rd32(m, c, rd32(m, c, kPerCfgPtrLiteral) + uint32_t(int32_t(int16_t(*cfgOff))));
    for (int i = 0; i < kUnits; ++i) {
        const uint32_t at = kTable + kUnitSize * uint32_t(i);
        uint8_t        raw[kUnitSize];
        if (!m.read(at, std::as_writable_bytes(std::span(raw)))) sh4_fatal(c, "pd nativo: tabela fora da RAM", c->pc, at);
        const Pds before = pds_load(raw);
        Unit      u;
        u.info     = i == 0 ? kHleNativePadInfo : perBase + kPerInfoStride * uint32_t(i);
        u.response = 0xFF;  // sem resposta
        if (i != 0) {
            // as outras 23 unidades sao da camada per (sondagem; VMU ate o N4): a descricao bruta dela,
            // como o servidor original le por 8C16F138
            const uint32_t rawAt = rd32(m, c, kPerRawLiteral) + kPerRawStride * uint32_t(i);
            uint8_t        rp[12];
            if (!m.read(rawAt, std::as_writable_bytes(std::span(rp)))) sh4_fatal(c, "pd nativo: descricao bruta fora da RAM", c->pc, rawAt);
            u.error    = rp[2];
            u.response = rp[4];
            uint32_t frame;
            std::memcpy(&frame, rp + 8, 4);
            if (const auto off = GuestMemory::ram_offset(frame + 8, 8)) std::memcpy(u.cond, c->ram + *off, 8);
        } else if (s.got != Got::Nothing) {
            if (s.got == Got::Data) {
                const PadState p = input_bridge::to_pad(s.collected);
                const uint16_t b = uint16_t(~p.buttons);  // ativo em 0, como no barramento
                const uint8_t  cond[8] = {uint8_t(b), uint8_t(b >> 8), p.rtrig, p.ltrig, p.joyx, p.joyy, p.joy2x, p.joy2y};
                std::memcpy(u.cond, cond, 8);
                u.response = 8;  // dados
            } else u.response = 5;  // so a identificacao (DEVICE_INFO): o servidor ainda ve "sem dispositivo"
        }
        u.infoFlags = rd32(m, c, u.info);
        // support (8C15DAB6): dados da funcao do controle em big-endian (8C16F44E) e, com a
        // configuracao 5 da camada per em 1, convertidos (8C15DA94)
        u.support = bswap(rd32(m, c, u.info + 4));
        if (perCfg5 == 1) u.support = convertSupport(u.support);
        // gatilhos: limiares e estado de histerese da unidade na RAM do SDK (8C15D900)
        Triggers t{rd8(m, c, kThrHighAddr), rd8(m, c, kThrLowAddr), rd8(m, c, kLatchLAddr + uint32_t(i)),
                   rd8(m, c, kLatchRAddr + uint32_t(i))};
        const Triggers t0 = t;
        Pds after = server_unit(i, before, u, mode, t, (i == 0 && (s.mutations & kNatOldHost)) ? kMutOldNative : kMutNone,
                                s.nativeOld);
        if (t.latchL != t0.latchL) m.store<uint8_t>(kLatchLAddr + uint32_t(i), t.latchL);
        if (t.latchR != t0.latchR) m.store<uint8_t>(kLatchRAddr + uint32_t(i), t.latchR);
        if (i == 0) {
            if (s.mutations & kNatPressOn) after.press = after.on;
            if (s.mutations & kNatReleaseZero) after.release = 0;
            s.nativeOld = after.on;
        }
        pds_store(raw, after);
        m.write(at, std::as_bytes(std::span(raw)));
        const uint32_t code = status_code(u.response);
        m.write(kStatusTable + 4 * uint32_t(i), std::as_bytes(std::span(&code, 1)));
    }
}

// pdGetPeripheral (8C15DC40): mov #52,r0; mul.l r0,r4; r0 = macl + 8C311DAC (sem pilha).
void fGetter(Sh4Context* c) {
    ++st().stats.getters;
    c->macl = c->r[4] * 52u;
    c->r[0] = kTable + c->macl;
}

bool g_installed = false;

} // namespace

uint32_t parse_mutations(const std::string& list) {
    static const std::map<std::string, uint32_t> kNames = {
        {"old_host", kNatOldHost},           {"press_on", kNatPressOn},         {"release_zero", kNatReleaseZero},
        {"amostra_adiantada", kNatSampleEarly}, {"info_sem_conectado", kNatInfoNoConnect},
        {"presenca_imediata", kNatPresenceNow}, {"maple", kNatMaple}};
    uint32_t           m = 0;
    std::istringstream in(list);
    std::string        name;
    while (std::getline(in, name, ',')) {
        if (name.empty()) continue;
        const auto it = kNames.find(name);
        if (it == kNames.end()) throw std::runtime_error("HH_PAD_NATIVO_MUTACAO: mutacao desconhecida: " + name);
        m |= it->second;
    }
    return m;
}

void install(BoundaryRegistry& registry, ni::InputSystem& sys, uint32_t mutations) {
    for (uint32_t a : {kServer, kGetter, kDmaTrigger}) {
        if (!sh4_lookup_generated(a)) throw std::runtime_error("controles nativos: funcao do SDK sem codigo gerado");
        if (!mods::has_slot(a)) {
            char hexa[16];
            std::snprintf(hexa, sizeof hexa, "%08X", a);
            throw std::runtime_error(std::string("controles nativos: a funcao ") + hexa +
                                     " nao tem slot (gere o codigo de novo com [native] slots)");
        }
    }
    State& s    = st();
    s           = {};
    s.sys       = &sys;
    s.mutations = mutations;
    const std::pair<uint32_t, Sh4Func> fns[] = {{kDmaTrigger, &fTrigger}, {kServer, &fServer}, {kGetter, &fGetter}};
    for (const auto& [a, fn] : fns) {
        registry.add(a, dcmap::Subsystem::Input, "pd");
        registry.set_native(a, fn);
        if (registry.select(a, Provider::Native) != SelectResult::Ok)
            throw std::runtime_error("controles nativos: troca de provedor falhou");
    }
    s.trigger = registry.entries().at(sh4_phys(kDmaTrigger)).previous;
    if (!s.trigger) s.trigger = sh4_lookup_generated(kDmaTrigger);
    g_installed = true;
}

void uninstall(BoundaryRegistry& registry) {
    if (!g_installed) return;
    for (uint32_t a : {kServer, kGetter, kDmaTrigger}) registry.select(a, Provider::Legacy);
    st().sys    = nullptr;
    g_installed = false;
}

namespace test {
void begin(ni::InputSystem& sys, uint32_t mutations) {
    st()           = {};
    st().sys       = &sys;
    st().mutations = mutations;
}
void dma_fired(Sh4Context* c) {
    State& s = st();
    ++s.stats.triggers, ++s.stats.dmaTriggers;
    request(s);
    collect(s);
    GuestMemory m(c);
    writeInfo(s, c, m);
}
void server(Sh4Context* c) { fServer(c); }
void getter(Sh4Context* c) { fGetter(c); }
void end() { st() = {}; }
} // namespace test

bool     installed() { return g_installed; }
Stats    stats() { return st().stats; }
uint64_t reads() { return st().stats.reads; }

std::string summary() {
    const Stats& t = st().stats;
    char         b[320];
    std::snprintf(b, sizeof b,
                  "[pd nativo] disparos %llu (efetivos %llu), servidor %llu, getter %llu, amostras %llu, presenca %llu, leituras %llu, "
                  "identificado %llu, perdido %llu, guarda %llu, servidores extras %llu\n",
                  (unsigned long long)t.triggers, (unsigned long long)t.dmaTriggers, (unsigned long long)t.servers,
                  (unsigned long long)t.getters,
                  (unsigned long long)t.latches, (unsigned long long)t.presence, (unsigned long long)t.reads,
                  (unsigned long long)t.identified, (unsigned long long)t.lost, (unsigned long long)t.guardFaults,
                  (unsigned long long)t.extraServers);
    return b;
}

// --- prova dinamica (HH_PAD_FECHAMENTO) ---
namespace {

struct Closure {
    std::string path;
    bool        done = false;
    // funcoes do SDK pd e da camada per: entradas no codigo gerado
    // funcoes do SDK pd 1.50 e da camada per: entradas no codigo gerado
    std::array<std::pair<uint32_t, uint64_t>, 12> fns = {{{0x8C15DB3A, 0}, {0x8C15D900, 0}, {0x8C15DAB6, 0}, {0x8C15DB06, 0},
                                                          {0x8C15D8C2, 0}, {0x8C15DC40, 0}, {0x8C16F138, 0}, {0x8C16F14A, 0},
                                                          {0x8C17F470, 0}, {0x8C17F0AA, 0}, {0x8C16EF18, 0},
                                                          {0x8C15D744, 0}}};  // 8C15D744 init do pd (original)
};
Closure& cl() {
    static Closure c;
    return c;
}
void onEnter(Sh4Context*, uint32_t entry) {
    for (auto& [a, n] : cl().fns)
        if (a == entry) ++n;
}

} // namespace

void closure_install() {
    const char* v = std::getenv("HH_PAD_FECHAMENTO");
    if (!v || !*v) return;
    if (g_sh4_trace) throw std::runtime_error("HH_PAD_FECHAMENTO: outro observador de entradas ja esta ligado");
    cl().path = v;
    sh4_set_trace(&onEnter);
}

void closure_finish(bool nativeMode) {
    Closure& k = cl();
    if (k.path.empty() || k.done) return;
    k.done = true;
    sh4_set_trace(nullptr);
    FILE* f = std::fopen(k.path.c_str(), "w");
    if (!f) return;
    std::fprintf(f, "modo\t%s\n", nativeMode ? "nativo" : "legado");
    for (const auto& [a, n] : k.fns) std::fprintf(f, "f_%08X\t%llu\n", a, (unsigned long long)n);
    const MapleCounters m = maple_counters();
    std::fprintf(f, "maple_dmas\t%llu\nmaple_respostas_controle\t%llu\nmaple_leituras_fonte\t%llu\n",
                 (unsigned long long)m.dmas, (unsigned long long)m.controllerResponses, (unsigned long long)m.sourceReads);
    for (int p = 0; p < 4; ++p)
        std::fprintf(f, "maple_porta%c_devinfo\t%llu\nmaple_porta%c_getcond\t%llu\nmaple_porta%c_outros\t%llu\n", 'A' + p,
                     (unsigned long long)m.devInfo[p], 'A' + p, (unsigned long long)m.getCond[p], 'A' + p,
                     (unsigned long long)m.other[p]);
    const Stats& t = st().stats;
    std::fprintf(f, "pd_disparos\t%llu\npd_disparos_efetivos\t%llu\npd_servidor\t%llu\npd_getter\t%llu\npd_amostras\t%llu\npd_leituras\t%llu\npd_guarda\t%llu\npd_servidores_extras\t%llu\n",
                 (unsigned long long)t.triggers, (unsigned long long)t.dmaTriggers, (unsigned long long)t.servers,
                 (unsigned long long)t.getters, (unsigned long long)t.latches, (unsigned long long)t.reads,
                 (unsigned long long)t.guardFaults, (unsigned long long)t.extraServers);
    std::fclose(f);
}

} // namespace hh::transitional::pad
