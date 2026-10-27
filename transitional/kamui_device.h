// SPDX-License-Identifier: MIT
#pragma once
// HH13.6: provedor nativo da camada Kamui (docs/planos/HH13-PLANO.md, "HH13.6"). Todo o caminho de render do jogo ate o
// hardware passa por cinco funis do SDK (sonda HH_TA_FONTES no roteiro da area 1):
//   0x8C1793E0 (off, val)   unica escrita de registrador do PVR: *(0xA05F8000 + off) = val;
//   0x8C1793EA (off)         leitura de registrador do PVR;
//   0x8C17C600 (desc)        pedido de DMA (tipo em +8; 0 = canal 2 para o TA): +12 bytes, +16 origem (RAM), +20 destino
//                            (FIFO de poligonos ou caminho direto de textura);
//   0x8C159820 (dst, src, n) copia em blocos de 8/4/2/1 bytes (texturas e o plano de fundo na VRAM, pela CPU);
//   0x8C159874 (dst, n)      zeragem (o framebuffer).
// Modo sombra (HH_KAMUI=sombra): os funis continuam originais e tambem alimentam um estado nativo proprio (registradores,
// memoria no layout de 32 bits da VRAM e o fluxo de parametros do TA dividido como o TA divide). Em cada STARTRENDER,
// antes do render, o estado nativo e conferido com o do runtime: registradores escritos, parametros desde o
// TA_LIST_INIT e os blocos de memoria escritos pelos funis desde a ultima conferencia. Prova que os funis cobrem tudo o
// que o render le, antes de o modo nativo deixar de passar pelo hardware.
#include "boundary_registry.h"

#include <cstdint>
#include <string>

namespace hh::transitional::kamui {

constexpr uint32_t kRegWrite = 0x8C1793E0, kRegRead = 0x8C1793EA, kDma = 0x8C17C600, kCopy = 0x8C159820,
                   kZero = 0x8C159874;

enum class Mode : uint8_t { Off, Shadow };

struct Stats {
    uint64_t regWrites = 0, regReads = 0, dmaTa = 0, dmaTexture = 0, dmaOther = 0, copies = 0, zeros = 0;
    uint64_t taBytes = 0, textureBytes = 0, copyBytes = 0, zeroBytes = 0;
    uint64_t renders = 0, regMismatch = 0, paramMismatch = 0, memMismatch = 0, memBlocksChecked = 0, paramsChecked = 0;
    std::string first;  // primeira divergencia (o que e onde)
};

void        install(BoundaryRegistry& registry, Mode m);  // exige os slots
void        uninstall(BoundaryRegistry& registry);
bool        installed();
Stats       stats();
std::string summary();  // linha "[kamui] ..." para o log

} // namespace hh::transitional::kamui
