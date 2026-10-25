// SPDX-License-Identifier: MIT
// Testes do decodificador SH-4 (HH03), sem framework.
//
// Partes fixadas aqui (com origem em analysis/hh03/sleigh_truth2.txt,
// oraculo SLEIGH/Ghidra SuperH4, 65.536 opcodes):
//   - total legal = 59001, ilegal = 6535 (HH04: as 50 codificacoes das
//     extensoes do SH-4A - synco, icbi, movua.l, fpchg - sao ilegais no SH7091);
//   - ilegais por major nibble: 0=1589, 2=256, 3=512, 4=2080, 8=1792, F=306;
//   - spot-checks de mnemonico (verificados na verdade por opcode).
// A equivalencia mnemonica completa tabela-vs-oraculo foi verificada na
// geracao (gen_decoder_table.py) com 1 divergencia documentada: 0xFFFD,
// que o SLEIGH etiqueta "fsca FPUL,dr14" mas que e ILEGAL por bit 8 = 1
// (paridade do par DRn: MIT "1111nnn011111101", dream-recomp {0xF1FF,0xF0FD}).
#include "sh4/ops.hpp"

#include <cstddef>
#include <iostream>
#include <string>

using namespace hh::sh4;

static int g_failed = 0;

#define CHECK(cond, msg)                                                                          \
    do {                                                                                          \
        if (!(cond)) {                                                                            \
            std::cerr << "FALHA: " << msg << " (linha " << __LINE__ << ")\n";                      \
            ++g_failed;                                                                          \
        }                                                                                         \
    } while (0)

