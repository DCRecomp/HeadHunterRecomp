// SPDX-License-Identifier: MIT
// HH15.3: servico nativo das syscalls do BIOS (ver syscall_service.h). O efeito em r0 e na RAM e o do BIOS HLE
// (hle/bios.cpp, hle/flashrom.cpp, gdrom/gdrom.cpp) no perfil instantaneo, so no contrato medido; a equivalencia passo
// a passo e conferida em tests/unit/test_hh15_sistema.cpp e nas referencias (hh15_referencias.py, --sistema nativo).
#include "system/syscall_service.h"

#include "cpu/sh4_mem.h"
#include "cpu/sh4_runtime.h"
#include "debug/file_log.h"
#include "hh_extract/runtime_data.hpp"

#include <algorithm>
#include <array>
#include <cctype>
#include <cstdio>
#include <cstring>
#include <memory>
#include <vector>

namespace hh {
namespace {

// Os mesmos enderecos do BIOS HLE (e do reios): os vetores da area do sistema apontam para eles.
constexpr uint32_t kVecSysinfo = 0x8C0000B0, kVecRomfont = 0x8C0000B4, kVecFlashrom = 0x8C0000B8;
constexpr uint32_t kVecGdrom = 0x8C0000BC, kVecGdrom2 = 0x8C0000C0, kVecMisc = 0x8C0000E0;
constexpr uint32_t kSysinfo = 0x8C001000, kRomfont = 0x8C001002, kFlashrom = 0x8C001004, kGdrom = 0x8C001006;
constexpr uint32_t kMisc = 0x8C001008, kGdrom2 = 0x8C0010F0, kBootstrap = 0x8C008300;
constexpr uint32_t kSysinfoData = 0x8C000068;

constexpr uint32_t kFadOffset = 150;  // FAD = LBA + 150
constexpr uint32_t kFlashP2 = 0x1C000, kFlashP2Size = 16u << 10;

// GDROM: funcoes (r7) e comandos (documentacao do KallistiOS; comportamento do modelo gdrom.cpp)
enum : uint32_t { kReqCmd = 0, kGetCmdStat = 1, kExecServer = 2, kInitSystem = 3, kGetDrvStat = 4 };
enum : int32_t { kOk = 0, kBusy = 1, kComplete = 2, kErr = -1 };
enum : uint32_t { kCmdDmaRead = 0x11, kCmdInit = 0x18, kCmdGetScd = 0x22, kCmdGetVersion = 0x28 };
constexpr uint32_t kStatPause = 1, kDiscGdrom = 0x80;

struct Track {
    uint32_t number = 0, startFad = 0, length = 0, ctrl = 0;
};

struct State {
    bool                               on = false;
    std::unique_ptr<disc::RuntimeDisc> disc;
    std::vector<Track>                 tracks;
    // GDROM: um pedido por vez (o mesmo estado do modelo)
    int32_t  status = kOk;
    uint32_t lastId = 0xFFFFFFFF, nextId = 1, command = 0;
    uint32_t params[4] = {}, result[4] = {};
    uint32_t headFad = 0;  // FAD logo depois do ultimo setor lido (canal Q do GETSCD)
    std::array<uint8_t, kFlashP2Size> flash{};  // particao 2, volatil
    SyscallExitHook     exitHook = nullptr;
    SyscallServiceStats stats;
};
State g;

[[noreturn]] void refuse(Sh4Context* c, const char* what, uint32_t entry, uint32_t value) {
    ++g.stats.refused;
    char b[200];
    std::snprintf(b, sizeof b, "syscall fora do contrato do servico nativo (componente removido do produto, HH15.3): %s",
                  what);
    sh4_fatal(c, b, entry, value);
}

void ret(Sh4Context* c, uint32_t entry) {
    if (c->pr != c->ret_expect) sh4_bad_return(c, entry, c->pr);
}

void resetGdrom() {
    g.status = kOk, g.lastId = 0xFFFFFFFF, g.nextId = 1, g.command = 0, g.headFad = 0;
    std::memset(g.params, 0, sizeof g.params), std::memset(g.result, 0, sizeof g.result);
}

// ---- SYSINFO ----
void sysinfo(Sh4Context* c) {
    ++g.stats.sysinfo;
    switch (c->r[7]) {
    case 0:  // INIT: ID (fabrica 0x1A056, 8 bytes) e propriedades (0x1A000, 5 bytes); a area de fabrica e branca (0xFF)
        for (uint32_t i = 0; i < 24; ++i) sh4_write8(c, kSysinfoData + i, i < 13 ? 0xFF : 0);
        c->r[0] = 0;
        break;
    case 3:  // ID
        c->r[0] = kSysinfoData;
        break;
    default:
        refuse(c, "SYSINFO", kSysinfo, c->r[7]);
    }
    ret(c, kSysinfo);
}

// ---- FLASHROM (so a particao 2) ----
bool inP2(uint32_t off, uint32_t n) { return off >= kFlashP2 && n <= kFlashP2Size && off - kFlashP2 <= kFlashP2Size - n; }

void flashrom(Sh4Context* c) {
    ++g.stats.flash;
    const uint32_t r4 = c->r[4], r5 = c->r[5], r6 = c->r[6];
    switch (c->r[7]) {
    case 0:  // INFO
        if (r4 != 2) refuse(c, "FLASHROM INFO de outra particao", kFlashrom, r4);
        sh4_write32(c, r5, kFlashP2);
        sh4_write32(c, r5 + 4, kFlashP2Size);
        c->r[0] = 0;
        break;
    case 1:  // READ
        if (!inP2(r4, r6)) refuse(c, "FLASHROM READ fora da particao 2", kFlashrom, r4);
        for (uint32_t i = 0; i < r6; ++i) sh4_write8(c, r5 + i, g.flash[r4 - kFlashP2 + i]);
        c->r[0] = 0;
        break;
    case 2:  // WRITE (a flash so apaga bits)
        if (!inP2(r4, r6)) refuse(c, "FLASHROM WRITE fora da particao 2", kFlashrom, r4);
        for (uint32_t i = 0; i < r6; ++i) g.flash[r4 - kFlashP2 + i] &= sh4_read8(c, r5 + i);
        c->r[0] = r6;
        break;
    case 3:  // DELETE
        if (r4 != kFlashP2) refuse(c, "FLASHROM DELETE de outra particao", kFlashrom, r4);
        g.flash.fill(0xFF);
        c->r[0] = 0;
        break;
    default:
        refuse(c, "FLASHROM", kFlashrom, c->r[7]);
    }
    ret(c, kFlashrom);
}

// ---- GDROM ----
bool readSector(uint32_t fad, uint8_t* out) {
    if (fad < kFadOffset || !g.disc || !g.disc->readSector(fad - kFadOffset, out)) return false;
    ++g.stats.sectors;
    if (g_file_log) {  // o mesmo registro do modelo (origem gdrom), para o comparador das referencias nao mudar
        std::string path;
        uint64_t    off = 0, size = 0;
        if (g.disc->locate(fad - kFadOffset, path, off, size) && off < size) {
            std::transform(path.begin(), path.end(), path.begin(), [](unsigned char ch) { return char(std::toupper(ch)); });
            file_log_chunk("gdrom", path, off, out, size_t(std::min<uint64_t>(2048, size - off)));
        } else {
            file_log_other("gdrom", "setor", fad - kFadOffset);
        }
    }
    return true;
}

void execute(Sh4Context* c) {
    switch (g.command) {
    case kCmdInit:
        break;
    case kCmdGetVersion: {
        static const char kVer[] = "GDC Version 1.10 1999-03-31 ";
        for (size_t i = 0; i + 1 < sizeof(kVer); ++i)
            sh4_write8(c, g.params[0] + uint32_t(i), i + 2 == sizeof(kVer) ? 0x02 : uint8_t(kVer[i]));
        break;
    }
    case kCmdGetScd: {
        if (g.params[0] != 1 || g.params[1] != 14) refuse(c, "GDROM GETSCD em formato ou tamanho fora do contrato", kGdrom, g.params[0]);
        if (!g.headFad) refuse(c, "GDROM GETSCD antes de qualquer leitura", kGdrom, 0);
        const Track* t = nullptr;
        for (const Track& tr : g.tracks)
            if (g.headFad >= tr.startFad && g.headFad <= tr.startFad + tr.length) t = &tr;
        if (!t) refuse(c, "GDROM GETSCD com a cabeca fora das faixas", kGdrom, g.headFad);
        const uint32_t rel = g.headFad - t->startFad, abs = g.headFad;
        const uint8_t  buf[14] = {0x00, 0x15, 0x00, 0x0E, uint8_t((t->ctrl << 4) | 1), uint8_t(t->number), 0x01,
                                  uint8_t(rel >> 16), uint8_t(rel >> 8), uint8_t(rel), 0x00,
                                  uint8_t(abs >> 16), uint8_t(abs >> 8), uint8_t(abs)};
        for (uint32_t i = 0; i < 14; ++i) sh4_write8(c, g.params[2] + i, buf[i]);
        g.result[2] = 14;
        break;
    }
    case kCmdDmaRead: {
        const uint32_t fad = g.params[0] & 0xFFFFFF, n = g.params[1];
        uint32_t       dest = g.params[2];
        uint8_t        buf[2048];
        for (uint32_t k = 0; k < n; ++k, dest += 2048) {
            if (!readSector(fad + k, buf)) {
                // os dados foram conferidos na abertura: setor ilegivel e instalacao incompleta, nunca setor em branco
                char b[160];
                std::snprintf(b, sizeof b, "GDROM: setor %u ilegivel nos dados da instalacao (refaca a instalacao)",
                              fad + k - kFadOffset);
                sh4_fatal(c, b, kGdrom, fad + k);
            }
            for (uint32_t i = 0; i < 2048; i += 4) {
                uint32_t w;
                std::memcpy(&w, buf + i, 4);
                sh4_write32(c, dest + i, w);
            }
        }
        if (n) g.headFad = fad + n;
        g.result[2] = n * 2048;
        break;
    }
    default:
        refuse(c, "GDROM comando", kGdrom, g.command);
    }
    g.status = kComplete;
}

void gdrom(Sh4Context* c) {
    ++g.stats.gdrom;
    if (c->r[6] != 0) refuse(c, "GDROM superfuncao", kGdrom, c->r[6]);
    uint32_t&      r0 = c->r[0];
    const uint32_t r4 = c->r[4], r5 = c->r[5];
    switch (c->r[7]) {
    case kReqCmd:
        if (g.status != kOk) {
            r0 = 0;
            break;
        }
        for (int i = 0; i < 4; ++i) g.params[i] = r5 ? sh4_read32(c, r5 + 4 * i) : 0;
        std::memset(g.result, 0, sizeof g.result);
        if (g.nextId == 0 || g.nextId == 0xFFFFFFFF) g.nextId = 1;
        g.lastId  = g.nextId++;
        g.status  = kBusy;
        g.command = r4;
        r0        = g.lastId;
        break;
    case kGetCmdStat:
        for (int i = 0; i < 4; ++i) sh4_write32(c, r5 + 4 * i, g.result[i]);
        if (g.status == kOk || g.status == kBusy) r0 = uint32_t(g.status);
        else if (r4 != g.lastId) r0 = kOk;
        else {
            r0       = uint32_t(g.status);
            g.status = kOk;
            g.lastId = 0xFFFFFFFF;
        }
        break;
    case kExecServer:
        if (g.status == kBusy) execute(c);
        break;
    case kInitSystem:
        resetGdrom();
        break;
    case kGetDrvStat:
        sh4_write32(c, r4, kStatPause);
        sh4_write32(c, r4 + 4, kDiscGdrom);
        r0 = kOk;
        break;
    default:
        refuse(c, "GDROM funcao", kGdrom, c->r[7]);
    }
    ret(c, kGdrom);
}

// ---- fim do jogo ----
[[noreturn]] void exitGame(Sh4Context* c, uint32_t code, uint32_t entry) {
    ++g.stats.misc;
    if (g.exitHook) g.exitHook(c, code);  // normalmente lanca
    sh4_fatal(c, "o jogo pediu para sair (sem gancho de saida no host)", entry, code);
}
void misc(Sh4Context* c) {
    if (c->r[4] == 1 || c->r[4] == 3) exitGame(c, c->r[4], kMisc);
    refuse(c, "MISC", kMisc, c->r[4]);
}
void bootstrap(Sh4Context* c) { exitGame(c, kBootstrap, kBootstrap); }
void romfont(Sh4Context* c) { refuse(c, "ROMFONT", kRomfont, c->r[7]); }
void gdrom2(Sh4Context* c) { refuse(c, "GDROM2", kGdrom2, c->r[7]); }

} // namespace

void syscall_service_install(Sh4Context* c, const std::filesystem::path& data) {
    const SyscallExitHook hook = g.exitHook;
    g          = State{};
    g.exitHook = hook;
    if (!data.empty()) {  // sem dados (testes): sem setores nem faixas; a leitura e o GETSCD sao erro explicito
        g.disc = std::make_unique<disc::RuntimeDisc>(data);
        for (const auto& t : g.disc->manifest().tracks)
            g.tracks.push_back({t.number, t.start_lba + kFadOffset, t.length, t.data ? 4u : 0u});
    }
    g.flash.fill(0xFF);
    const struct {
        uint32_t vec, addr;
        Sh4Func  fn;
    } k[] = {{kVecSysinfo, kSysinfo, sysinfo},    {kVecRomfont, kRomfont, romfont}, {kVecFlashrom, kFlashrom, flashrom},
             {kVecGdrom, kGdrom, gdrom},          {kVecGdrom2, kGdrom2, gdrom2},    {kVecMisc, kMisc, misc}};
    for (const auto& e : k) {
        sh4_register_hle(e.addr, e.fn);  // substitui o registro do BIOS HLE feito no boot (mesmos enderecos)
        sh4_write32(c, e.vec, e.addr);
    }
    sh4_register_hle(kBootstrap, bootstrap);
    g.on = true;
}

void syscall_service_uninstall() {
    const SyscallExitHook hook = g.exitHook;
    g                          = State{};
    g.exitHook                 = hook;
}
bool syscall_service_installed() { return g.on; }
void syscall_service_set_exit_hook(SyscallExitHook fn) { g.exitHook = fn; }
SyscallServiceStats syscall_service_stats() { return g.stats; }

std::string syscall_service_summary() {
    if (!g.on) return {};
    char b[240];
    std::snprintf(b, sizeof b, "[sistema] nativo: GDROM %llu (setores %llu), FLASHROM %llu, SYSINFO %llu, fim %llu; recusadas %llu\n",
                  (unsigned long long)g.stats.gdrom, (unsigned long long)g.stats.sectors, (unsigned long long)g.stats.flash,
                  (unsigned long long)g.stats.sysinfo, (unsigned long long)g.stats.misc, (unsigned long long)g.stats.refused);
    return b;
}

} // namespace hh
