// SPDX-License-Identifier: MIT
#pragma once
// HH14: provedor nativo do audio (docs/planos/HH14-PLANO.md). O jogo so chega a AICA por sete funis do Audio64
// (sonda do HH07): _acG2SetHostCommand 0x8C122440 (fila de comandos ao driver ARM7), acG2Write 0x8C122784, acG2Read
// 0x8C122602, acG2Fill 0x8C12250E, acG2ReadLong 0x8C1226AC, acG2WriteLong 0x8C12271C e a limpeza dos registradores do DSP
// 0x8C12234C, mais acDigiGetCurrentReadPosition 0x8C120610 (le a tabela de posicoes direto da RAM de som). No modo nativo eles deixam de tocar a AICA: a RAM de som e os registradores sao nativos e os comandos vao a
// uma reconstrucao do driver AUDIO64.DRV (lido estaticamente; nunca executado): porta = canal da AICA, a sombra de cada
// canal, as tabelas que o jogo le (posicao atual 0xB34, anterior 0xC34, alvo do evento 0xD34, mensagens no anel
// 0xAEC-0xB2B com o ponteiro em 0xE4) e o pedido de interrupcao ao SH-4 (MCIPD bit 5). O som sai do motor nativo de canais
// (native/audio/aica_channels), que anda pelo relogio logico (44 100 amostras por segundo de tempo do jogo).
#include "boundary_registry.h"

#include <cstdint>
#include <string>

struct Sh4Context;

namespace hh::native::audio {
class IAudioOutput;
}

namespace hh::transitional::audio {

constexpr uint32_t kHostCommand = 0x8C122440, kG2Write = 0x8C122784, kG2Read = 0x8C122602, kG2Fill = 0x8C12250E,
                   kG2ReadLong = 0x8C1226AC, kG2WriteLong = 0x8C12271C, kDspClear = 0x8C12234C, kReadPosition = 0x8C120610,
                   kDmaStart = 0x8C123564;

struct Options {
    native::audio::IAudioOutput* output = nullptr;  // janela: XAudio2; headless: nulo (o audio e so medido)
    std::string                  wavPath;           // diagnostico: grava tudo o que o motor produziu (pasta privada)
    std::string                  readsPath;         // diagnostico: leituras do SH-4 na RAM de som (endereco, contagem)
};

struct Stats {
    uint64_t commands = 0, unknownCommands = 0, writes = 0, writeBytes = 0, reads = 0, readBytes = 0, fills = 0;
    uint64_t readLongs = 0, writeLongs = 0, samples = 0, keyOns = 0, events = 0, irqs = 0, dropped = 0, transfers = 0;
    uint64_t byOpcode[256] = {};
    std::string first;  // primeiro comando desconhecido ou recusado
};

void        install(BoundaryRegistry& registry, const Options& o);  // exige os slots
void        uninstall(BoundaryRegistry& registry);
bool        installed();
Stats       stats();
std::string summary();  // linha "[audio] ..." para o log

// Avanca o motor ate o instante atual e entrega o produzido a saida (chamado pelo host a cada VBlank).
void pump(Sh4Context* c);

} // namespace hh::transitional::audio
