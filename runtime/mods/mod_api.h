// SPDX-License-Identifier: MIT
// Derivado de recomp-BStinger runtime/mods/mod_api.h (MIT, Copyright (c) 2026 Blue Stinger
// Recompiled contributors; ver third_party/licenses/recomp-BStinger-LICENSE.txt).
#pragma once
// API de mods do Head Hunter recompilado.
//
// Um mod e um arquivo .cpp na pasta mods/ do projeto editavel (gerado pelo Builder). Ele e
// compilado junto com o jogo (HH_MODS=ON) e registra o que quer mudar, sem editar as
// milhares de funcoes geradas:
//
//   #include "mods/mod_api.h"
//   static void minhaVersao(hh::Sh4Context* c) {
//       hh::mods::call_original(0x8C194580, c);   // comportamento original
//       c->r[0] = 1;                               // e depois muda o retorno
//   }
//   HH_MOD(exemplo) { hh::mods::replace(0x8C194580, minhaVersao); }
//
// Uma funcao SH-4 recompilada recebe o contexto (registradores r0-r15, FPU, memoria em
// c->ram) e segue a convencao do jogo (argumentos em r4-r7, retorno em r0). O mapa
// endereco -> funcao C++ esta em functions.tsv no projeto.
//
// replace() vale para TODAS as chamadas: diretas (o codigo gerado chama pelo slot da
// funcao quando HH_MODS=ON) e indiretas (despacho por ponteiro). Sem mods (build normal),
// as chamadas diretas continuam diretas, sem custo.
#include "cpu/sh4_runtime.h"

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace hh::mods {

// Uma entrada por funcao gerada (so com HH_MODS=ON): o slot pelo qual o codigo gerado chama.
struct Slot {
    uint32_t addr;
    Sh4Func* slot;
    Sh4Func  original;
};

// Chamado pelo codigo gerado (register_functions) com os slots de todas as funcoes.
void set_slots(const Slot* slots, size_t count);

// Troca a funcao do endereco SH-4 `addr` (chamadas diretas e indiretas). false se o
// endereco nao e uma funcao recompilada.
bool    replace(uint32_t addr, Sh4Func fn);
// true se as chamadas diretas a `addr` passam por slot (build HH_MODS): replace() entao vale
// para elas tambem; sem slot, so as indiretas mudam.
bool    has_slot(uint32_t addr);
// A funcao gerada original (para chamar de dentro da substituta). nullptr se nao existe.
Sh4Func original(uint32_t addr);
void    call_original(uint32_t addr, Sh4Context* c);  // fatal se nao existe
// Esperas (N2b, criterio B10). Um mod que reescreve um laco de espera do jogo (por exemplo a
// espera do VBlank em 8C064A20) chama wait() em cada volta e wait_end() na saida, como o codigo
// gerado faz com HH_WAIT/HH_WAIT_END nos pontos declarados em [native] esperas:
//   while (hh::sh4_read16(c, bandeira) == 0) hh::mods::wait(c, 0x8C064A2C, 0x8C064A26, 6);
//   hh::mods::wait_end(c, 0x8C064A2C);
// `pc` identifica a espera (o back-edge do original), `resume` e onde o original retomaria (vira
// SPC se uma interrupcao for entregue) e `cycles` o custo estimado de uma volta (enquanto o tempo
// andar por ciclos, sem ele o laco nao deixa o tempo andar). Um laco de espera sem wait() e
// trabalho sem fim para o monitor de progresso: falta de progresso com a thread e o PC (no
// modo sombra, aviso; quando o tempo so andar nas esperas, erro).
void wait(Sh4Context* c, uint32_t pc, uint32_t resume, uint32_t cycles);
void wait_end(Sh4Context* c, uint32_t pc);
// Ponto de poll sem espera (laco de trabalho longo num mod): interrupcoes e eventos vencidos.
void poll(Sh4Context* c, uint32_t resume, uint32_t cycles);

// Volta todas as substituicoes ao original (testes; nova sessao).
void    reset();

// Registro dos mods (HH_MOD) e inicializacao: o GameHost chama init_all() depois de
// registrar as funcoes, antes de o jogo comecar.
using ModInit = void (*)();
struct ModReg {
    ModReg(const char* name, ModInit init);
};
void                     init_all();
std::vector<std::string> registered();  // nomes dos mods compilados (log)

} // namespace hh::mods

#define HH_MOD(name)                                                       \
    static void                  hh_mod_init_##name();                    \
    static ::hh::mods::ModReg    hh_mod_reg_##name(#name, hh_mod_init_##name); \
    static void                  hh_mod_init_##name()