namespace {

// Classificacao exaustiva: para todo opcode, exatamente um destino com
// o mnemonico da verdade (independe da ordem da tabela, pois o teste
// compara mnemonicos, nao indices).
void testClassifyExhaustive() {
    unsigned decoded = 0;
    unsigned illegal = 0;
    unsigned illegalPorMajor[16] = {};
    unsigned ambiguides = 0;

    for (unsigned op = 0; op <= 0xFFFF; ++op) {
        const InstrInfo* hit = lookup(static_cast<uint16_t>(op));
        if (hit == nullptr) {
            ++illegal;
            ++illegalPorMajor[op >> 12];
            continue;
        }
        ++decoded;
        // Pureza: nenhuma OUTRA linha com mnemonico diferente casa este op.
        const unsigned n = tableSize();
        for (unsigned i = 0; i < n; ++i) {
            const InstrInfo& row = *tableRow(i);
            if ((op & row.mask) == row.value && row.mnemonic != hit->mnemonic) {
                ++ambiguides;
                break;
            }
        }
    }

    CHECK(decoded == 59001, "total decodificado = 59001");
    CHECK(illegal == 6535, "total ilegal = 6535");
    const unsigned esperado[16] = {1589, 0, 256, 512, 2080, 0, 0, 0,
                                    1792, 0, 0, 0, 0, 0, 0, 306};
    for (unsigned m = 0; m < 16; ++m) {
        CHECK(illegalPorMajor[m] == esperado[m],
              "ilegais por major nibble divergem");
    }
    CHECK(ambiguides == 0, "nenhum opcode casa regras de mnemonico distintos");
}

// Pureza em nivel de gravura: para cada opcode casado, todas as linhas
// casadas tem o mesmo OperandFmt (a tabela e pura por shape).
void testPurezaOperando() {
    const unsigned n = tableSize();
    for (unsigned op = 0; op <= 0xFFFF; ++op) {
        const InstrInfo* hit = lookup(static_cast<uint16_t>(op));
        if (!hit) continue;
        for (unsigned i = 0; i < n; ++i) {
            const InstrInfo& row = *tableRow(i);
            if ((op & row.mask) == row.value && row.fmt != hit->fmt) {
                CHECK(false, "opcode casado por formatos divergentes");
                return;
            }
        }
    }
}

void testMnemonic() {
    struct V {uint16_t op; const char* mnem;};
    const V vetores[] = {
        {0x0009, "nop"}, {0x0008, "clrt"}, {0x0040, nullptr},
        {0xFFFD, nullptr}, // divergencia 0xFFFD: ilegal (b. 8 = par do par DRn)
        // extensoes do SH-4A: ilegais no SH7091 (HH04)
        {0x00AB, nullptr}, {0x00E3, nullptr}, {0x0FE3, nullptr}, {0x40A9, nullptr},
        {0x4FE9, nullptr}, {0xF7FD, nullptr},
        {0x300C, "add"}, {0x300E, "addc"}, {0x6003, "mov"},
        {0x8800, "cmp/eq"}, {0xE000, "mov"}, {0x0038, "ldtlb"},
        {0x4010, "dt"}, {0x4020, "shal"}, {0x4015, "cmp/pl"},
        {0x400A, "lds"}, {0xA000, "bra"}, {0xB000, "bsr"},
        {0x8900, "bt"}, {0x8D00, "bt/s"}, {0x0023, "braf"},
        {0x402B, "jmp"}, {0x400B, "jsr"}, {0x000B, "rts"},
        {0x002B, "rte"}, {0xF000, "fadd"}, {0xF02D, "float"},
        {0xF3FD, "fschg"}, {0xFBFD, "frchg"}, {0xFEFD, "fsca"},
        {0xF08D, "fldi0"}, {0x4019, "shlr8"}, {0x406A, "lds"},
        {0x001A, "sts"}, {0x0029, "movt"},
    };
    for (const V& v : vetores) {
        const InstrInfo* info = lookup(v.op);
        if (v.mnem == nullptr) {
            CHECK(info == nullptr, "0040 deve ser ILEGAL");
        } else {
            CHECK(info != nullptr, "opcode legal deve decodificar");
            if (info) CHECK(std::string(info->mnemonic) == v.mnem,
                            "mnemonico spot-check");
        }
    }
}

void testFlags() {
    const InstrInfo* bra = lookup(0xA000);
    CHECK(bra != nullptr && bra->has_delay_slot == 1, "bra tem delay slot");
    const InstrInfo* bt_s = lookup(0x8D00);
    CHECK(bt_s != nullptr && bt_s->has_delay_slot == 1, "bt/s tem delay slot");
    const InstrInfo* bt = lookup(0x8900);
    CHECK(bt != nullptr && bt->has_delay_slot == 0, "bt sem delay slot");
    const InstrInfo* rts = lookup(0x000B);
    CHECK(rts != nullptr && rts->has_delay_slot == 1, "rts tem delay slot");
    const InstrInfo* rte = lookup(0x002B);
    CHECK(rte != nullptr && rte->has_delay_slot == 1, "rte tem delay slot");
    const InstrInfo* add = lookup(0x300C);
    CHECK(add != nullptr && add->has_delay_slot == 0, "add sem delay slot");
    const InstrInfo* fadd = lookup(0xF000);
    CHECK(fadd != nullptr && fadd->fpu == 1, "fadd e FPU");
    const InstrInfo* fschg = lookup(0xF3FD);
    CHECK(fschg != nullptr && fschg->fpu == 1, "fschg e FPU");
    const InstrInfo* lds_fpscr = lookup(0x406A);
    CHECK(lds_fpscr != nullptr && lds_fpscr->fpu == 0,
          "lds fpscr nao e operacao FPU");
}

void testCampos() {
    const Decoded d1 = decode(0x7130); // add #0x30,r1
    CHECK(d1.rn == 1 && d1.imm8 == 0x30, "campos de add #imm");
    const Decoded d1b = decode(0x7003); // add #3,r0
    CHECK(d1b.rn == 0 && d1b.imm8 == 3, "rn de add #imm em R0");
    const Decoded d2 = decode(0x700A); // imm8 decimal sign-extended
    CHECK(d2.imm8 == 0x0A, "imm8 0x0A");
    const Decoded d3 = decode(0x700F); // 0x0F ainda positivo
    CHECK(d3.imm8 == 0x0F, "imm8 0x0F");
    const Decoded d4 = decode(0x70FA); // imm8 0xFA = -6
    CHECK(d4.imm8 == -6, "imm8 0xFA = -6");
    const Decoded d5 = decode(0xA001); // bra disp12=1
    CHECK(d5.disp12 == 1, "disp12 1");
    const Decoded d6 = decode(0xAFFF); // bra disp12 = -1 (12 bits assinado)
    CHECK(d6.disp12 == -1, "disp12 -1");
    const Decoded d7 = decode(0xEFFF); // imm8 = -1
    CHECK(d7.imm8 == -1, "imm8 -1");
    const Decoded d8 = decode(0x1234); // mov.l R3,@(disp4,R2)
    CHECK(d8.rn == 2 && d8.rm == 3, "campos bruto de 0x1234");
    const InstrInfo* ilegal = lookup(0x0040);
    CHECK(ilegal == nullptr, "0040 ilegal");
}

} // namespace

int main() {
    testClassifyExhaustive();
    testPurezaOperando();
    testMnemonic();
    testFlags();
    testCampos();
    if (g_failed == 0) {
        std::cout << "OK - hh03_decoder" << std::endl;
        return 0;
    }
    std::cout << g_failed << " falhas - hh03_decoder" << std::endl;
    return 1;
}
