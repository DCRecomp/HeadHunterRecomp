// SPDX-License-Identifier: MIT
// Derivado de recomp-BStinger transitional/gdfs_adapter.h (MIT, Copyright (c) 2026 Blue Stinger
// Recompiled contributors; ver third_party/licenses/recomp-BStinger-LICENSE.txt).
#pragma once
// HH08: adaptador da API gdFs (Shinobi, gdFs Ver 1.09) sobre native::files::IFileSystem.
//
// Contratos identificados no executavel (docs/planos/HH08-PLANO.md, "HH08.1 — resultados"). O
// GDFS do guest vira um handle de 32 bits fora da RAM (0x7E______): ponteiros do host nunca vao
// para o guest, e um acesso direto do jogo ao "ponteiro" cai no caminho lento (visivel na
// sonda). Os setores de 2 048 bytes sao a unidade da API gdFs (posicao, tamanho, pedidos),
// convertida aqui em deslocamentos de bytes no arquivo; nada de FAD, LBA ou diretorio do disco.
//
// Diferenca para o 1.05 do Blue Stinger: as funcoes por nome (Open, ChangeDir) sao stubs que
// saltam pela tabela do driver (0x8C188D08 -> 0x8C185DAC). O adaptador substitui os stubs que o
// jogo chama; a tabela e o driver continuam legados (inicializacao, estado do drive).
//
// Um GDFS legado (ponteiro na RAM) que chegue a uma funcao migrada vai para a funcao original.
// Um handle nativo que chegue a uma funcao interna sem equivalente nativo (espera, PIO,
// inicializacao do GDFS) e erro fatal com mensagem: nunca leitura silenciosa de memoria.
#include "boundary_registry.h"
#include "platform/data_files.h"
#include "platform/files.h"

#include <cstdint>
#include <string>

namespace hh::transitional::gdfs {

// Enderecos das funcoes gdFs no executavel (config/headhunter.toml, [native] slots).
struct Addresses {
    uint32_t open, changeDir, close, getFileSize, getFileSctSize, getFileFad, seek, tell, eof, setTransMode,
        read, reqPio, reqRd32, wait, getStat, getTransStat, stop, getErr, initGdfs, driveStat;
};

// Head Hunter (hedhunter_v1006_a/b: o executavel e o mesmo nos dois discos fora de um dword).
constexpr Addresses kHeadHunter = {
    0x8C15D09A, 0x8C15C7A0, 0x8C15C7B8, 0x8C15C7F0, 0x8C15C80A, 0x8C15C824, 0x8C15C83E, 0x8C15C88A,
    0x8C15C8A0, 0x8C15C8C8, 0x8C15C8E2, 0x8C15C970, 0x8C15CA48, 0x8C15C6B4, 0x8C15CD68, 0x8C15CD80,
    0x8C15CD96, 0x8C15CEC4, 0x8C15C746, 0x8C15D150,
};

constexpr uint32_t kTokenTag = 0x7E000000;
inline bool        is_native_handle(uint32_t v) { return (v & 0xFF000000u) == kTokenTag && v != kTokenTag; }

// Registra as funcoes no `registry` (subsistema arquivos) e seleciona o provedor nativo.
// Lanca std::runtime_error se alguma funcao com codigo gerado nao tem slot (gerado sem
// [native] slots): sem slot, chamadas diretas iriam ao legado com handles nativos.
void install(BoundaryRegistry& registry, native::files::IFileSystem& fs, const Addresses& a = kHeadHunter);
// Volta ao legado e fecha os arquivos abertos.
void uninstall(BoundaryRegistry& registry);

// O contrato gdFs guarda o tamanho em 32 bits (+12 bytes, +16 setores no GDFS do binario): um
// recurso ou mod maior nao e representavel. Lanca std::runtime_error com o caminho.
void check_sizes(const native::files::DataFileSystem& fs);

struct Stats {
    uint64_t opens = 0, reads = 0, bytes = 0, notFound = 0, legacyPassThrough = 0;
};
Stats stats();

// Troca de disco (HH08.5): abre a tampa logica. O jogo le o estado do drive antes de cada
// abertura de arquivo (0x8C04E754) e, com a tampa aberta, desliga os subsistemas e sai para o
// BIOS (0x8C04E470); o host da o boot do outro disco. Fica aberta ate o proximo install().
void open_lid();
bool lid_open();

} // namespace hh::transitional::gdfs
