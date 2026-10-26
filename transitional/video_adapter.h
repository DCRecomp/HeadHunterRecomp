// SPDX-License-Identifier: MIT
#pragma once
// Provedor nativo dos filmes (HH12, --video nativo). Contrato em docs/planos/HH12-PLANO.md ("Contrato da API do
// filme"). A funcao do jogo que toca um filme (PlayScene 0x8C09E904) continua original, como a funcao de video do
// N5 do BS; as entradas do objeto do filme 4X que ela chama sao atendidas aqui, sobre os MP4 do Builder
// (native/video: catalogo + MoviePlayback), sem o decodificador 4X, o stream do arquivo nem a AICA:
//   8C11E9D2 open(obj, nome, params): troca a pasta do gdFs para \MOVIES pelo jogo (efeito observavel do original),
//            acha o filme no catalogo e preenche o objeto como o cabecalho do 4X (oraculo do Flycast no play:
//            +0x0C/+0x10 = largura/altura exibidas, +0x14/+0x18 = 640/480, +0x1C = trilhas de audio, trilha 0 em
//            +0x20 = {0, bytes/s}); obj+8 (stream) fica 0. Retorna 0; filme ausente: 1, como o open original sem arquivo.
//   8C11EBE0 audio(obj, trilha): so o efeito no objeto (obj[0x20 + 16*trilha + 12] = 0); o volume do stream nao existe.
//   8C11ECA4 play(obj, callback): obj+0x33C = 1; um passo por quadro do filme: espera o instante do quadro no relogio
//            do jogo (VBlanks, no ponto de espera declarado 0x8C11FC3A), mostra o quadro (o host o apresenta no lugar
//            do quadro do PVR), chama o callback do jogo (skip pelo controle: zera obj+0x33C) e para no fim do filme
//            ou com obj+0x33C == 0.
//   8C11EB32 close(obj, r5): obj+8 = 0 (nada mais foi alocado).
// create (8C11E30C) e destroy (8C11E360) continuam originais: so alocam e liberam o objeto.
//
// Some no HH15, quando os chamadores do jogo forem elevados.
#include "boundary_registry.h"

#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

namespace hh::transitional::video {

constexpr uint32_t kOpen = 0x8C11E9D2, kAudioTrack = 0x8C11EBE0, kPlay = 0x8C11ECA4, kClose = 0x8C11EB32;
constexpr uint32_t kWaitPc       = 0x8C11FC3A;  // espera do quadro no laco original (HH09, [native] esperas)
constexpr uint32_t kGdFsChangeDir = 0x8C15C7A0, kMoviesDirText = 0x8C142E78;  // gdFsChangeDir("\MOVIES")
constexpr uint32_t kObjContinue = 0x33C;  // o callback do jogo (0x8C09E896) grava 0 aqui no skip
// Por quadro, o laco original tambem conta o quadro desenhado (0x8C14A220, restaurado por PlayScene no fim, e 0x8C2388DC,
// o contador de quadros do jogo, lido por 0x8C073F28) e chama 0x8C016DF8 (tempos derivados desses contadores).
constexpr uint32_t kFrameCount = 0x8C14A220, kDrawnFrames = 0x8C2388DC, kFrameTimes = 0x8C016DF8;
// Legendas do jogo: o laco original chama 0x8C0E4006 por quadro, que desenha a lista carregada por 0x8C0E38A4
// (Locale\<idioma>\SubTitle\<filme>.txt) quando a opcao esta ligada (*0x8C151E14). Lista em *0x8C151E10: uma linha por
// no {+0x00 texto, +0x0C proximo, +0x10 segundos desde o inicio do filme; 0 = continua a legenda anterior}
// (RAM do jogo no SCENE01_06, docs/planos/HH12-PLANO.md).
constexpr uint32_t kSubList = 0x8C151E10, kSubOn = 0x8C151E14;

struct Options {
    std::filesystem::path dataDir;  // data\video (manifesto do Builder)
    std::filesystem::path modsDir;  // mods\video (opcional)
    bool                  audio = true;  // false: saida nula (headless)
};

struct Stats {
    uint64_t opens = 0, absent = 0, plays = 0, framesShown = 0, framesSkipped = 0, endedNatural = 0, endedByGame = 0;
    uint64_t vblanks = 0;  // VBlanks passados dentro do play nativo
    uint64_t subtitleCues = 0;  // legendas do jogo (ou do mod) dos filmes tocados
    uint64_t decodeErrors = 0;  // filmes interrompidos por erro do decodificador (nao contam como fim natural)
    std::string last;      // ultimo filme aberto (chave do catalogo)
};

// Instala o provedor (exige os slots; sem eles, std::runtime_error). Problemas do catalogo vao para `warnings`.
void        install(BoundaryRegistry& registry, const Options& o, std::vector<std::string>* warnings = nullptr);
void        uninstall(BoundaryRegistry& registry);
bool        installed();
Stats       stats();
std::string summary();  // linha "[video] ..." para o log

// Host (thread do jogo, no VBlank): com um filme na tela, compoe o quadro de 640x480 (ARGB) como o jogo o desenha
// (escala min(640/w, 480/h) da area exibida, centralizado, fundo preto) e devolve true.
bool compose_frame(std::vector<uint32_t>& argb, uint32_t& width, uint32_t& height);

} // namespace hh::transitional::video
