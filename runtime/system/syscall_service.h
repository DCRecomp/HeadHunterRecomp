// SPDX-License-Identifier: MIT
#pragma once
// HH15.3 (docs/planos/HH15-PLANO.md; modelo: system/syscall_service do N8.3 do Blue Stinger): servico nativo das
// syscalls do BIOS no contrato medido do Head Hunter (HH_HLE_LOG na rota nativa). Atende os mesmos enderecos dos vetores
// que o BIOS HLE (0x8C001000+), com o mesmo efeito em r0 e na RAM, sem BIOS, drive nem flash simulados:
//   GDROM (r6 = 0): REQ_CMD, GET_CMD_STAT, EXEC_SERVER, INIT_SYSTEM e GET_DRV_STAT; comandos INIT (0x18), GET_VERSION
//     (0x28), GETSCD formato 1 (0x22) e DMAREAD (0x11), que termina inteira no EXEC_SERVER (perfil instantaneo do
//     modelo, o do produto). Os setores vem dos dados da instalacao (RuntimeDisc), com o mesmo registro de leituras;
//   FLASHROM: so a particao 2 (0x1C000, 16 KB), em memoria e volatil (em branco a cada sessao): INFO, READ, WRITE (AND)
//     e DELETE;
//   SYSINFO: INIT (identificacao a partir da area de fabrica, em branco como na flash do modelo) e ID;
//   MISC 1/3 e o bootstrap do IP.BIN: fim do jogo pelo gancho do host.
// Tudo o mais (outra funcao, comando, particao, superfuncao; ROMFONT, GDROM2) e componente removido: erro explicito
// (sh4_fatal), contado em syscall_service_stats().refused, sem recuo silencioso para o modelo.
#include "cpu/sh4_context.h"

#include <cstdint>
#include <filesystem>
#include <string>

namespace hh {

// Instala o servico nos enderecos dos vetores (depois de boot_prepare, que monta a area do sistema) e zera os pedidos
// e a flash. `data`: a pasta de dados da instalacao (data\, com disc.manifest e disc_meta).
void syscall_service_install(Sh4Context* c, const std::filesystem::path& data);
void syscall_service_uninstall();
bool syscall_service_installed();

using SyscallExitHook = void (*)(Sh4Context* c, uint32_t code);
void syscall_service_set_exit_hook(SyscallExitHook fn);  // MISC 1/3 e bootstrap (como bios_set_exit_hook)

struct SyscallServiceStats {
    uint64_t gdrom = 0, flash = 0, sysinfo = 0, misc = 0, sectors = 0;
    uint64_t refused = 0;
};
SyscallServiceStats syscall_service_stats();
std::string         syscall_service_summary();

} // namespace hh
