// SPDX-License-Identifier: MIT
// Derivado de recomp-BStinger transitional/pad_adapter.h (MIT, Copyright (c) 2026 Blue Stinger
// Recompiled contributors; ver third_party/licenses/recomp-BStinger-LICENSE.txt).
#pragma once
// Provedor nativo do SDK de perifericos "pd Ver 1.50" (HH10, docs/planos/HH10-PLANO.md; --controles nativo).
//
// Caminho: InputSystem (native/platform/input.h) -> este adaptador -> PDS_PERIPHERAL na RAM guest ->
// jogo. Sem Maple: no modo nativo o barramento fica sem controle (maple_set_controller_present) e
// nada aqui le registrador, tabela de DMA ou resposta Maple. Tres slots (config [native] slots):
//   8C17F0AA  disparo do DMA da camada per no VBlank (tratador 8C16E908): so o PONTO DE AMOSTRAGEM
//             (latch), o mesmo instante em que o legado tirava a amostra. O original continua rodando
//             depois: a camada per sonda as portas (VMU ate o HH11), sem controle nenhum.
//   8C15DB3A  servidor pd (chamado pelo jogo em 0x8C0915F8, uma vez por passo): escreve as 24 unidades
//             pelo contrato do HH10.1 (pad_contract): old lido da propria RAM, on/off/press/release
//             pelo modo, gatilhos com histerese (estado na RAM do SDK), analogicos, support, name,
//             info e a tabela de estado.
//   8C15DC40  pdGetPeripheral(porta): 8C311DAC + 52 * porta (MACL como o original).
// O init 8C15D744 NAO e substituido (inicializa per/Maple, que a VMU usa ate o HH11).
//
// Presenca: o SDK so ve um controle depois de identifica-lo (DEVICE_INFO) e o perde quando uma
// leitura nao tem resposta. O adaptador reproduz essa semantica sobre as amostras do InputSystem
// (identificar -> ler; leitura sem controle -> perder), sem quadro nem resposta Maple. O indice das
// amostras (`reads`) e o numero da leitura do legado.
// Coleta (regra medida, revisao Codex HH10): o pedido e o resultado valem no proprio disparo. No SDK a
// ordem estatica e coleta anterior (8C16ED76) -> novo disparo (8C16ED7C); no runtime hibrido o Maple
// legado responde na escrita do MDST e a resposta ja chega a passada do servidor do mesmo passo. Essa
// e a semantica do oraculo (o legado), comprovada pelos portoes C1/C4/C5/C8 (traco do PDS, leituras,
// info/name e quadros iguais por VBlank, nos tempos estavel e experimental, inclusive com
// desconexao). E uma equivalencia com o runtime hibrido, nao um modelo da duracao fisica do DMA; o
// HH15 a troca pela fronteira de quadro nativa.
//
// info/name da porta A: o legado aponta para o buffer da camada per, cujo conteudo a camada troca na
// coleta. Aqui o ponteiro e fixo na area nativa do BIOS HLE (kHleNativePadInfo) e o conteudo segue o
// modelo: info do DEVICE_INFO do controle padrao, sintetizada, com o controle identificado; senao a
// copia do buffer da camada per. Conferida antes de cada escrita (escrita alheia = erro). As outras 23
// unidades apontam para os buffers da camada per, como no legado.
//
// Some no HH15, quando os consumidores do jogo forem elevados.
#include "boundary_registry.h"
#include "platform/input.h"

#include <cstdint>
#include <string>

