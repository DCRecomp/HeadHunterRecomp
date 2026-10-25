// SPDX-License-Identifier: MIT
// Nucleo de execucao do jogo recompilado (HH06), sem janela: boot nativo a partir dos dados
// de execucao (nunca o CDI), GD-ROM HLE, entrada, gancho de VBlank, parada e liberacao das
// fibras. Usado pelo hh-run (headless) e pelo HeadHunter (Preview com janela). Inspirado no
// GameHost do Blue Stinger (app/game_host.cpp, MIT), reduzido ao que o HH06 precisa.
#pragma once
#include <atomic>
#include <cstdint>
#include <filesystem>
#include <functional>
#include <string>
#include <vector>

namespace hh {

struct HostFrame {
    std::vector<uint32_t> argb;  // ARGB8888 do quadro exibido pelo PVR
    uint32_t              width = 0, height = 0;
    uint64_t              vblank = 0;
    uint64_t              reads  = 0;  // leituras do controle (porta 0) ate aqui
};

// HH08: provedor dos arquivos do jogo. Legado: gdFs recompilado sobre o GD-ROM HLE (setores de
// disc_meta). Nativo: adaptador gdFs sobre os arquivos de data\disc (sem setores).
enum class FileProvider { Legacy, Native };
// HH09: provedor de tempo e eventos. Legado: o contador de ciclos decide (runtime/cpu/time_legacy).
// Nativo: relogio logico e filas de eventos (transitional/time_adapter), entrega nos mesmos pontos.
// Experimental: o tempo so anda nas esperas declaradas ([native] esperas).
enum class TimeProvider { Legacy, Native, Experimental };

struct HostOptions {
    std::filesystem::path data;           // dados de execucao (hh-extract --runtime)
    uint64_t              vblankLimit = 0;  // > 0: para sozinho nesse VBlank
    int64_t               rtcUnix     = -1;  // >= 0: relogio fixo (segundos Unix, hora local)
    bool                  realtime    = false;  // segura o jogo em 59,94 VBlanks por segundo
    std::vector<uint32_t> watch;          // diagnostico: entradas nessas funcoes
    FileProvider          files = FileProvider::Native;  // --arquivos legado|nativo (nativo: HH08, P1/P2)
    TimeProvider          time  = TimeProvider::Native;  // --tempo legado|nativo|experimental (HH09: nativo estavel padrao, A1-A8)
    // HH08: mods de recursos (mesma arvore de data\disc; qualquer tamanho), so no provedor
    // nativo. Vazio ou inexistente = sem mods; mod invalido = erro com o caminho e o motivo.
    std::filesystem::path modsDir;
    // HH08: versao esperada do executavel de boot (a do disco 1, hh::kInstallBootVersion),
    // conferida pelo hash; vazio = qualquer versao aceita.
    std::string expectVersion;
    // HH08: lista de arquivos conferida pelo provedor nativo (arquivos.manifest dos discos
    // juntos, ou disc.manifest); vazio = <data>\disc.manifest.
    std::filesystem::path filesManifest;
    // Chamado na thread do jogo a cada VBlank, depois do quadro exibido.
    std::function<void(const HostFrame&)> onFrame;
    const std::atomic<bool>*              stop = nullptr;  // pedido de parada (janela fechada)
};

enum class HostEnd { Limit, Stopped, GameExit, Error };

struct HostResult {
    HostEnd     end = HostEnd::Error;
    std::string error;       // HostEnd::Error
    uint32_t    exitCode = 0;  // HostEnd::GameExit
    uint64_t    vblanks = 0, nonBlank = 0, firstNonBlank = 0;
};

// Executa o jogo ate o limite, a parada pedida, a saida do jogo ou um erro fatal. A entrada
// vem de input_configure_from_env (HH_INPUT_SCRIPT) ou de uma fonte ja instalada pelo chamador
// (input_set_source). Uma execucao por processo.
HostResult host_run(const HostOptions& opt);

} // namespace hh
