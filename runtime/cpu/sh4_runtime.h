// SPDX-License-Identifier: MIT
// Derivado de recomp-BStinger runtime/cpu/sh4_runtime.h (MIT, Copyright (c) 2026 Blue Stinger
// Recompiled contributors; ver third_party/licenses/recomp-BStinger-LICENSE.txt).
#pragma once
// Pontos de contato entre o codigo gerado e o runtime: despacho de chamadas
// indiretas, excecoes, cache/store queues e verificacoes de fluxo.
#include "cpu/sh4_context.h"

#include <cstddef>

namespace hh {

using Sh4Func = void (*)(Sh4Context*);

struct Sh4FuncEntry {
    uint32_t addr;  // endereco fisico (29 bits) da entrada da funcao
    Sh4Func  fn;
};

// Tabela de funcoes recompiladas (ordenada por addr). Registrada pelo executavel.
void          sh4_set_function_table(const Sh4FuncEntry* table, size_t count);
// Funcao nativa (HLE) num endereco sem codigo recompilado, p.ex. as syscalls do BIOS
// em 0x8C001000+. A tabela recompilada tem prioridade.
void          sh4_register_hle(uint32_t addr, Sh4Func fn);
Sh4Func       sh4_lookup(uint32_t addr);  // nullptr se desconhecido
// A funcao gerada (ou HLE) do endereco, ignorando substituicoes de mods/provedores.
Sh4Func       sh4_lookup_generated(uint32_t addr);
// Substitui (fn != nullptr) ou restaura (nullptr) a funcao de um endereco para os desvios
// indiretos; usado pela camada de mods (runtime/mods), antes da tabela gerada.
void          sh4_override(uint32_t addr, Sh4Func fn);
inline uint32_t sh4_phys(uint32_t addr) { return addr & 0x1FFFFFFF; }

// Chamada/salto indireto: procura `target` e executa. Endereco desconhecido vai
// para sh4_dispatch_miss (log + politica configurada), nunca em silencio.
void sh4_dispatch(Sh4Context* c, uint32_t from_pc, uint32_t target);
void sh4_dispatch_miss(Sh4Context* c, uint32_t from_pc, uint32_t target);

// rts/rte para um endereco diferente do que o chamador esperava.
void sh4_bad_return(Sh4Context* c, uint32_t pc, uint32_t target);
// rte para um SPC diferente do esperado: continua em `target` (ver HH_RTE).
void sh4_rte_redirect(Sh4Context* c, uint32_t pc, uint32_t target);
// rts de uma funcao longjmp (config [threads]) para um alvo diferente do esperado:
// retoma a thread suspensa cujo R15 e o do contexto restaurado, no ponto de retorno do
// setjmp (`target`), ou comeca uma thread nova em `target` (ver sh4_threads.h).
void sh4_longjmp_redirect(Sh4Context* c, uint32_t pc, uint32_t target);
// Pontos de reentrada do codigo gerado (retornos de setjmp), ordenados.
void sh4_set_reentry_points(const uint32_t* points, size_t count);
// O fluxo chegou num endereco que a analise classificou como dado/invalido.
void sh4_unreachable(Sh4Context* c, uint32_t pc);
// Instrucao sem traducao (p.ex. opcode invalido alcancado).
void sh4_illegal(Sh4Context* c, uint32_t pc, uint16_t opcode);

void sh4_trapa(Sh4Context* c, uint32_t imm);
void sh4_address_error(Sh4Context* c, uint32_t addr, bool write);  // word/longword desalinhado
void sh4_sleep(Sh4Context* c);
void sh4_ldtlb(Sh4Context* c);
void sh4_sq_flush(Sh4Context* c, uint32_t addr);  // pref em 0xE0000000-0xE3FFFFFF
constexpr uint64_t kServiceQuantum = 2000;          // ciclos entre polls (~10 us a 200 MHz)
void sh4_service(Sh4Context* c);                   // eventos/interrupcoes pendentes
// HH15.1 (cadencia B do N8.1 do BS). Caminho lento do HH_POLL: atendimento (cycles >= next_event) ou tick de
// vivacidade (polls >= next_poll, so no tempo logico).
void sh4_poll_slow(Sh4Context* c);
// Tempo logico (--tempo experimental): o atendimento so roda quando pedido; o quantum de ciclos nao conta. Ligado pelo
// provedor de tempo na instalacao, com o contexto da sessao (o unico; as threads do guest o compartilham).
void sh4_set_logical_cadence(bool on);
bool sh4_logical_cadence();
void sh4_set_session_context(Sh4Context* c);  // registrado no boot; nullptr no encerramento
// Pede atendimento no proximo poll se ha nivel de interrupcao entregavel agora (SR.BL = 0, nivel IRL > SR.IMASK), sem
// avaliar eventos: chamado quando uma fonte acende ou uma mascara muda. Sem efeito no tempo por ciclos.
void sh4_irq_wakeup();
constexpr uint64_t kProgressTick = 4096;  // polls entre ticks de vivacidade no tempo logico
// HH06 (revisao Codex): gancho do host chamado a cada poll central (sh4_service), antes dos
// eventos. Permite ao host parar o jogo (lancando a propria excecao de parada) mesmo quando o
// guest deixou de produzir VBlank (laco preso, espera sem fim). nullptr = nenhum.
using Sh4ServiceHook = void (*)(Sh4Context* c);
void sh4_set_service_hook(Sh4ServiceHook fn);
// N2b-1 (modo sombra): back-edge de um ponto de espera declarado (HH_WAIT) e saida do laco.
void sh4_wait_point(Sh4Context* c, uint32_t pc);
void sh4_wait_end(Sh4Context* c, uint32_t pc);
// Codigo que o jogo copia para a RAM e o tradutor compilou no destino (relocated):
// confere que as instrucoes na RAM sao as compiladas; diferenca e fatal.
void sh4_check_code(Sh4Context* c, const uint32_t* pcs, const uint16_t* ops, size_t n);

// Rastreabilidade: toda funcao gerada chama HH_FN_ENTER(endereco SH-4). Com um
// observador registrado, cada entrada e reportada (testes diferenciais, traces).
using Sh4TraceFn = void (*)(Sh4Context* c, uint32_t entry);
// Erro fatal (despacho sem funcao, address error, ...). Sem tratador: log + abort.
// Um tratador (p.ex. hh-diff) pode lancar excecao para continuar o processo.
using Sh4FatalFn = void (*)(Sh4Context* c, const char* what, uint32_t pc, uint32_t extra);
void sh4_set_fatal_handler(Sh4FatalFn fn);
// Erro fatal a partir do hardware/HLE (mesmo tratamento do despacho).
[[noreturn]] void sh4_fatal(Sh4Context* c, const char* what, uint32_t pc, uint32_t extra);
extern Sh4TraceFn g_sh4_trace;
inline void sh4_set_trace(Sh4TraceFn fn) { g_sh4_trace = fn; }

} // namespace hh
