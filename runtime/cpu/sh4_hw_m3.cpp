// SPDX-License-Identifier: MIT
// Pontos de contato com o hardware na fase de traducao (HH04, equivalente ao M3 do
// Blue Stinger): ainda nao ha memoria fora da RAM principal, fontes de interrupcao,
// tempo nem store queues. Todo acesso a eles e erro fatal explicito. As duas unicas
// funcoes que retornam sem efeito sao exatas para um sistema SEM fontes de evento:
// sh4_service (nada a entregar; so reprograma o quantum dos back-edges) e
// sh4_irq_check (nenhuma interrupcao pode estar pendente). O HH05 substitui este
// arquivo pelo hardware temporario (runtime/memory, pvr, intc, ...).
#include "cpu/sh4_intc.h"
#include "cpu/sh4_mem.h"
#include "cpu/sh4_runtime.h"

namespace hh {

uint8_t  sh4_read8_slow(Sh4Context* c, uint32_t a) { sh4_fatal(c, "leitura de 8 bits fora da RAM (sem hardware no HH04)", c->pc, a); }
uint16_t sh4_read16_slow(Sh4Context* c, uint32_t a) { sh4_fatal(c, "leitura de 16 bits fora da RAM ou desalinhada (sem hardware no HH04)", c->pc, a); }
uint32_t sh4_read32_slow(Sh4Context* c, uint32_t a) { sh4_fatal(c, "leitura de 32 bits fora da RAM ou desalinhada (sem hardware no HH04)", c->pc, a); }
void     sh4_write8_slow(Sh4Context* c, uint32_t a, uint8_t) { sh4_fatal(c, "escrita de 8 bits fora da RAM (sem hardware no HH04)", c->pc, a); }
void     sh4_write16_slow(Sh4Context* c, uint32_t a, uint16_t) { sh4_fatal(c, "escrita de 16 bits fora da RAM ou desalinhada (sem hardware no HH04)", c->pc, a); }
void     sh4_write32_slow(Sh4Context* c, uint32_t a, uint32_t) { sh4_fatal(c, "escrita de 32 bits fora da RAM ou desalinhada (sem hardware no HH04)", c->pc, a); }

void sh4_sq_flush(Sh4Context* c, uint32_t addr) { sh4_fatal(c, "store queue (sem hardware no HH04)", c->pc, addr); }

// Sem eventos: o poll so reprograma o proximo quantum.
void sh4_service(Sh4Context* c) { c->next_event = c->cycles + kServiceQuantum; }
void sh4_irq_check(Sh4Context*) {}
void sh4_irq_check_sr(Sh4Context*) {}
void sh4_poll_slow(Sh4Context* c) { sh4_service(c); }
// SLEEP so termina com uma interrupcao; sem fonte de interrupcao, retornar seria
// silenciar uma espera infinita (revisao Codex HH04).
void sh4_sleep(Sh4Context* c) { sh4_fatal(c, "sleep sem fonte de interrupcao (sem hardware no HH04)", c->pc, 0); }
// So emitidos para esperas declaradas em [native] esperas (nenhuma no HH04).
void sh4_wait_point(Sh4Context* c, uint32_t pc) { sh4_fatal(c, "ponto de espera sem provedor de tempo (HH04)", pc, 0); }
void sh4_wait_end(Sh4Context* c, uint32_t pc) { sh4_fatal(c, "fim de espera sem provedor de tempo (HH04)", pc, 0); }

void sh4_rte_redirect(Sh4Context* c, uint32_t pc, uint32_t target) { sh4_fatal(c, "rte para contexto inesperado (sem threads no HH04)", pc, target); }
void sh4_longjmp_redirect(Sh4Context* c, uint32_t pc, uint32_t target) { sh4_fatal(c, "longjmp para contexto inesperado (sem threads no HH04)", pc, target); }
// Pontos de reentrada so existem com [threads] (setjmp/longjmp), ainda nao suportado.
void sh4_set_reentry_points(const uint32_t*, size_t count) {
    if (count) sh4_fatal(nullptr, "pontos de reentrada ([threads]) sem suporte no HH04", 0, uint32_t(count));
}

} // namespace hh
