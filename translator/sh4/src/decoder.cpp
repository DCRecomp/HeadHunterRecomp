// SPDX-License-Identifier: MIT
// Decodificador SH-4 declarativo - HH03.
// A tabela (mask,value)->InstrInfo e gerada por analysis/hh03/
// gen_decoder_table.py a partir do atlas de retangulos puros, verificado
// contra o oraculo SLEIGH (Ghidra SuperH4). Os retangulos sao disjuntos e
// cobrem exatamente os opcodes legais: o casamento linear abaixo nunca e
// ambiguo; a ordenacao (popcount descendente) e apenas determinismo.
#include "sh4/ops.hpp"

namespace hh::sh4 {

namespace {

#include "decoder_table.inc"

} // namespace

const InstrInfo* lookup(uint16_t op) {
    for (unsigned i = 0; i < kTableSize; ++i) {
        if ((op & kTable[i].mask) == kTable[i].value) {
            return &kTable[i];
        }
    }
    return nullptr;
}

Decoded decode(uint16_t op) {
    Decoded d{};
    d.op = op;
    const InstrInfo* info = lookup(op);
    d.row = info ? static_cast<uint16_t>(info - kTable) : 0;
    d.rn = (op >> 8) & 0xF;
    d.rm = (op >> 4) & 0xF;
    d.imm8 = static_cast<int8_t>(op & 0xFF);
    d.disp12 = static_cast<int16_t>(op << 4) >> 4;
    return d;
}

unsigned tableSize() { return kTableSize; }

const InstrInfo* tableRow(unsigned index) {
    return index < kTableSize ? &kTable[index] : nullptr;
}

} // namespace hh::sh4
