// SPDX-License-Identifier: MIT
#pragma once
// HH15.10 (D-HH15-1 = B, docs/planos/HH15-PLANO.md secao 7): funcoes do jogo elevadas no lugar das geradas. Cada
// elevada entra pelo slot da funcao gerada (mods::replace; o endereco precisa estar em [native] slots do config, senao
// as chamadas diretas continuariam no gerado e a instalacao recusa). A logica fica em native/game; aqui fica o
// adaptador da fase intermediaria: os argumentos pela convencao do jogo, a RAM guest e o custo do caminho original
// (ciclos e back-edges, enquanto o tempo por ciclos e o tick de vivacidade existirem), que mantem as referencias.
//
// Modos (HH_ELEVACAO, so na verificacao): ativo (padrao, o do produto), gerado (as funcoes geradas) e conferir: em
// cada chamada a elevada roda num ensaio (escritas num diario, desfeitas; back-edges so contados), a gerada roda de
// verdade e o resultado e comparado (custo, registradores do contrato, escritas da elevada; a RAM inteira numa amostra,
// menos o quadro da propria funcao, os 256 bytes abaixo do r15 de entrada: o que a gerada empilha e lixo depois do
// retorno, e as referencias de RAM do HH15 ignoram a pilha morta das threads). Uma chamada em que a gerada foi atendida (interrupcao, tick) so
// conta; se so a elevada teria sido atendida, e divergencia.
#include "cpu/sh4_runtime.h"

#include <cstdint>
#include <span>
#include <string>

namespace hh::transitional::elevacao {

enum class Retorno : uint8_t { Nenhum, R0, Fr0 };
// Contrato de registradores conferido: o da convencao C (r8-r15 e o resto preservado, mais o retorno) ou o completo
// (tambem r0-r7, T, FPUL e fr0-fr11 como a gerada deixa: o escalonador do jogo salva todos os registradores no contexto
// da thread, na RAM, a cada troca, entao um registrador "morto" diferente aparece na RAM) ou o auxiliar (o
// completo menos os bits M, Q e T do SR: rascunho das sequencias de div1, que toda divisao recomeca com div0s/div0u).
enum class Contrato : uint8_t { C, Completo, Auxiliar };

// Palavra do executavel (literal pool) da qual a elevada depende: o valor fica embutido na elevada.
struct Premissa {
    uint32_t endereco;
    uint32_t valor;
};

struct Elevada {
    uint32_t                  endereco;
    const char*               nome;
    Sh4Func                   funcao;
    Retorno                   retorno;
    std::span<const Premissa> premissas;
    Contrato                  contrato = Contrato::C;
};

// Todas as elevadas: as do jogo (elevadas_jogo.cpp) e as da biblioteca do compilador e do runtime C
// (elevadas_runtime_c.cpp) e as da biblioteca matematica (elevadas_libm.cpp), nessa ordem.
std::span<const Elevada> lista();
std::span<const Elevada> elevadas_jogo();
std::span<const Elevada> elevadas_runtime_c();
std::span<const Elevada> elevadas_libm();

enum class Modo : uint8_t { Gerado, Ativo, Conferir };
// Instala o modo (Gerado devolve as geradas). std::runtime_error se uma elevada nao tem slot ou nao e funcao gerada.
void        install(Modo m);
Modo        modo();
const char* nome_modo(Modo m);
// As premissas de todas as elevadas contra a RAM (o executavel ja carregado); erro explicito na primeira diferente.
void verificar_premissas(Sh4Context* c);

struct Stats {
    uint64_t chamadas       = 0;  // no modo conferir
    uint64_t conferidas     = 0;  // sem atendimento na gerada: comparadas
    uint64_t completas      = 0;  // das conferidas, com a RAM inteira
    uint64_t comAtendimento = 0;  // a gerada foi atendida: so contadas
    uint64_t atendidasComRetornoDiferente = 0;  // diagnostico: dessas, o retorno da elevada (ensaio) difere do da gerada
};
const Stats& stats();                // total
const Stats& stats(size_t indice);   // por elevada (indice de lista())
std::string  summary();              // o total e uma linha por elevada
bool         em_ensaio();            // testes: o ensaio terminou (inclusive por excecao)

// ---- para o codigo elevado ----

// Custo de um trecho linear do original (HH_CYCLES).
inline void cobrar(Sh4Context* c, uint32_t ciclos) { c->cycles += ciclos; }
// Back-edge do original (HH_POLL com o mesmo ponto de retomada); no ensaio da conferencia, so contado.
void volta(Sh4Context* c, uint32_t retomada);
// Chamada a outra elevada como a gerada chama (jsr): PR e o retorno esperado, o r15 do chamador, e o poll no retorno
// (HH_CALL), com cur_fn no endereco da chamada. A elevada chamada roda direto (no ensaio, tambem ela no diario). Um longjmp
// pendente depois da chamada ou de um poll e erro explicito.
void chamar(Sh4Context* c, uint32_t endereco, Sh4Func f, uint32_t retorno);
// RAM principal, alinhada; outro endereco e erro explicito (a elevada nao toca hardware).
uint32_t ler32(Sh4Context* c, uint32_t a);
uint8_t  ler8(Sh4Context* c, uint32_t a);
void     escrever8(Sh4Context* c, uint32_t a, uint8_t v);
float    ler_f(Sh4Context* c, uint32_t a);
void     escrever32(Sh4Context* c, uint32_t a, uint32_t v);
void     escrever_f(Sh4Context* c, uint32_t a, float v);
// Contrato de FPU das elevadas em precisao simples: FPSCR.PR = 0 e SZ = 0.
void exigir_fpu_simples(Sh4Context* c, uint32_t funcao);

} // namespace hh::transitional::elevacao
