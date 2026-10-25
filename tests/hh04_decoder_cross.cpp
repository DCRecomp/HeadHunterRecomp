// SPDX-License-Identifier: MIT
// HH04: o decodificador portado do Blue Stinger (hh::isa, usado pela analise e pelo
// gerador) e o decodificador declarativo do HH03 (hh::sh4, conferido contra o SLEIGH)
// foram construidos de forma independente. Os dois precisam concordar, opcode a
// opcode, em legalidade e mnemonico: e a segunda evidencia de que o tradutor decodifica
// o SH-4 corretamente.
#include "test.h"

#include "decoder/sh4_decoder.h"
#include "sh4/ops.hpp"

#include <cctype>
#include <map>
#include <string>

namespace {

std::string lower(const char* s) {
    std::string r(s ? s : "");
    for (char& ch : r) ch = char(std::tolower(static_cast<unsigned char>(ch)));
    return r;
}

} // namespace

HH_TEST(hh04_decoders_agree_on_legality_and_mnemonic) {
    unsigned legality = 0, mnemonic = 0;
    std::map<std::string, unsigned> diffs;
    for (uint32_t op = 0; op < 0x10000; ++op) {
        const hh::sh4::InstrInfo* a = hh::sh4::lookup(uint16_t(op));
        const hh::isa::Insn       b = hh::isa::decode(uint16_t(op));
        if ((a != nullptr) != b.valid()) {
            ++legality;
            ++diffs[std::string("legalidade hh03=") + (a ? a->mnemonic : "ILEGAL") +
                    " bs=" + (b.valid() ? b.info().mnemonic : "ILEGAL")];
            continue;
        }
        if (!a) continue;
        const std::string ma = lower(a->mnemonic), mb = lower(b.info().mnemonic);
        // Mesmo opcode, nomes diferentes: o SLEIGH chama a copia entre registradores
        // FRm,FRn de "fmov" e o padrao do Blue Stinger de "fmov.s" (o manual usa os dois
        // nomes conforme SZ). So essa troca de nome e aceita.
        if (ma == "fmov" && mb == "fmov.s") continue;
        if (ma != mb) {
            ++mnemonic;
            ++diffs["mnemonico " + ma + " <> " + mb];
        }
    }
    for (const auto& [k, n] : diffs) std::fprintf(stderr, "  %s: %u opcodes\n", k.c_str(), n);
    HH_CHECK(legality == 0);
    HH_CHECK(mnemonic == 0);
}

// Revisao HH04 (Codex, 2026-10-24): todas as instrucoes marcadas "New" no manual SH-4A,
// inclusive movco.l, movli.l e prefi, precisam ser ilegais nos dois decodificadores.
HH_TEST(hh04_sh4a_extensions_are_illegal) {
    struct Form { const char* name; uint32_t value, mask; };
    const Form forms[] = {{"movco.l", 0x0073, 0xF0FF}, {"movli.l", 0x0063, 0xF0FF}, {"prefi", 0x00D3, 0xF0FF},
                          {"synco", 0x00AB, 0xFFFF},   {"icbi", 0x00E3, 0xF0FF},    {"movua.l", 0x40A9, 0xF0BF},
                          {"fpchg", 0xF7FD, 0xFFFF}};
    for (const Form& f : forms) {
        unsigned legal = 0;
        for (uint32_t op = 0; op < 0x10000; ++op)
            if ((op & f.mask) == f.value)
                legal += (hh::sh4::lookup(uint16_t(op)) != nullptr) + hh::isa::decode(uint16_t(op)).valid();
        if (legal) std::fprintf(stderr, "  %s: %u opcodes legais\n", f.name, legal);
        HH_CHECK(legal == 0);
    }
}
