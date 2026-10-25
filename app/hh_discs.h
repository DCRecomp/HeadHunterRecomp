// SPDX-License-Identifier: MIT
#pragma once
// HH08: a instalacao do Head Hunter tem os dois discos juntos, sem troca de disco. O Builder
// prepara os dados do disco 1 (boot, disc.manifest, disc_meta) e junta neles os arquivos do
// disco 2 (hh-extract merge_runtime_discs: arquivos.manifest, variantes em disc\_DISCO2). O
// jogo continua do disco 1 para o 2 sozinho: o provedor nativo de arquivos mostra o disco em que
// o jogo esta (transitional/gdfs_adapter.h).
//
// Mensagens seguras: dados ausentes, de outro disco ou sem o disco 2 viram std::runtime_error com
// o caminho e o que fazer; nada de travar nem de ler setor.
#include <filesystem>
#include <string>

namespace hh {

struct InstallData {
    std::filesystem::path dir;       // pasta de dados (disc\, disc.manifest, disc_meta.*)
    std::filesystem::path manifest;  // lista conferida pelo provedor nativo (arquivos ou disc)
    bool                  merged = false;  // com o disco 2 (arquivos.manifest)
};

// Versao do executavel de boot da instalacao (o do disco 1).
constexpr const char* kInstallBootVersion = "hedhunter_v1006_a";

// `dir`: a pasta de dados. Sem o disco 2 (dados de desenvolvimento de um disco so), merged = false.
InstallData resolve_install(const std::filesystem::path& dir);

} // namespace hh
