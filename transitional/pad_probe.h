// SPDX-License-Identifier: MIT
// Derivado de recomp-BStinger transitional/pad_probe.h (MIT, Copyright (c) 2026 Blue Stinger
// Recompiled contributors; ver third_party/licenses/recomp-BStinger-LICENSE.txt).
#pragma once
// Teste de contrato do servidor pd 1.50 contra o legado (HH10.0). Desligado por padrao.
//
//   HH_PAD_CONTRATO=<arquivo>           envolve o servidor 8C15DB3A (a condicao bruta de cada unidade
//                                       vem da camada per, 8C16F138) e confere cada passada com
//                                       pad_contract, inclusive as travas dos gatilhos; no fim grava
//                                       contagens, cobertura e divergencias
//   HH_PAD_CONTRATO_MUTACAO=<lista>     negativos: os nomes aceitos por pad_probe.cpp (troca_ab,
//                                       limiar_menos1, old_nativo, info_sem_conectado, ...)
//   HH_PAD_MODO=<n>                     forca o modo do servidor (8C3122EC) antes de cada passada
//
// Exige slots (build com mods): sem eles as chamadas diretas nao passariam pela sonda.
namespace hh::transitional::pad {

void probe_install();  // le o ambiente; nada sem HH_PAD_CONTRATO
void probe_finish();   // grava o relatorio (tambem no atexit)

} // namespace hh::transitional::pad