namespace hh::transitional::pad {

constexpr uint32_t kServer     = 0x8C15DB3A;
constexpr uint32_t kGetter     = 0x8C15DC40;
constexpr uint32_t kDmaTrigger = 0x8C17F0AA;
// per_info(i) (8C16F14A -> 8C17F470): *(*(8C17F4A8)) + (int16)*(8C17F49A) + 120 * i
constexpr uint32_t kPerInfoPtrLiteral = 0x8C17F4A8, kPerInfoOffLiteral = 0x8C17F49A, kPerInfoStride = 120;
// per_raw(i) (8C16F138): *(8C16F18C) + 24 * i; +2 erro, +4 resposta, +8 quadro (condicao em quadro + 8)
constexpr uint32_t kPerRawLiteral = 0x8C16F18C, kPerRawStride = 24;
// configuracao 5 da camada per (8C16F736(5) -> 8C17F528): *(*(8C17F600) + (int16)*(8C17F5E8)); 1 = o
// servidor converte o support (8C15DA94)
constexpr uint32_t kPerCfgPtrLiteral = 0x8C17F600, kPerCfg5OffLiteral = 0x8C17F5E8;
constexpr uint32_t kControllerFunction = 0xFE060F00;  // dados da funcao do controle padrao (DEVICE_INFO)

// Negativos do teste do N3-2 (HH_PAD_NATIVO_MUTACAO=<lista>): cada um tem de ser acusado.
enum NativeMutation : uint32_t {
    kNatNone         = 0,
    kNatOldHost      = 1u << 0,  // old_host: old do on nativo anterior, nao da RAM
    kNatPressOn      = 1u << 1,  // press_on: press = on (sem borda)
    kNatReleaseZero  = 1u << 2,  // release_zero: release sempre 0
    kNatSampleEarly  = 1u << 3,  // amostra_adiantada: a amostra do indice seguinte (1 leitura adiantada; C4)
    kNatInfoNoConnect = 1u << 4, // info_sem_conectado
    kNatPresenceNow  = 1u << 5,  // presenca_imediata: le sem identificar (reconexao um disparo antes)
    kNatMaple        = 1u << 6,  // maple: o controle continua no Maple com a ponte (retorno indevido ao Maple)
};
uint32_t parse_mutations(const std::string& list);  // nomes acima separados por virgula; desconhecido: lanca

struct Stats {
    uint64_t triggers = 0, dmaTriggers = 0, servers = 0, getters = 0;  // dmaTriggers: o original disparou (r0 >= 0)
    uint64_t latches = 0, presence = 0;   // amostras completas / consultas so de presenca
    uint64_t reads = 0;                   // leituras com resposta (o g_condSeq do legado)
    uint64_t identified = 0, lost = 0;    // controle identificado / perdido
    uint64_t guardFaults = 0;
    uint64_t extraServers = 0;            // passadas do servidor alem da primeira entre dois disparos efetivos
};

// Instala os tres provedores nativos (exige os slots; sem eles, std::runtime_error). `sys` e o dono
// das amostras (o chamador mantem vivo ate uninstall).
void  install(BoundaryRegistry& registry, native::input::InputSystem& sys, uint32_t mutations = kNatNone);
void  uninstall(BoundaryRegistry& registry);
bool  installed();
Stats stats();
uint64_t    reads();    // indice das amostras (coluna "leituras" do HH_PAD_TRACE no modo nativo)
std::string summary();  // linha "[pd nativo] ..." para o log

// Testes unitarios: o mesmo modelo e as mesmas funcoes, sem registro nem slots (RAM de teste).
namespace test {
void begin(native::input::InputSystem& sys, uint32_t mutations = kNatNone);
void dma_fired(Sh4Context* c);  // um disparo efetivo de 8C17F0AA (coleta do anterior + amostra deste)
void server(Sh4Context* c);   // 8C15DB3A
void getter(Sh4Context* c);   // 8C15DC40
void end();
} // namespace test

// Diagnostico HH_PAD_FECHAMENTO=<arquivo> (prova dinamica): conta as entradas nas funcoes geradas do
// SDK pd e da camada per (HH_FN_ENTER) e grava, no fim, as contagens com os contadores do Maple.
void closure_install();  // le o ambiente; nada sem a variavel
void closure_finish(bool nativeMode);

} // namespace hh::transitional::pad
