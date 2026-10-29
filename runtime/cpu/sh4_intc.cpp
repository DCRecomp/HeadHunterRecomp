// SPDX-License-Identifier: MIT
// Derivado de recomp-BStinger runtime/cpu/sh4_intc.cpp (MIT, Copyright (c) 2026 Blue Stinger
// Recompiled contributors; ver third_party/licenses/recomp-BStinger-LICENSE.txt).
// Modelo dos registradores do INTC (manual SH7750, cap. 19 "Interrupt Controller"):
//   ICR 0xFFD00000, IPRA/IPRB/IPRC 0xFFD00004/08/0C (16 bits): guardados.
//   INTEVT 0xFF000028: codigo da ultima interrupcao aceita (gravado pela ponte, irq_bridge.cpp).
// HH15.2: a entrega das interrupcoes saiu para a ponte; este modelo atende os acessos do tempo estavel e, no tempo
// logico, os de fora do contrato do servico (sys_logical, sh4_intc_adopt).
#include "cpu/sh4_intc.h"

namespace hh {
namespace {

constexpr uint32_t kIcr = 0xFFD00000, kIpra = 0xFFD00004, kIprc = 0xFFD0000C;
constexpr uint32_t kIntevt = 0xFF000028;

uint16_t g_icr = 0, g_ipr[3] = {0, 0, 0};

} // namespace

void sh4_intc_adopt(uint16_t icr, const uint16_t ipr[3]) {
    g_icr = icr;
    for (int i = 0; i < 3; ++i) g_ipr[i] = ipr[i];
}

void sh4_intc_reset() {
    g_icr = 0;
    g_ipr[0] = g_ipr[1] = g_ipr[2] = 0;
    sh4_irq_set_intevt(0);
}

bool sh4_intc_read(uint32_t a, unsigned size, uint32_t& v) {
    if (a == kIntevt && size == 4) {
        v = sh4_irq_intevt();
        return true;
    }
    if (size != 2) return false;
    if (a == kIcr) v = g_icr;
    else if (a >= kIpra && a <= kIprc && !(a & 3)) v = g_ipr[(a - kIpra) / 4];
    else return false;
    return true;
}

bool sh4_intc_write(uint32_t a, unsigned size, uint32_t v) {
    if (a == kIntevt && size == 4) {
        sh4_irq_set_intevt(v);
        return true;
    }
    if (size != 2) return false;
    if (a == kIcr) g_icr = uint16_t(v);
    else if (a >= kIpra && a <= kIprc && !(a & 3)) g_ipr[(a - kIpra) / 4] = uint16_t(v);
    else return false;
    return true;
}

} // namespace hh
