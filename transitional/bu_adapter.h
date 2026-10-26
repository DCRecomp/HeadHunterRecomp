// SPDX-License-Identifier: MIT
// Derivado de recomp-BStinger transitional/bu_adapter.h (MIT, Copyright (c) 2026 Blue Stinger
// Recompiled contributors; ver third_party/licenses/recomp-BStinger-LICENSE.txt).
#pragma once
// Provedor nativo dos saves (HH11, --saves nativo): a API publica da biblioteca de backup "bu Ver 1.47"
// do Head Hunter passa a ser atendida por bu::Machine sobre um ISaveStore (native/platform/saves.h),
// sem VMU, Maple nem a FAT do cartao. Contrato e oraculo em docs/planos/HH11-PLANO.md.
//
// Slots ([native] slots): as entradas publicas que o jogo chama e o servidor periodico.
//   8C15FB44 init(capacidade, mascara, r6, callback): a maquina habilita as unidades da mascara; o
//            servidor 8C15FC9C e registrado como no original, na tabela de callbacks da camada per
//            (8C16F7CA(*8C186498, servidor)); o callback do 4o argumento (que registra os dois callbacks do
//            jogo) e chamado sem argumentos; flag de init 8C188D20 = 1.
//   8C15FC9C servidor: uma passada da maquina; entrega progresso e conclusoes aos callbacks.
//   8C15FDCC / 8C15FDD2  registro dos callbacks de conclusao / progresso.
//   8C160754 mount, 8C15FF54 disk_info, 8C16037C exists, 8C1603AA file_info, 8C160614 load (op 0x0E),
//   8C160558 save (op 0x0C), 8C160500 op 0x0B, 8C1603DC status, 8C160774 last_error (desvio para 8C15FE66),
//   8C15FC2A exit.
// A rotina pura do tamanho do VMS (8C160990) e o CRC (8C160B30) continuam recompilados.
//
// Some no HH15, quando os chamadores do jogo forem elevados.
#include "boundary_registry.h"
#include "platform/saves.h"

#include <cstdint>
#include <string>

namespace hh::transitional::bu {

constexpr uint32_t kInit = 0x8C15FB44, kServer = 0x8C15FC9C, kRegDone = 0x8C15FDCC, kRegProgress = 0x8C15FDD2,
                   kMount = 0x8C160754, kDiskInfo = 0x8C15FF54, kExists = 0x8C16037C, kFileInfo = 0x8C1603AA,
                   kLoad = 0x8C160614, kSave = 0x8C160558, kOp0B = 0x8C160500, kStatus = 0x8C1603DC,
                   kExit = 0x8C15FC2A, kLastError = 0x8C160774;  // 8C160774: bra 8C15FE66 (last_error, = BS 8C08C112)
// Registro do servidor (os mesmos argumentos do init original, 8C15FB92..8C15FB9C)
constexpr uint32_t kCallbackRegister = 0x8C16F7CA, kCallbackSlotPtr = 0x8C186498, kInitFlag = 0x8C188D20;

struct Stats {
    uint64_t inits = 0, ticks = 0, mounts = 0, exists = 0, fileInfos = 0, saves = 0, loads = 0, statusBusy = 0;
    uint64_t saveOk = 0, saveFail = 0, loadOk = 0, loadFail = 0, unsupported = 0;
    // HH11 (revisao Codex): FNV-1a de 64 bits do que o ultimo LOAD deixou na RAM do jogo, relido do guest depois da
    // escrita (o portao compara com o arquivo do cartao), e o tamanho
    uint64_t lastLoadFnv = 0, lastLoadBytes = 0;
};

// Negativos do portao hh11_saves (HH_BU_MUTACAO=<lista>): cada um tem de ser acusado.
enum Mutation : uint32_t {
    kMutNone      = 0,
    kMutNoTime    = 1u << 0,  // sem_data: ignora a data que o jogo passa ao SAVE (relogio do store)
    kMutLoadFail  = 1u << 1,  // load_falha: op 0x0B devolve "nao encontrado"
    kMutCorrupt   = 1u << 2,  // save_corrompe: um byte da imagem gravada trocado
    kMutLoadAlter = 1u << 3,  // load_altera: um byte do que o LOAD entrega ao jogo trocado (contagens iguais)
};
uint32_t parse_mutations(const std::string& list);  // nomes acima separados por virgula; desconhecido: lanca

// Instala o provedor (exige os slots; sem eles, std::runtime_error). `store` vive ate uninstall.
void        install(BoundaryRegistry& registry, native::saves::ISaveStore& store, uint32_t mutations = kMutNone);
void        uninstall(BoundaryRegistry& registry);
bool        installed();
Stats       stats();
std::string summary();  // linha "[saves] ..." para o log

} // namespace hh::transitional::bu
