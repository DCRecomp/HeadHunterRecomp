// SPDX-License-Identifier: MIT
// Mini suga de diagnostico HH03: executa UM step por linha de comando e
// imprime o estado final. Usar via shell para interrogar o executor:
//   hh03_probe.exe op=0x2E50 r5=0x8081 r14=0x4000 [sr=...] [fpscr=...]
//                 [pc=...] [step=N] [macl=...] [fpul=...] [fr0=...]
// Sintaxe de campo: nome=valor (hex com 0x; decimal sem). Nomes: rN, fxN
// (fr), xfN, gadmissao: mach/macl/pr/fpul/gbr/vbr/ssr/spc/sgr/dbr/sr/fpscr/
// rbankN/pc/memA:valor / memB:valor / readA (le 32 bits, imprime).
#include "sh4/cpu.hpp"

#include <bit>
#include <cctype>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <string>

using namespace hh::sh4;

namespace {

class ProbeBus final : public Sh4Bus {
public:
    uint8_t mem[1 << 20] = {};
    uint8_t read8(uint32_t a) override { return mem[a & 0xFFFFF]; }
    uint16_t read16(uint32_t a) override {
        return uint16_t(mem[a & 0xFFFFF]) | uint16_t(mem[(a + 1) & 0xFFFFF]) << 8;
    }
    uint32_t read32(uint32_t a) override {
        return uint32_t(mem[a & 0xFFFFF]) | uint32_t(mem[(a + 1) & 0xFFFFF]) << 8 |
               uint32_t(mem[(a + 2) & 0xFFFFF]) << 16 |
               uint32_t(mem[(a + 3) & 0xFFFFF]) << 24;
    }
    void write8(uint32_t a, uint8_t v) override { mem[a & 0xFFFFF] = v; }
    void write16(uint32_t a, uint16_t v) override {
        mem[a & 0xFFFFF] = uint8_t(v);
        mem[(a + 1) & 0xFFFFF] = uint8_t(v >> 8);
    }
    void write32(uint32_t a, uint32_t v) override {
        mem[a & 0xFFFFF] = uint8_t(v);
        mem[(a + 1) & 0xFFFFF] = uint8_t(v >> 8);
        mem[(a + 2) & 0xFFFFF] = uint8_t(v >> 16);
        mem[(a + 3) & 0xFFFFF] = uint8_t(v >> 24);
    }
};

uint32_t val(const char* s) {
    return s[0] == '-' ? ~strtoul(s + 1, nullptr, 16) + 1 : strtoul(s, nullptr, 16);
}

// Indices de registrador sao DECIMAIS (r15 = 15, nao 0x15).
unsigned idx(const char* s) { return unsigned(strtoul(s, nullptr, 10)); }
} // namespace

