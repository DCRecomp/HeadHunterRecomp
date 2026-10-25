// SPDX-License-Identifier: MIT
// Derivado de recomp-BStinger transitional/pad_contract.h (MIT, Copyright (c) 2026 Blue Stinger
// Recompiled contributors; ver third_party/licenses/recomp-BStinger-LICENSE.txt).
#pragma once
// Contrato do servidor de perifericos do SDK (Shinobi "pd Ver 1.50"), HH10.1 (docs/planos/HH10-PLANO.md).
//
// O jogo nao fala com o Maple: le uma tabela de 24 PDS_PERIPHERAL (4 portas x 6 unidades, 0x34
// bytes cada, em 8C311DAC) que o servidor 8C15DB3A reescreve quando o jogo o chama (0x8C0915F8,
// uma vez por passo de jogo) e o ponteiro que pdGetPeripheral (8C15DC40) devolve. Este arquivo
// descreve, sem Sh4Context e sem nada do protocolo, o que o servidor escreve em cada unidade a
// partir do que a camada "per" entrega (8C16F138: descricao bruta; 8C16F14A: info do dispositivo).
//
// Ordem do servidor por unidade i (disassembly de 8C15DB3A..8C15DC3E):
//   info = per_info(i); unidade+48 = info; old (+44) = on (+8) LIDO DA RAM (com as escritas do jogo);
//   status[i] = status_code(resposta) (8C15DB06)
//   i % 6 != 0 (subunidade): name = info + 20; nada mais
//   i % 6 == 0: controle (flags & 0x21, erro 0, resposta 8) -> id/support/name (8C15DAB6) e decode
//               (8C15D900); senao "sem dispositivo" (8C15D8C2); depois off = ~on e as bordas pelo modo.
// Diferencas do 1.07a do Blue Stinger: sem o ramo do teclado; o decodificador recebe o indice da
// unidade e trata os gatilhos com histerese (limiares em 8C3122F0/F1, um byte de estado por unidade
// para L em 8C3122F2+i e para R em 8C31230A+i); o modo 0 inverte a palavra (ativo em 0 -> ativo em 1).
#include <cstdint>

namespace hh::transitional::pad {

constexpr uint32_t kTable        = 0x8C311DAC;  // PDS_PERIPHERAL[24]
constexpr uint32_t kUnitSize     = 0x34;
constexpr int      kUnits        = 24;
constexpr uint32_t kStatusTable  = 0x8C31228C;  // uint32_t[24], codigo de estado por unidade
constexpr uint32_t kModeAddr     = 0x8C3122EC;  // modo do servidor (0 no jogo)
constexpr uint32_t kThrHighAddr  = 0x8C3122F0;  // limiar alto dos gatilhos (byte; pdSetThreshold 8C15DCEC)
constexpr uint32_t kThrLowAddr   = 0x8C3122F1;  // limiar baixo (byte)
constexpr uint32_t kLatchLAddr   = 0x8C3122F2;  // uint8_t[24]: gatilho L acionado (histerese)
constexpr uint32_t kLatchRAddr   = 0x8C31230A;  // uint8_t[24]: gatilho R acionado
constexpr uint32_t kNoDeviceName = 0x8C185F50;  // "(no device)"
constexpr uint32_t kGamePortTable = 0x8C247384; // PDS* do jogo por porta (+12 * porta; 0x8C0915F8)

struct Pds {
    uint32_t id = 0, support = 0, on = 0, off = 0, press = 0, release = 0;
    uint16_t r = 0, l = 0;
    int16_t  x1 = 0, y1 = 0, x2 = 0, y2 = 0;
    uint32_t name = 0, extend = 0, old = 0, info = 0;
    bool     operator==(const Pds&) const = default;
};
Pds  pds_load(const uint8_t* p);  // 0x34 bytes, little-endian
void pds_store(uint8_t* p, const Pds& s);

// O que a camada per entrega ao servidor para uma unidade.
struct Unit {
    uint32_t info      = 0;  // endereco guest da info (8C16F14A)
    uint32_t infoFlags = 0;  // *(info + 0): bit 0 controle, 0x20 outro tipo com dados
    uint32_t support   = 0;  // dados da funcao como o SDK os escreve em +4 (8C15DAB6)
    uint8_t  error     = 0;  // byte +2 da descricao bruta
    uint8_t  response  = 0;  // byte +4: codigo da resposta Maple (8 = dados)
    uint8_t  cond[8]   = {}; // condicao: 0-1 botoes (ativos em 0), 2 gatilho R, 3 gatilho L, 4-7 eixos x1 y1 x2 y2
};

// Estado dos gatilhos (RAM do SDK): limiares e o byte de histerese da unidade.
struct Triggers {
    uint8_t high = 0, low = 0;  // 8C3122F0, 8C3122F1
    uint8_t latchL = 0, latchR = 0;  // 8C3122F2+i, 8C31230A+i (entrada e saida)
};

enum class Kind { Subunit, Controller, Absent };
Kind classify(int unit, const Unit& u);

// Codigo de estado (8C15DB06) pelo byte de resposta.
uint32_t status_code(uint8_t response);

// Decodificador (8C15D900): on (antes das bordas), r, l, x1, y1, x2, y2 pelo `support` ja escrito.
// Gatilho (L byte 3 bit 17, R byte 2 bit 16): >= alto aciona e marca o estado; <= baixo solta e
// limpa; entre os dois, mantem o estado anterior.
void decode_controller(Pds& p, const uint8_t cond[8], uint32_t mode, Triggers& t);

// Bordas (8C15DBF0..DC20): off = ~on; modo 0: press = (on^old) & on, release = (on^old) & off;
// modo != 0: press = ~(on^old) | on, release = ~(on^old) | off.
void edges(Pds& p, uint32_t mode);

// "Sem dispositivo" (8C15D8C2) antes das bordas.
void clear_absent(Pds& p, uint32_t mode);

// Mutacoes do teste de contrato (negativos): um contrato errado de proposito tem de divergir do
// legado. old_nativo usa `nativeOld` (o on decodificado no servidor anterior) em vez da RAM.
enum Mutation : uint32_t {
    kMutNone          = 0,
    kMutSwapAB        = 1u << 0,  // A e B trocados em on
    kMutThresholdOff1 = 1u << 1,  // gatilho aciona a partir de alto - 1
    kMutOldNative     = 1u << 2,  // old vindo do estado nativo, nao da RAM
    kMutInfoNoConnect = 1u << 3,  // info sintetizada sem o bit de conectado
};

// Uma passada do servidor numa unidade: `before` e a unidade na RAM antes do servidor; `t` o estado
// dos gatilhos da unidade (atualizado). Devolve a unidade depois do servidor.
Pds server_unit(int unit, const Pds& before, const Unit& u, uint32_t mode, Triggers& t,
                uint32_t mutations = kMutNone, uint32_t nativeOld = 0);

// Info do controle padrao como o legado a monta do DEVICE_INFO (conteudo que o provedor nativo
// sintetiza): +0 flags (1 = controle), +4 funcao (bytes como na RAM), +0x10 01 e area FF, +0x14 nome
// (30 bytes com espacos), +0x34 licenca (60 bytes com espacos), +0x74 consumo em espera e maximo.
// O leiaute e o do BS (mesmo Shinobi `per`); o HH10 confere contra a RAM do legado (HH_PAD_TRACE).
constexpr int kInfoBytes = 0x78;
void controller_info(uint8_t out[kInfoBytes], uint32_t functionData, uint32_t mutations = kMutNone);

} // namespace hh::transitional::pad
