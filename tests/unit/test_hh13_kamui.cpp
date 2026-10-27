// SPDX-License-Identifier: MIT
// HH13.6: ciclos guest que o slot nativo da copia (0x8C159820) e da zeragem (0x8C159874) gasta no lugar do codigo gerado
// do original. Os valores esperados sao os medidos no roteiro da area 1 (HH_KAMUI=sombra HH_KAMUI_CUSTO=<tsv>: ciclos de
// cada chamada original por bytes e alinhamento) e, nos caminhos que o roteiro nao exercita, a soma dos HH_CYCLES dos
// blocos do codigo gerado (entrada, preparo do laco, voltas, rts).
#include "test.h"

#include "kamui_device.h"

namespace k = hh::transitional::kamui;

// medidos (todas as combinacoes de bytes/alinhamento do roteiro: 2 098 360 copias, 2 zeragens)
HH_TEST(hh13_kamui_copy_cycles_measured) {
    for (uint32_t dst = 0; dst < 32; dst += 4) HH_CHECK(k::copy_cycles(0x0C000000 + dst, 0x0C10000C, 4) == 15);
    HH_CHECK(k::copy_cycles(0x05000008, 0x0C100018, 24) == 25);  // laco de 8 bytes
    HH_CHECK(k::copy_cycles(0x05000000, 0x0C10001C, 48) == 70);  // laco de 4 bytes
    HH_CHECK(k::copy_cycles(0x05000010, 0x0C10001C, 48) == 70);
    HH_CHECK(k::copy_cycles(0x05000008, 0x0C100018, 60) == 85);
}

HH_TEST(hh13_kamui_zero_cycles_measured) { HH_CHECK(k::zero_cycles(0x05000000, 614400) == 307211); }

// somas dos blocos do codigo gerado nos caminhos de 2 e 1 byte (e de 4 bytes da zeragem)
HH_TEST(hh13_kamui_cycles_generated_paths) {
    HH_CHECK(k::copy_cycles(0x05000002, 0x0C100000, 6) == 5 + 2 + 2 + 1 + 5 * 3 + 2);
    HH_CHECK(k::copy_cycles(0x05000001, 0x0C100000, 3) == 5 + 2 + 2 + 5 * 3 + 2);
    HH_CHECK(k::zero_cycles(0x05000004, 8) == 4 + 2 + 2 + 4 * 2 + 2);
    HH_CHECK(k::zero_cycles(0x05000002, 6) == 4 + 2 + 2 + 2 + 4 * 3 + 2);
    HH_CHECK(k::zero_cycles(0x05000001, 3) == 4 + 2 + 2 + 1 + 4 * 3 + 2);
}
