// SPDX-License-Identifier: MIT
// Derivado de recomp-BStinger runtime/hle/bios.cpp (MIT, Copyright (c) 2026 Blue Stinger
// Recompiled contributors; ver third_party/licenses/recomp-BStinger-LICENSE.txt).
// Syscalls do BIOS (convencao documentada no KallistiOS, kernel/arch/dreamcast:
// r4-r7 argumentos, r6 = superfuncao, r7 = funcao; resultado em r0; volta por PR).
// M4: cada chamada e registrada; funcoes ainda nao implementadas sao fatais, para
// que so o que o boot pede seja implementado.
#include "hle/bios.h"

#include "cpu/sh4_mem.h"
#include "cpu/sh4_runtime.h"
#include "gdrom/gdrom.h"
#include "hle/flashrom.h"

#include <cstdio>
#include <cstdlib>

namespace hh {
namespace {

constexpr uint32_t kSysinfo  = 0x8C001000;
constexpr uint32_t kRomfont  = 0x8C001002;
constexpr uint32_t kFlashrom = 0x8C001004;
constexpr uint32_t kGdrom    = 0x8C001006;
constexpr uint32_t kMisc     = 0x8C001008;
constexpr uint32_t kGdrom2   = 0x8C0010F0;

// Diagnostico: HH_HLE_LOG=1 registra cada syscall (no jogo, o GD-ROM e consultado a cada
// quadro: ligado sempre, o log pesava no ritmo do executavel).
void log(const char* what, Sh4Context* c) {
    static const bool on = std::getenv("HH_HLE_LOG") != nullptr;
    if (!on) return;
    std::fprintf(stderr, "[hle] %s r4=%08X r5=%08X r6=%08X r7=%08X pr=%08X\n", what, c->r[4], c->r[5], c->r[6],
                 c->r[7], c->pr);
}

// Volta como um rts: PR precisa ser o retorno que o chamador espera.
void ret(Sh4Context* c, uint32_t entry) {
    if (c->pr != c->ret_expect) sh4_bad_return(c, entry, c->pr);
}

[[noreturn]] void todo(Sh4Context* c, const char* what, uint32_t entry) {
    log(what, c);
    sh4_fatal(c, "syscall do BIOS ainda nao implementada", entry, c->r[7]);
}

// SYSINFO (r7): 0 INIT grava em 0x8C000068 o ID do sistema (flash 0x1A056, 8 bytes) e as
// propriedades (flash 0x1A000, 5 bytes), o resto zerado; 2 ICON (r4 = icone) -> 704 ou -1;
// 3 ID -> ponteiro para 0x8C000068.
constexpr uint32_t kSysinfoData = 0x8C000068;

void sysinfo(Sh4Context* c) {
    log("SYSINFO", c);
    switch (c->r[7]) {
    case 0: {
        const uint8_t* f = flashrom_data();
        for (uint32_t i = 0; i < 24; ++i) {
            const uint8_t b = i < 8 ? f[0x1A056 + i] : i < 13 ? f[0x1A000 + i - 8] : 0;
            sh4_write8(c, kSysinfoData + i, b);
        }
        c->r[0] = 0;
        break;
    }
    case 2:
        c->r[0] = c->r[4] > 9 ? 0xFFFFFFFF : 704;
        break;
    case 3:
        c->r[0] = kSysinfoData;
        break;
    default:
        todo(c, "SYSINFO", kSysinfo);
    }
    ret(c, kSysinfo);
}
void romfont(Sh4Context* c) { todo(c, "ROMFONT", kRomfont); }
void flashrom(Sh4Context* c) {
    log("FLASHROM", c);
    flashrom_syscall(c);
    ret(c, kFlashrom);
}
void gdrom2(Sh4Context* c) { todo(c, "GDROM2", kGdrom2); }
BiosExitHook g_exitHook = nullptr;

[[noreturn]] void exitGame(Sh4Context* c, uint32_t code, uint32_t entry) {
    if (g_exitHook) g_exitHook(c, code);  // normalmente lanca
    sh4_fatal(c, "o jogo pediu para sair (sem gancho de saida no host)", entry, code);
}

// MISC (r4): 1 = voltar ao menu do BIOS (documentacao do KallistiOS); 3 tambem e usado pela
// rotina de saida do Head Hunter (0x8C15E5A8-0x8C15E5B6). As duas encerram o jogo nativo.
// Outros codigos continuam fatais ate serem observados e documentados.
void misc(Sh4Context* c) {
    log("MISC", c);
    if (c->r[4] == 1 || c->r[4] == 3) exitGame(c, c->r[4], kMisc);
    todo(c, "MISC", kMisc);
}

// Bootstrap do IP.BIN em 0x8C008300: a rotina de saida salta para la quando as bandeiras em
// 0x8C0080FC/0x8C0080FE valem 1 (reinicio). Nao ha codigo recompilado do IP.BIN.
void bootstrap(Sh4Context* c) { exitGame(c, kBiosExitBootstrap, kBiosExitBootstrap); }

void gdrom(Sh4Context* c) {
    log("GDROM", c);
    if (c->r[6] != 0) todo(c, "GDROM (superfuncao)", kGdrom);
    gdrom_syscall(c, c->r[7], c->r[4], c->r[5]);
    ret(c, kGdrom);
}

} // namespace

void bios_hle_install(Sh4Context* c, bool writeVectors) {
    const struct {
        uint32_t vec, addr;
        Sh4Func  fn;
    } k[] = {{kBiosVecSysinfo, kSysinfo, sysinfo},    {kBiosVecRomfont, kRomfont, romfont},
             {kBiosVecFlashrom, kFlashrom, flashrom}, {kBiosVecGdrom, kGdrom, gdrom},
             {kBiosVecGdrom2, kGdrom2, gdrom2},       {kBiosVecMisc, kMisc, misc}};
    for (const auto& e : k) {
        sh4_register_hle(e.addr, e.fn);
        if (writeVectors) sh4_write32(c, e.vec, e.addr);
    }
    sh4_register_hle(kBiosExitBootstrap, bootstrap);
}

void bios_set_exit_hook(BiosExitHook fn) { g_exitHook = fn; }

} // namespace hh
