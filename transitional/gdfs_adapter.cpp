// SPDX-License-Identifier: MIT
// Derivado de recomp-BStinger transitional/gdfs_adapter.cpp (MIT, Copyright (c) 2026 Blue Stinger
// Recompiled contributors; ver third_party/licenses/recomp-BStinger-LICENSE.txt).
#include "gdfs_adapter.h"

#include "cpu/sh4_runtime.h"
#include "debug/file_log.h"
#include "guest_abi.h"
#include "handle_table.h"
#include "mods/mod_api.h"

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <stdexcept>
#include <string>
#include <vector>

namespace hh::transitional::gdfs {

namespace {

namespace nf = native::files;

constexpr uint32_t kSector = 2048;  // unidade da API gdFs (nao do disco)

// Estado (GDFS do SDK: +80 estado, +82 erro, +24 transferidos, +20 posicao...).
enum Status : int32_t { kIdle = 0, kComplete = 1, kBusy = 2, kError = 6 };
constexpr int32_t kErrNotOpen = -10, kErrAlign = -14, kErrOrigin = -16, kErrRange = -17;
constexpr int32_t kErrRead = -1;  // falha de E/S do host (o legado nao tem equivalente observado)
// Estado do drive devolvido por gdFsGetDrvStat (0x8C15D150): o jogo (0x8C04E754) sai para o
// BIOS com 6 (tampa aberta) ou 7 (sem disco); o legado devolve sempre 1 no roteiro.
constexpr int32_t kDrivePause = 1, kDriveOpen = 6;

struct Gdfs {
    nf::FileHandle file;
    std::string    logical;
    uint64_t       sizeBytes = 0;
    int32_t        sizeSct = 0, pos = 0;
    uint32_t       trans = 0;
    int32_t        status = kIdle, err = 0;
    uint32_t       mode = 0;
};

struct State {
    nf::IFileSystem*  fs = nullptr;
    Addresses         a{};
    HandleTable<Gdfs> table{15};  // gen <= 15: o handle cabe em 24 bits sob a etiqueta 0x7E
    std::string       dir;        // diretorio corrente (nome logico, "" = raiz)
    Stats             stats;
    // Pedido assincrono (gdFsReqRd32): os dados vao para o buffer na propria chamada, como no
    // GD-ROM HLE do legado (transferencia imediata; o P1 mostra os blocos entregues mesmo quando o
    // jogo cancela). O estado fica "ocupado" (2, 0 bytes) ate o jogo observar (GetStat,
    // GetTransStat) ou fazer outro pedido: so entao vira concluido (1). Os getters nao leem nada
    // (revisao Codex HH08): nenhum caminho depende de consulta para receber os dados.
    uint32_t busyToken = 0, busyTrans = 0;
    // Estado logico do drive (gdFsGetDrvStat): o host abre a tampa para trocar de disco.
    int32_t driveStat = kDrivePause;
};

State& st() {
    static State s;
    return s;
}

Gdfs* get(uint32_t token) { return is_native_handle(token) ? st().table.get(token & 0x00FFFFFFu) : nullptr; }

// GDFS legado (ponteiro na RAM): a funcao original trata.
bool passLegacy(Sh4Context* c, uint32_t addr) {
    if (is_native_handle(c->r[4]) || c->r[4] == 0) return false;
    if (!GuestMemory::ram_offset(c->r[4], 4)) return false;
    ++st().stats.legacyPassThrough;
    sh4_lookup_generated(addr)(c);
    return true;
}

std::string joinPath(const std::string& dir, const std::string& name) {
    if (!name.empty() && (name[0] == '\\' || name[0] == '/')) return name;
    return dir.empty() ? name : dir + "/" + name;
}

// Leitura de `nsct` unidades a partir da posicao, para `buf` (RAM do guest). Devolve as
// unidades pedidas (depois do recorte no fim do arquivo) ou 0 no fim do arquivo.
int32_t doRead(Sh4Context* c, Gdfs& g, uint32_t nsct, uint32_t buf) {
    State& s = st();
    if (g.pos >= g.sizeSct || nsct == 0) {
        g.trans = 0, g.status = kComplete;
        return 0;
    }
    nsct = std::min<uint32_t>(nsct, uint32_t(g.sizeSct - g.pos));
    const uint64_t         off = uint64_t(g.pos) * kSector;
    std::vector<std::byte> tmp(size_t(nsct) * kSector, std::byte{0});  // ultimo setor: zeros apos o fim
    const auto             r = s.fs->read(g.file, off, tmp);
    if (!r) {
        std::fprintf(stderr, "[arquivos] falha ao ler %s (%s)\n", g.logical.c_str(), native::error_name(r.error()));
        g.status = kError, g.err = kErrRead, g.trans = 0;
        return int32_t(nsct);
    }
    // Diagnostico (negativo do portao P1): HH_ARQUIVOS_MUTACAO=corromper_bloco troca um byte da
    // 20a leitura; o contrato de leituras tem de acusar.
    static const bool corrupt = [] {
        const char* v = std::getenv("HH_ARQUIVOS_MUTACAO");
        return v && std::string(v) == "corromper_bloco";
    }();
    if (corrupt && s.stats.reads + 1 == 20 && !tmp.empty()) tmp[0] ^= std::byte{0x5A};
    GuestMemory mem(c);
    if (!mem.write(buf, tmp)) sh4_fatal(c, "gdFs nativo: destino da leitura fora da RAM principal", c->pc, buf);
    if (g_file_log)
        for (uint64_t k = 0; k < r.value(); k += kSector)
            file_log_chunk("nativo", g.logical, off + k, reinterpret_cast<const uint8_t*>(tmp.data()) + k,
                           size_t(std::min<uint64_t>(kSector, r.value() - k)));
    g.pos += int32_t(nsct);
    g.trans  = nsct * kSector;
    g.status = kComplete;
    ++s.stats.reads;
    s.stats.bytes += r.value();
    return int32_t(nsct);
}

// Publica a conclusao do pedido assincrono ja transferido (observacao do estado, novo pedido).
void publishBusy() {
    State& s = st();
    if (!s.busyToken) return;
    if (Gdfs* g = get(s.busyToken); g && g->status == kBusy) g->status = kComplete, g->trans = s.busyTrans;
    s.busyToken = 0;
}

// Stop/Close: os dados ja estao no buffer (como no legado); o pedido deixa de existir.
void dropBusy(uint32_t token) {
    if (st().busyToken == token) st().busyToken = 0;
}

// --- funcoes da API (Sh4Func) ---

// gdFsOpen (stub 0x8C15D09A -> tabela[60]): (nome, dirrec) -> GDFS ou 0.
void fOpen(Sh4Context* c) {
    State&     s = st();
    GuestCall  call(c);
    const auto name = call.memory().cstring(c->r[4], 255);
    if (!name) return call.ret(0);
    if (c->r[5] != 0) {
        // Registro de diretorio carregado pelo legado (0x8C15D042 com dirrec, visto 1x em
        // 0x8C02B6A0, "\Streams"): a funcao original abre e devolve um GDFS legado, que as demais
        // funcoes reconhecem pelo endereco na RAM (revisao Codex HH08). Nenhuma abertura assim
        // aparece no roteiro newgame_area1.
        ++s.stats.legacyPassThrough;
        return sh4_lookup_generated(s.a.open)(c);
    }
    const std::string logical = joinPath(s.dir, *name);
    const auto        h       = s.fs->open(logical);
    if (!h && (h.error() == native::Error::Corrupt || h.error() == native::Error::Io)) {
        // arquivo do disco alterado ou truncado (HH08.6) ou ilegivel no Windows (permissao,
        // bloqueio): parar com mensagem, nunca seguir como se o arquivo nao existisse
        const std::string msg = std::string(h.error() == native::Error::Corrupt
                                                ? "arquivo do jogo alterado ou corrompido: "
                                                : "arquivo do jogo ilegivel (permissao ou bloqueio do Windows): ") +
                                nf::canonical_path(logical) + ". Refaca a instalacao com o Builder.";
        sh4_fatal(c, msg.c_str(), c->pc, c->r[4]);
    }
    if (!h) {
        ++s.stats.notFound;
        std::fprintf(stderr, "[arquivos] nao encontrado: %s\n", logical.c_str());
        return call.ret(0);
    }
    Gdfs g;
    g.file      = h.value();
    g.logical   = nf::canonical_path(logical);
    g.sizeBytes = s.fs->size(g.file).value();
    if (g.sizeBytes > 0xFFFFFFFFull) {  // o GDFS do jogo guarda o tamanho em 32 bits
        std::fprintf(stderr, "[arquivos] %s: tamanho alem do limite do jogo (4 GB - 1)\n", logical.c_str());
        s.fs->close(h.value());
        return call.ret(0);
    }
    g.sizeSct        = int32_t((g.sizeBytes + kSector - 1) / kSector);
    const uint32_t t = s.table.insert(std::move(g));
    if (!t) {
        s.fs->close(h.value());
        return call.ret(0);
    }
    ++s.stats.opens;
    call.ret(kTokenTag | t);
}

// gdFsChangeDir (0x8C15C7A0 -> 0x8C15D042(nome, 0)): (nome) -> 0, ou -1 sem o diretorio.
void fChangeDir(Sh4Context* c) {
    State&     s = st();
    GuestCall  call(c);
    const auto name = call.memory().cstring(c->r[4], 255);
    if (!name) return call.ret(uint32_t(-1));
    std::string next = joinPath(s.dir, *name);
    std::string norm;  // ".." sobe um nivel
    for (size_t i = 0; i <= next.size();) {
        const size_t      j    = std::min(next.find_first_of("\\/", i), next.size());
        const std::string part = next.substr(i, j - i);
        if (part == "..") norm = norm.substr(0, norm.find_last_of('/') == std::string::npos ? 0 : norm.find_last_of('/'));
        else if (!part.empty() && part != ".") norm += (norm.empty() ? "" : "/") + part;
        i = j + 1;
    }
    if (!norm.empty() && !s.fs->list(norm)) {
        std::fprintf(stderr, "[arquivos] diretorio nao encontrado: %s\n", norm.c_str());
        return call.ret(uint32_t(-1));
    }
    s.dir = norm;
    call.ret(0);
}

void fClose(Sh4Context* c) {
    if (passLegacy(c, st().a.close)) return;
    if (!is_native_handle(c->r[4])) return;
    const uint32_t h = c->r[4] & 0x00FFFFFFu;
    dropBusy(c->r[4]);
    if (Gdfs* g = st().table.get(h)) {
        st().fs->close(g->file);
        st().table.remove(h);
    }
}

void fGetFileSize(Sh4Context* c) {
    if (passLegacy(c, st().a.getFileSize)) return;
    GuestCall call(c);
    Gdfs*     g = get(c->r[4]);
    if (!g) return call.ret(0);
    call.memory().store<uint32_t>(c->r[5], uint32_t(g->sizeBytes));
    call.ret(1);
}

void fGetFileSctSize(Sh4Context* c) {
    if (passLegacy(c, st().a.getFileSctSize)) return;
    GuestCall call(c);
    Gdfs*     g = get(c->r[4]);
    if (!g) return call.ret(0);
    call.memory().store<uint32_t>(c->r[5], uint32_t(g->sizeSct));
    call.ret(1);
}

// gdFsSeek: no binario, -10 sem arquivo e 0 nos demais caminhos observados; os codigos de origem
// e faixa sao os do 1.05 (mesmo corpo em 0x8C15C83E).
void fSeek(Sh4Context* c) {
    if (passLegacy(c, st().a.seek)) return;
    GuestCall call(c);
    Gdfs*     g = get(c->r[4]);
    if (!g) return call.ret(uint32_t(kErrNotOpen));
    const int32_t off = int32_t(c->r[5]);
    int32_t       np;
    switch (c->r[6]) {
    case 0: np = off; break;
    case 1: np = g->pos + off; break;
    case 2: np = g->sizeSct + off; break;
    default: return call.ret(uint32_t(kErrOrigin));
    }
    if (np < 0 || np > g->sizeSct) return call.ret(uint32_t(kErrRange));
    g->pos = np;
    call.ret(0);
}

void fTell(Sh4Context* c) {
    if (passLegacy(c, st().a.tell)) return;
    Gdfs* g = get(c->r[4]);
    GuestCall(c).ret(g ? uint32_t(g->pos) : 0);
}

void fEof(Sh4Context* c) {
    if (passLegacy(c, st().a.eof)) return;
    GuestCall call(c);
    Gdfs*     g = get(c->r[4]);
    if (!g) return call.ret(uint32_t(kErrNotOpen));
    call.memory().store<uint32_t>(c->r[5], g->pos >= g->sizeSct ? 1u : 0u);
    call.ret(0);
}

void fSetTransMode(Sh4Context* c) {
    if (passLegacy(c, st().a.setTransMode)) return;
    GuestCall call(c);
    Gdfs*     g = get(c->r[4]);
    if (!g) return call.ret(uint32_t(kErrNotOpen));
    g->mode = c->r[5] & 0xFFFF;
    call.ret(0);
}

// gdFsRead: sincrona; buffer de qualquer alinhamento (o legado usa PIO se nao alinhado).
void fRead(Sh4Context* c) {
    if (passLegacy(c, st().a.read)) return;
    GuestCall call(c);
    Gdfs*     g = get(c->r[4]);
    if (!g) return call.ret(uint32_t(kErrNotOpen));
    const uint32_t nsct = c->r[5], buf = c->r[6];
    publishBusy();  // o drive ja terminou o pedido anterior
    doRead(c, *g, nsct, buf);
    call.ret(g->status == kError ? uint32_t(g->err) : 0u);
}

// gdFsReqRd32: -10 sem arquivo; -14 com buffer desalinhado; no fim do arquivo, concluido com 0
// unidades; senao recorta no fim, transfere (como o legado) e publica "ocupado" (2) com 0 bytes
// ate a proxima observacao; devolve as unidades.
void fReqRd32(Sh4Context* c) {
    if (passLegacy(c, st().a.reqRd32)) return;
    State&    s = st();
    GuestCall call(c);
    Gdfs*     g = get(c->r[4]);
    if (!g) return call.ret(uint32_t(kErrNotOpen));
    if (c->r[6] & 31) return call.ret(uint32_t(kErrAlign));
    publishBusy();  // o drive ja terminou o pedido anterior
    if (g->pos >= g->sizeSct || c->r[5] == 0) {
        g->trans = 0, g->status = kComplete;
        return call.ret(0);
    }
    const int32_t n = doRead(c, *g, c->r[5], c->r[6]);
    if (g->status != kError) {
        s.busyToken = c->r[4], s.busyTrans = g->trans;
        g->status = kBusy, g->trans = 0;
    }
    call.ret(uint32_t(n));
}

void fGetStat(Sh4Context* c) {
    if (passLegacy(c, st().a.getStat)) return;
    Gdfs* g = get(c->r[4]);
    if (g && st().busyToken == c->r[4]) publishBusy();  // observado: o pedido terminou
    GuestCall(c).ret(g ? uint32_t(g->status) : uint32_t(kErrNotOpen));
}

void fGetTransStat(Sh4Context* c) {
    if (passLegacy(c, st().a.getTransStat)) return;
    Gdfs* g = get(c->r[4]);
    if (g && st().busyToken == c->r[4]) publishBusy();
    GuestCall(c).ret(g ? g->trans : 0u);
}

// gdFsStop: o pedido deste arquivo deixa de existir (os dados ja foram entregues, como no legado)
// e o arquivo volta a ocioso.
void fStop(Sh4Context* c) {
    if (passLegacy(c, st().a.stop)) return;
    GuestCall call(c);
    Gdfs*     g = get(c->r[4]);
    if (!g) return call.ret(uint32_t(kErrNotOpen));
    dropBusy(c->r[4]);
    g->status = kIdle;
    call.ret(0);
}

void fGetErr(Sh4Context* c) {
    if (passLegacy(c, st().a.getErr)) return;
    Gdfs* g = get(c->r[4]);
    GuestCall(c).ret(g ? uint32_t(g->err) : 0u);
}

// gdFsGetDrvStat (stub 0x8C15D150 -> tabela[84]): () -> estado do drive. Nativo: o estado logico
// do host (pausa, ou tampa aberta para a troca de disco); nada do drive do legado.
void fDriveStat(Sh4Context* c) { GuestCall(c).ret(uint32_t(st().driveStat)); }

// Funcoes internas sem equivalente nativo: um handle nativo aqui e erro de integracao, nao dado.
template <int Which>
void fGuard(Sh4Context* c) {
    static const char* kNames[] = {"gdFsGetFileFad (endereco do disco)", "leitura por PIO (interna)",
                                   "espera de pedido (interna)", "inicializacao do GDFS (interna)"};
    const Addresses& a    = st().a;
    const uint32_t   addr = Which == 0 ? a.getFileFad : Which == 1 ? a.reqPio : Which == 2 ? a.wait : a.initGdfs;
    if (is_native_handle(c->r[4])) {
        const std::string msg =
            std::string("gdFs nativo: ") + kNames[Which] + " recebeu um arquivo aberto pelo provedor nativo";
        sh4_fatal(c, msg.c_str(), c->pc, c->r[4]);
    }
    sh4_lookup_generated(addr)(c);
}

std::vector<std::pair<uint32_t, Sh4Func>> functions(const Addresses& a) {
    return {
        {a.open, &fOpen},
        {a.changeDir, &fChangeDir},
        {a.close, &fClose},
        {a.getFileSize, &fGetFileSize},
        {a.getFileSctSize, &fGetFileSctSize},
        {a.getFileFad, &fGuard<0>},
        {a.seek, &fSeek},
        {a.tell, &fTell},
        {a.eof, &fEof},
        {a.setTransMode, &fSetTransMode},
        {a.read, &fRead},
        {a.reqPio, &fGuard<1>},
        {a.reqRd32, &fReqRd32},
        {a.wait, &fGuard<2>},
        {a.getStat, &fGetStat},
        {a.getTransStat, &fGetTransStat},
        {a.stop, &fStop},
        {a.getErr, &fGetErr},
        {a.initGdfs, &fGuard<3>},
        {a.driveStat, &fDriveStat},
    };
}

} // namespace

void install(BoundaryRegistry& registry, nf::IFileSystem& fs, const Addresses& a) {
    State&     s   = st();
    const auto fns = functions(a);
    // Endereco sem codigo gerado (funcao nunca alcancada pela analise) nao pode ser chamado:
    // fica de fora. Codigo gerado sem slot e erro: chamadas diretas iriam ao legado.
    for (const auto& [addr, fn] : fns)
        if (sh4_lookup_generated(addr) && !mods::has_slot(addr)) {
            char hexa[16];
            std::snprintf(hexa, sizeof hexa, "%08X", addr);
            throw std::runtime_error(std::string("provedor nativo de arquivos: a funcao gdFs ") + hexa +
                                     " nao tem slot (gere o codigo de novo com [native] slots)");
        }
    s.fs = &fs;
    s.a  = a;
    s.dir.clear();
    s.table.clear();
    s.busyToken = 0;
    s.stats        = {};
    s.driveStat    = kDrivePause;
    for (const auto& [addr, fn] : fns) {
        if (!sh4_lookup_generated(addr)) continue;
        registry.add(addr, dcmap::Subsystem::Files, "gdFs");
        registry.set_native(addr, fn);
        if (registry.select(addr, Provider::Native) != SelectResult::Ok)
            throw std::runtime_error("provedor nativo de arquivos: troca de provedor falhou");
    }
}

void uninstall(BoundaryRegistry& registry) {
    State& s = st();
    for (const auto& [addr, fn] : functions(s.a))
        if (addr) registry.select(addr, Provider::Legacy);
    if (s.fs) s.table.for_each([&](Gdfs& g) { s.fs->close(g.file); });  // o que ficou aberto
    s.table.clear();
    s.busyToken = 0;
    s.fs           = nullptr;
}

void check_sizes(const nf::DataFileSystem& fs) {
    for (const auto& m : fs.mods())
        if (m.modSize > 0xFFFFFFFFull)
            throw std::runtime_error("mod de recurso grande demais para o jogo (limite de 4 GB - 1 byte): " + m.logical);
}

Stats stats() { return st().stats; }

void open_lid() { st().driveStat = kDriveOpen; }
bool lid_open() { return st().driveStat == kDriveOpen; }

} // namespace hh::transitional::gdfs
