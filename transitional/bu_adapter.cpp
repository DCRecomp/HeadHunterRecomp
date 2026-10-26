// SPDX-License-Identifier: MIT
// Derivado de recomp-BStinger transitional/bu_adapter.cpp (MIT, Copyright (c) 2026 Blue Stinger
// Recompiled contributors; ver third_party/licenses/recomp-BStinger-LICENSE.txt).
// HH11: init/exit do bu 1.47 do Head Hunter (servidor na tabela de callbacks da camada per, callback do init
// sem argumentos), file_info e op 0x0B. Contrato em docs/planos/HH11-PLANO.md.
#include "bu_adapter.h"

#include "bu_machine.h"
#include "guest_abi.h"
#include "mods/mod_api.h"

#include <array>
#include <cstdio>
#include <memory>
#include <stdexcept>
#include <string>
#include <vector>

namespace hh::transitional::bu {

namespace {

namespace ns = native::saves;

struct State {
    ns::ISaveStore*          store = nullptr;
    std::unique_ptr<Machine> m;
    uint32_t                 cbDone = 0, cbProgress = 0;
    uint32_t                 loadDst[kDrives] = {};
    uint32_t                 mutations = kMutNone;
    Stats                    stats;
};

State& st() {
    static State s;
    return s;
}

// Chamada do guest a partir do nativo, convencao normal (r4-r7), retorno conferido pelo runtime.
uint32_t callGuest(Sh4Context* c, uint32_t func, uint32_t a0 = 0, uint32_t a1 = 0, uint32_t a2 = 0, uint32_t a3 = 0) {
    if (!func) return 0;
    const uint32_t savedPr = c->pr, savedRet = c->ret_expect, savedSp = c->ret_sp, savedFn = c->cur_fn;
    const uint32_t savedR4 = c->r[4], savedR5 = c->r[5], savedR6 = c->r[6], savedR7 = c->r[7];
    constexpr uint32_t kBack = 0xFFFFFFF0u;  // retorno sentinela (so comparado)
    c->pr = c->ret_expect = kBack;
    c->ret_sp            = c->r[15];
    c->r[4] = a0, c->r[5] = a1, c->r[6] = a2, c->r[7] = a3;
    sh4_dispatch(c, 0, func);
    const uint32_t r0 = c->r[0];
    c->pr = savedPr, c->ret_expect = savedRet, c->ret_sp = savedSp, c->cur_fn = savedFn;
    c->r[4] = savedR4, c->r[5] = savedR5, c->r[6] = savedR6, c->r[7] = savedR7;
    return r0;
}

int drive(Sh4Context* c) { return int(int32_t(c->r[4])); }

std::string name(GuestCall& call, uint32_t ptr) {
    const auto n = call.memory().cstring(ptr, 12);
    return n && ns::valid_save_name(*n) ? *n : std::string();
}

void registerServer(Sh4Context* c, uint32_t fn) {
    const auto slot = GuestMemory(c).load<uint32_t>(kCallbackSlotPtr);
    if (!slot) sh4_fatal(c, "bu nativo: indice do callback da camada per fora da RAM", c->pc, kCallbackSlotPtr);
    callGuest(c, kCallbackRegister, *slot, fn);
}

void fInit(Sh4Context* c) {
    State&         s    = st();
    const uint32_t mask = c->r[5], cb = c->r[7];
    s.m = std::make_unique<Machine>(*s.store);
    s.m->init(mask);
    s.cbDone = s.cbProgress = 0;
    ++s.stats.inits;
    registerServer(c, kServer);
    if (cb) callGuest(c, cb);  // o original so chama com a inicializacao interna em 0; sem argumentos
    GuestMemory(c).store<uint32_t>(kInitFlag, 1);
    GuestCall(c).ret(0);
}

void fRegDone(Sh4Context* c) { st().cbDone = c->r[4], GuestCall(c).ret(0); }
void fRegProgress(Sh4Context* c) { st().cbProgress = c->r[4], GuestCall(c).ret(0); }

void fServer(Sh4Context* c) {
    State& s = st();
    if (!s.m) return GuestCall(c).ret(0);
    ++s.stats.ticks;
    for (Event& e : s.m->tick(true)) {  // ocupada durante os progressos
        if (e.kind == Event::Kind::Progress) {
            callGuest(c, s.cbProgress, uint32_t(e.drive), e.op, e.a, e.b);
            continue;
        }
        if (e.op == kOpLoad || e.op == kOpLoadFile) {
            if (e.a == uint32_t(kOk) && (s.mutations & kMutLoadAlter) && e.data.size() > 0x100) e.data[0x100] ^= std::byte{0x5A};
            if (e.a == uint32_t(kOk) && !GuestMemory(c).write(s.loadDst[e.drive], e.data))
                sh4_fatal(c, "bu nativo: destino do LOAD fora da RAM principal", c->pc, s.loadDst[e.drive]);
            if (e.a == uint32_t(kOk)) {  // o que o jogo recebeu, relido do guest
                std::vector<std::byte> back(e.data.size());
                uint64_t               h = 1469598103934665603ull;
                if (GuestMemory(c).read(s.loadDst[e.drive], back))
                    for (std::byte b : back) h = (h ^ uint64_t(b)) * 1099511628211ull;
                s.stats.lastLoadFnv = h, s.stats.lastLoadBytes = back.size();
            }
            ++(e.a == uint32_t(kOk) ? s.stats.loadOk : s.stats.loadFail);
        }
        if (e.op == kOpSave) ++(e.a == uint32_t(kOk) ? s.stats.saveOk : s.stats.saveFail);
        if ((e.op == kOpSave || e.op == kOpLoad || e.op == kOpLoadFile) && e.a != uint32_t(kOk))
            std::fprintf(stderr, "[saves] %s na unidade %d falhou: codigo %08X\n", e.op == kOpSave ? "SAVE" : "LOAD", e.drive,
                         e.a);
        s.m->release(e.drive);  // livre antes da conclusao: dentro dela o jogo pode consultar disk_info
        callGuest(c, s.cbDone, uint32_t(e.drive), e.op, e.a);
    }
    GuestCall(c).ret(0);
}

void fMount(Sh4Context* c) {
    ++st().stats.mounts;
    GuestCall(c).ret(uint32_t(st().m ? st().m->mount(drive(c)) : kNoCard));
}

void fDiskInfo(Sh4Context* c) {
    GuestCall call(c);
    std::array<std::byte, kDiskInfoSize> buf{};
    const int32_t r = st().m ? st().m->disk_info(drive(c), buf) : kNoCard;
    if (r == kOk && !call.memory().write(c->r[5], buf)) return call.ret(uint32_t(kBusy));
    call.ret(uint32_t(r));
}

void fExists(Sh4Context* c) {
    GuestCall call(c);
    ++st().stats.exists;
    const std::string n = name(call, c->r[5]);
    call.ret(uint32_t(!st().m ? kNoCard : n.empty() ? kNotFound : st().m->exists(drive(c), n)));
}

// file_info(drive, nome, info*): os 18 bytes do oraculo c3 (bu_machine.h), na hora
void fFileInfo(Sh4Context* c) {
    State&    s = st();
    GuestCall call(c);
    ++s.stats.fileInfos;
    const std::string                    n = name(call, c->r[5]);
    std::array<std::byte, kFileInfoSize> buf{};
    const int32_t r = !s.m ? kNoCard : n.empty() ? kNotFound : s.m->file_info(drive(c), n, buf);
    if (r == kOk && !call.memory().write(c->r[6], buf)) return call.ret(uint32_t(kBusy));
    call.ret(uint32_t(r));
}
// op 0x0B (drive, nome, destino, 0): o arquivo inteiro no destino (oraculo do Flycast, c2: H_HUNTER.SYS em
// 8C2F43B0, progresso 1..6 de 6, conclusao 0); o destino e escrito na conclusao, como no LOAD
void fOp0B(Sh4Context* c) {
    State&    s = st();
    GuestCall call(c);
    ++s.stats.loads;
    const std::string n = name(call, c->r[5]);
    if (!s.m || n.empty()) return call.ret(uint32_t(kBusy));
    const int32_t r = s.m->load(drive(c), (s.mutations & kMutLoadFail) ? std::string("NAOEXISTE.XX") : n, 0, 0, kOpLoadFile);
    if (r == kOk) s.loadDst[drive(c)] = c->r[6];
    call.ret(uint32_t(r));
}

// load(drive, nome, destino, bloco_inicial, nblocos): nblocos 0 = ate o fim
void fLoad(Sh4Context* c) {
    State&    s = st();
    GuestCall call(c);
    ++s.stats.loads;
    const std::string n  = name(call, c->r[5]);
    const auto        nb = call.arg(4);
    if (!s.m || n.empty() || !nb) return call.ret(uint32_t(kBusy));
    const int32_t r = s.m->load(drive(c), n, c->r[7], *nb);
    if (r == kOk) s.loadDst[drive(c)] = c->r[6];
    call.ret(uint32_t(r));
}

// save(drive, nome, origem, blocos, ...): a imagem e copiada agora (o jogo pode liberar o buffer) e gravada no
// disco na conclusao
void fSave(Sh4Context* c) {
    State&    s = st();
    GuestCall call(c);
    ++s.stats.saves;
    const std::string n      = name(call, c->r[5]);
    const uint32_t    blocks = c->r[7];
    if (!s.m || n.empty() || blocks == 0 || blocks > ns::kCardBlocks) return call.ret(uint32_t(kBusy));
    std::vector<std::byte> img(size_t(blocks) * ns::kBlock);
    if (!call.memory().read(c->r[6], img)) return call.ret(uint32_t(kBusy));
    if (std::string why; !ns::vms_check(img, &why))  // o store recusa: registra o motivo (a conclusao sai com erro)
        std::fprintf(stderr, "[saves] SAVE %s (%u blocos): imagem VMS recusada: %s\n", n.c_str(), blocks, why.c_str());
    // data do arquivo: a que o jogo passa (1o argumento de pilha, BuTime do relogio do console; o jogo passa a flag
    // 0x80000000 no 2o, oraculo c1). Sem ela, o relogio do store.
    std::optional<int64_t> when;
    if (const auto tp = call.arg(4); tp && *tp && !(s.mutations & kMutNoTime)) {
        uint8_t t[8];
        if (call.memory().read(*tp, std::as_writable_bytes(std::span(t)))) when = bu_time_to_unix(t);
    }
    if (s.mutations & kMutCorrupt) img[0x100] ^= std::byte{0x5A};  // dentro do trecho coberto pelo CRC
    call.ret(uint32_t(s.m->save(drive(c), n, std::move(img), when)));
}

void fLastError(Sh4Context* c) { GuestCall(c).ret(uint32_t(st().m ? st().m->last_error(drive(c)) : kBusy)); }

void fStatus(Sh4Context* c) {
    const int32_t r = st().m ? st().m->status(drive(c)) : kBusy;
    if (r == kBusy) ++st().stats.statusBusy;
    GuestCall(c).ret(uint32_t(r));
}

// exit: -1 com alguma unidade ocupada (o original consulta as 8); senao desfaz o init (flag e callback da per)
void fExit(Sh4Context* c) {
    State&        s = st();
    const int32_t r = s.m ? s.m->exit() : kOk;
    if (r == kOk) {
        s.m.reset();
        GuestMemory(c).store<uint32_t>(kInitFlag, 0);
        registerServer(c, 0);
    }
    GuestCall(c).ret(uint32_t(r));
}

const std::pair<uint32_t, Sh4Func> kFns[] = {
    {kInit, &fInit},         {kServer, &fServer},       {kRegDone, &fRegDone}, {kRegProgress, &fRegProgress},
    {kMount, &fMount},       {kDiskInfo, &fDiskInfo},   {kExists, &fExists},   {kFileInfo, &fFileInfo},
    {kLoad, &fLoad},         {kSave, &fSave},           {kOp0B, &fOp0B},       {kStatus, &fStatus},
    {kExit, &fExit},         {kLastError, &fLastError},
};

} // namespace

uint32_t parse_mutations(const std::string& list) {
    uint32_t m = kMutNone;
    size_t   i = 0;
    while (i < list.size()) {
        const size_t      j = list.find(',', i);
        const std::string n = list.substr(i, j == std::string::npos ? std::string::npos : j - i);
        if (n == "sem_data") m |= kMutNoTime;
        else if (n == "load_falha") m |= kMutLoadFail;
        else if (n == "save_corrompe") m |= kMutCorrupt;
        else if (n == "load_altera") m |= kMutLoadAlter;
        else if (!n.empty()) throw std::runtime_error("HH_BU_MUTACAO: mutacao desconhecida: " + n);
        if (j == std::string::npos) break;
        i = j + 1;
    }
    return m;
}

void install(BoundaryRegistry& registry, ns::ISaveStore& store, uint32_t mutations) {
    for (const auto& [addr, fn] : kFns)
        if (!sh4_lookup_generated(addr) || !mods::has_slot(addr)) {
            char hexa[16];
            std::snprintf(hexa, sizeof hexa, "%08X", addr);
            throw std::runtime_error(std::string("provedor nativo de saves: a funcao bu ") + hexa +
                                     " nao tem slot (gere o codigo de novo com [native] slots)");
        }
    State& s = st();
    s           = State{};
    s.store     = &store;
    s.mutations = mutations;
    for (const auto& [addr, fn] : kFns) {
        registry.add(addr, dcmap::Subsystem::Saves, "bu");
        registry.set_native(addr, fn);
        if (registry.select(addr, Provider::Native) != SelectResult::Ok)
            throw std::runtime_error("provedor nativo de saves: troca de provedor falhou");
    }
}

void uninstall(BoundaryRegistry& registry) {
    for (const auto& [addr, fn] : kFns) registry.select(addr, Provider::Legacy);
    State& s = st();
    s.m.reset();
    s.store = nullptr;
}

bool installed() { return st().store != nullptr; }

Stats stats() { return st().stats; }

std::string summary() {
    const Stats& s = st().stats;
    char         buf[320];
    std::snprintf(buf, sizeof buf,
                  "[saves] nativo: init %llu, passadas %llu, mount %llu, exists %llu, file_info %llu, SAVE %llu (ok %llu, "
                  "falha %llu), LOAD %llu (ok %llu, falha %llu), status ocupado %llu, sem suporte %llu\n",
                  (unsigned long long)s.inits, (unsigned long long)s.ticks, (unsigned long long)s.mounts,
                  (unsigned long long)s.exists, (unsigned long long)s.fileInfos, (unsigned long long)s.saves,
                  (unsigned long long)s.saveOk, (unsigned long long)s.saveFail, (unsigned long long)s.loads,
                  (unsigned long long)s.loadOk, (unsigned long long)s.loadFail, (unsigned long long)s.statusBusy,
                  (unsigned long long)s.unsupported);
    return buf;
}

} // namespace hh::transitional::bu
