// SPDX-License-Identifier: MIT
#pragma once
// HH15.6 (docs/planos/HH15-PLANO.md; modelo: system/sound_bus do N8.6 do Blue Stinger): area de som nativa, sem AICA,
// ARM7 nem driver. Com o audio nativo do HH14 (o padrao do produto) os comandos ao driver, a RAM de som e o DMA do G2
// sao do provedor nativo (transitional/audio_device), e a CPU so le o RTC na area da AICA (0x00710000/04, 0x8C1706E8,
// duas vezes por VBlank). O servico atende o contrato do RTC (hora do sistema, no relogio do provedor de tempo, com a
// liberacao de escrita em 0x00710008 como o modelo) e o registrador de controle 0x00702C00 (guardado), com a mesma
// instrumentacao que as referencias comparam (HH_CLOCK_LOG Rtc/Aud). Qualquer outro acesso a area (registradores de
// canal, DSP, temporizadores e interrupcoes do ARM7, a RAM de som pela CPU, outro tamanho) e erro explicito.
#include "cpu/sh4_context.h"

namespace hh {

void     sound_bus_reset();
void     sound_bus_rtc_set(uint32_t seconds, uint64_t now);  // segundos desde 1950-01-01, valendo no instante `now`
uint32_t sound_bus_rtc_host_now();                           // hora atual do host na epoca do Dreamcast
// false se `a` esta fora da area de som (0x00700000-0x0071FFFF e 0x00800000-0x00FFFFFF, fisico)
bool sound_bus_read(Sh4Context* c, uint32_t a, unsigned size, uint32_t& v);
bool sound_bus_write(Sh4Context* c, uint32_t a, unsigned size, uint32_t v);

} // namespace hh