int main(int argc, char** argv) {
    static ProbeBus bus;
    static Sh4State s;
    static unsigned ntrap = 0, nldtlb = 0, ncache = 0, nsleep = 0;
    static unsigned nillegal = 0;
    static uint32_t last_illegal = 0;
    s.on_trapa = [](Sh4State&, uint32_t) { ++ntrap; };
    s.on_ldtlb = [](Sh4State&) { ++nldtlb; };
    s.on_cache = [](Sh4State&, uint32_t, uint32_t) { ++ncache; };
    s.on_sleep = [](Sh4State&) { ++nsleep; };
    s.on_illegal = [](Sh4State&, uint32_t, uint16_t op) { ++nillegal; last_illegal = op; };
    s.reset(0x90000);

    bool wrote_op = false;
    uint32_t opval = 0;
    unsigned steps = 1;
    const char* reads[16];
    int nreads = 0;
    for (int i = 1; i < argc; ++i) {
        const char* a = argv[i];
        auto num = [](const char* p) { return val(p); };
        auto at = strchr(a, '=');
        if (!at) {
            if (strncmp(a, "read", 4) == 0) reads[nreads++] = a + 4;
            continue;
        }
        const std::string name(a, at);
        uint32_t v = num(at + 1);
        if (name == "op") {
            opval = v;
            wrote_op = true;
        } else if (name.rfind("opA", 0) == 0 && name.size() > 3) {
            // opADDR=0xXXXX: grava opcode em endereço fixo (hex), ex: op90004
            bus.write16(num(&name[3]), uint16_t(v));
        } else if (name.rfind("memB", 0) == 0) {
            bus.write16(v, 0);
        } else if (name.rfind("mem", 0) == 0) {
            bus.write32(num(&name[3]), v);
        } else if (name.rfind("r", 0) == 0 && isdigit(name[1])) {
            s.r[idx(&name[1])] = v;
        } else if (name.rfind("rbank", 0) == 0) {
            s.rbank[idx(&name[5])] = v;
        } else if (name.rfind("fr", 0) == 0) {
            s.fr[idx(&name[2])] = v;
        } else if (name.rfind("xf", 0) == 0) {
            s.xf[num(&name[2])] = v;
        } else if (name == "sr") {
            s.set_sr(v);
        } else if (name == "fpscr") {
            s.set_fpscr(v);
        } else if (name == "mach") s.mach = v;
        else if (name == "macl") s.macl = v;
        else if (name == "pr") s.pr = v;
        else if (name == "fpul") s.fpul = v;
        else if (name == "gbr") s.gbr = v;
        else if (name == "vbr") s.vbr = v;
        else if (name == "ssr") s.ssr = v;
        else if (name == "spc") s.spc = v;
        else if (name == "sgr") s.sgr = v;
        else if (name == "dbr") s.dbr = v;
        else if (name == "pc") s.pc = v;
        else if (name == "step") steps = v;
        else std::printf("?? campo ignorado: %s\n", a);
    }
    // O op grava APÓS todos os campos (assim pc= molda onde ele fica).
    if (wrote_op) bus.write16(s.pc, uint16_t(opval));
    if (!wrote_op) {
        std::printf("uso: op=0xXXXX ...\n");
        return 2;
    }
    for (unsigned st = 0; st < steps; ++st) {
        StepResult r = s.step(bus);
        std::printf("step %u: op=%04X pc=0x%X illegal=%d hooks(t=%d l=%d c=%d s=%d il=%d last=%04X) r0=%08X r1=%08X r2=%08X r15=%08X\n",
                    st + 1, r.opcode, r.addr, int(r.illegal), ntrap, nldtlb, ncache,
                    nsleep, nillegal, last_illegal, s.r[0], s.r[1], s.r[2], s.r[15]);
    }
    for (unsigned k = 0; k < 16; ++k) std::printf("r%u=%08X ", k, s.r[k]);
    std::printf("\n");
    for (unsigned k = 8; k < 16; ++k) std::printf("rbank%u=%08X ", k - 8, s.rbank[k - 8]);
    std::printf("\n");
    std::printf("mach=%08X macl=%08X pr=%08X pc=%08X sr=%08X fpscr=%08X fpul=%08X\n",
                s.mach, s.macl, s.pr, s.pc, s.sr, s.fpscr, s.fpul);
    std::printf("gbr=%08X vbr=%08X ssr=%08X spc=%08X sgr=%08X dbr=%08X T=%d\n",
                s.gbr, s.vbr, s.ssr, s.spc, s.sgr, s.dbr, int(s.t()));
    for (unsigned k = 0; k < 16; ++k) std::printf("fr%u=%08X ", k, s.fr[k]);
    std::printf("\n");
    for (unsigned k = 0; k < 16; ++k) std::printf("xf%u=%08X ", k, s.xf[k]);
    std::printf("\n");
    for (int k = 0; k < nreads; ++k)
        std::printf("read%s (@0x%s) = %08X\n", reads[k], reads[k], bus.read32(val(reads[k])));
    return 0;
}
