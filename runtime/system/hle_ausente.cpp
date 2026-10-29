// SPDX-License-Identifier: MIT
// HH15.3: o lugar do BIOS HLE, do drive de GD-ROM e da flash simulados no produto (HeadHunter.exe). Compilado como fonte
// do proprio produto (app/CMakeLists.txt): define os simbolos que o boot (boot_prepare), o host e o poll central
// referenciam, entao o linker nao puxa hle/bios, hle/flashrom, gdrom/gdrom nem gdrom/data_source do hh_runtime (a
// catraca hh15_produto_papeis confere no mapa). As syscalls do produto sao do servico nativo (system/syscall_service),
// instalado nos mesmos enderecos logo depois do boot; o caminho que precisaria do drive simulado (--arquivos legado,
// --sistema hle) e recusado antes, na linha de comando. Chegar aqui por ele e erro explicito.
#include "gdrom/data_source.h"
#include "gdrom/gdrom.h"
#include "hle/bios.h"

#include <cstdio>
#include <cstdlib>

namespace hh {
namespace {

[[noreturn]] void absent(const char* what) {
    std::fprintf(stderr, "ERRO: %s: o BIOS HLE e o drive simulado nao existem no produto (so no hh-run, --sistema hle)\n",
                 what);
    std::exit(2);
}

} // namespace

// boot_prepare grava os vetores e registra o HLE; o servico nativo os regrava e registra os mesmos enderecos em seguida
void bios_hle_install(Sh4Context*, bool) {}
void bios_set_exit_hook(BiosExitHook) {}  // o fim do jogo e do servico nativo (syscall_service_set_exit_hook)
void gdrom_reset() {}
void gdrom_poll(Sh4Context*) {}  // o servico nativo termina as leituras na hora (nada a avancar com o tempo)
void gdrom_set_disc(std::unique_ptr<DiscSource> disc) {
    if (disc) absent("gdrom_set_disc");
}
std::unique_ptr<DiscSource> open_runtime_data(const std::filesystem::path&, const std::filesystem::path&) {
    absent("open_runtime_data");
}

} // namespace hh
