// SPDX-License-Identifier: MIT
#pragma once
// HH08.5: os dois discos do Head Hunter numa instalacao (data\disc1, data\disc2, preparados pelo
// Builder a partir do CDI de cada disco). O jogo identifica o disco inserido pelo arquivo
// DISC\DISKn.ID (0x8C09EBC4); o host so escolhe a pasta e confere que ela e do disco pedido.
//
// Mensagens seguras: pasta ausente, disco trocado (a pasta do disco 2 tem o disco 1) e
// manifesto ilegivel viram std::runtime_error com o caminho e o que fazer; nada de travar nem
// de ler setor.
#include <filesystem>
#include <string>

namespace hh {

struct DiscData {
    std::filesystem::path dir;      // pasta de dados do disco (disc\, disc.manifest, disc_meta.*)
    int                   number = 1;
    std::string           version;  // hedhunter_v1006_a (disco 1) ou hedhunter_v1006_b (disco 2)
};

// `root`: a pasta de dados da instalacao (com disc1\ e disc2\) ou a pasta de um disco (com
// disc.manifest; instalacoes do HH06 e dados de teste). `number`: 1 ou 2.
DiscData resolve_disc(const std::filesystem::path& root, int number);

const char* disc_version(int number);  // versao esperada do disco (nullptr fora de 1..2)

} // namespace hh
