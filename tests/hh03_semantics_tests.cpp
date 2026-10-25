// SPDX-License-Identifier: MIT
// HH03 - testes semanticos do nucleo SH-4 (executor completo).
//
// Fontes dos fatos: oraculo SLEIGH SuperH4 (semantica e gravuras, via
// analysis/hh03/sleigh_truth2.txt + probes assimetricos registrados),
// manual Renesas SH-4 e referencia MIT C:\Source\recomp-BStinger
// (so leitura). Nenhum dado do jogo aqui: fatos de ISA e rotinas de
// divisao do manual com expectativa matematica exata.
#include "sh4/cpu.hpp"

#include <bit>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <vector>

using namespace hh::sh4;

static int g_failed = 0;
static uint16_t g_op = 0; // op em teste (setado nos lops formula-a-formula)
#define CHECK(name, cond)                                                        \
    do {                                                                         \
        if (!(cond)) {                                                           \
            std::printf("FALHA: %s (linha %d)\n", name, __LINE__);               \
            ++g_failed;                                                          \
        }                                                                        \
    } while (0)
#define CHECK_OP(name, cond, opv)                                                \
    do {                                                                         \
        if (!(cond)) {                                                           \
            std::printf("FALHA: %s (linha %d) op=%04X\n", name, __LINE__,        \
                        unsigned(opv));                                          \
            ++g_failed;                                                          \
        }                                                                        \
    } while (0)

namespace {

// Barramento plano: 1 MB espelhado, le/grava little-endian, mascara 20 bits.
class TestBus final : public Sh4Bus {
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

constexpr uint32_t kB = 0x90000; // PC base de todos os programas de teste.

// ---------- encodings de 16 bits (fixados no oraculo HH03) ----------
constexpr uint16_t MOV_I(unsigned n, unsigned imm) {
    return uint16_t(0xE000 | (n << 8) | (imm & 0xFF)); // mov #imm,rn
}
constexpr uint16_t MOV_RR(unsigned n, unsigned m) {
    return uint16_t(0x6003 | (n << 8) | (m << 4)); // mov Rm,Rn
}
// grava registrador -> memoria: 0010 nnnn mmmm sz (0/1/2) ou predec 4/5/6
constexpr uint16_t ST_(unsigned n, unsigned m, unsigned low) {
    return uint16_t(0x2000 | (n << 8) | (m << 4) | low);
}
// carrega memoria -> registrador: 0110 nnnn mmmm sz (0/1/2) ou posinc 4/5/6
constexpr uint16_t LD_(unsigned n, unsigned m, unsigned low) {
    return uint16_t(0x6000 | (n << 8) | (m << 4) | low);
}
// @(R0,Rm): carga 0x0nmC..E; grava 0x0nm4..6
constexpr uint16_t LD_I0(unsigned n, unsigned m, unsigned sz) {
    return uint16_t((n << 8) | (m << 4) | (0xC + sz));
}
constexpr uint16_t ST_I0(unsigned n, unsigned m, unsigned sz) {
    return uint16_t((n << 8) | (m << 4) | (0x4 + sz));
}
// disp .l com Rn/Rm livres: grava 0x1nmd (disp bits 3-0 x4), carrega 0x5nmd
constexpr uint16_t ST_DL(unsigned n, unsigned m, unsigned d) {
    return uint16_t(0x1000 | (n << 8) | (m << 4) | (d & 0xF));
}
constexpr uint16_t LD_DL(unsigned n, unsigned m, unsigned d) {
    return uint16_t(0x5000 | (n << 8) | (m << 4) | (d & 0xF));
}
// disp .b/.w com dado R0 fixo: (m bits, soma de campo no byte)
constexpr uint16_t ST_DB(unsigned m, unsigned d) { return uint16_t(0x8000 | (m << 4) | (d & 0xF)); }
constexpr uint16_t ST_DW(unsigned m, unsigned d) { return uint16_t(0x8100 | (m << 4) | (d & 0xF)); }
constexpr uint16_t LD_DB(unsigned m, unsigned d) { return uint16_t(0x8400 | (m << 4) | (d & 0xF)); }
constexpr uint16_t LD_DW(unsigned m, unsigned d) { return uint16_t(0x8500 | (m << 4) | (d & 0xF)); }
// GBR: disp = byte inteiro; k = 0xC0/1/2 grava, 0xC4/5/6 carrega
constexpr uint16_t GB_(unsigned k, unsigned d) { return uint16_t((k << 8) | (d & 0xFF)); }
// imediatos: add 0x7nim; cmp/eq 0x88im; tst/and/xor/or.b 0xC8|k<<8|im (k C..F);
// addimm em Rn 0x7n; operacoes com R0: tst 0xC8, and 0xC9, xor 0xCA, or 0xCB
constexpr uint16_t ADDI(unsigned n, unsigned imm) {
    return uint16_t(0x7000 | (n << 8) | (imm & 0xFF));
}
constexpr uint16_t CMPEQ_I(unsigned imm) { return uint16_t(0x8800 | (imm & 0xFF)); }
constexpr uint16_t IMM0(unsigned k, unsigned imm) { // k: 8 tst, C..F bops
    return uint16_t(0xC000 | (k << 8) | (imm & 0xFF));
}
// binarias registrador/reg: escopo 3: 0 eq, 2 hs, 3 ge, 6 hi, 7 gt, 8 sub,
// A subc, B subv, C add, D dmuls, E addc, F addv; escopo 2: 7 div0s,
// 8 tst, 9 and, A xor, B or, C cmp/str, D xtrct, E mulu.w, F muls.w;
// escopo 6: 3 mov, 7 not, 8 swap.b, 9 swap.w, A negc, B neg,
// C extu.b, D extu.w, E exts.b, F exts.w
constexpr uint16_t RR3(unsigned n, unsigned m, unsigned low) {
    return uint16_t(0x3000 | (n << 8) | (m << 4) | low);
}
constexpr uint16_t RR2(unsigned n, unsigned m, unsigned low) {
    return uint16_t(0x2000 | (n << 8) | (m << 4) | low);
}
constexpr uint16_t RR6(unsigned n, unsigned m, unsigned low) {
    return uint16_t(0x6000 | (n << 8) | (m << 4) | low);
}
// 0x4n: 00 shll, 01 shlr, 04 rotl, 05 rotr, 08 shll2, 09 shlr2, 10 dt,
// 11 cmp/pz, 15 cmp/pl, 18 shll8, 19 shlr8, 20 shal, 21 shar, 24 rotcl,
// 25 rotcr, 28 shll16, 29 shlr16; shad 0x4nmC, shld 0x4nmD
constexpr uint16_t OP4(unsigned n, unsigned low) { return uint16_t(0x4000 | (n << 8) | low); }
constexpr uint16_t DIV1(unsigned n, unsigned m) { return uint16_t(0x3004 | (n << 8) | (m << 4)); }
constexpr uint16_t MAC_(unsigned major, unsigned n, unsigned m) { // 0x0nmF / 0x4nmF
    return uint16_t(major << 12 | (n << 8) | (m << 4) | 0xF);
}
constexpr uint16_t NOP = 0x0009u;

const char* mnem(uint16_t op) {
    const InstrInfo* r = lookup(op);
    return r ? r->mnemonic : "?ILEGAL?";
}

struct Hooks {
    unsigned trapa = 0, ldtlb = 0, cache = 0, sleep = 0, illegal = 0;
    uint32_t trap_imm = 0, cache_addr = 0, cache_kind = 0, illegal_op = 0;
};

// Hooks no g_hooks global; os lambdas consultam o ponteiro global.
static Hooks* g_hooks = nullptr;

// Estado fresco com hooks instalados. O teste aponta g_hooks antes do step.
Sh4State fresh(TestBus&, uint32_t entry = kB) {
    Sh4State s;
    s.on_trapa = [](Sh4State&, uint32_t imm) { ++g_hooks->trapa; g_hooks->trap_imm = imm; };
    s.on_ldtlb = [](Sh4State&) { ++g_hooks->ldtlb; };
    s.on_cache = [](Sh4State&, uint32_t a, uint32_t k) {
        ++g_hooks->cache;
        g_hooks->cache_addr = a;
        g_hooks->cache_kind = k;
    };
    s.on_sleep = [](Sh4State&) { ++g_hooks->sleep; };
    s.on_illegal = [](Sh4State&, uint32_t, uint16_t op) {
        ++g_hooks->illegal;
        g_hooks->illegal_op = op;
    };
    s.reset(entry);
    return s;
}
} // namespace

// xorshift32 deterministico (nada de aleatoriedade externa).
uint32_t rnd32(uint32_t& st) {
    st ^= st << 13;
    st ^= st >> 17;
    st ^= st << 5;
    return st;
}

// (o fget ficou fora de uso; fb/FU abaixo cobrem os bits de float)
// ---------- 1. mov familia / memoria ----------
void testMovMemSizes(TestBus& bus) {
    Hooks h;
    g_hooks = &h;
    { // grava/le .l direto
        Sh4State s = fresh(bus);
        bus.write16(kB, MOV_I(1, 0x50));      // mov #0x50,r1
        bus.write16(kB + 2, MOV_I(14, 0x30)); // r14 = 0x30
        bus.write16(kB + 4, OP4(14, 0x28));   // shll16 r14 -> 0x300000
        bus.write16(kB + 6, ST_(14, 1, 2));   // mov.l r1,@r14
        for (int i = 0; i < 4; ++i) s.step(bus);
        CHECK("mov.l store", bus.read32(0x300000) == 0x50);
        bus.write16(kB + 8, LD_(2, 14, 2)); // mov.l @r14,r2
        s.step(bus);
        CHECK("mov.l load", s.r[2] == 0x50);
        CHECK("pc apos 5 passos", s.pc == kB + 10);
    }
    { // .b e .w com extensao de sinal
        Sh4State s = fresh(bus);
        s.r[5] = 0x8081;     // byte baixo 0x81
        s.r[6] = 0xFEAB;     // meia-palavra 0xFEAB
        s.r[14] = 0x4000;
        bus.write16(kB, ST_(14, 5, 0));     // mov.b r5,@r14
        bus.write16(kB + 2, LD_(7, 14, 0)); // mov.b @r14,r7
        s.step(bus);
        CHECK("mov.b grava byte", bus.read8(0x4000) == 0x81);
        s.step(bus);
        CHECK("mov.b load sext", s.r[7] == 0xFFFFFF81);
        bus.write16(kB + 4, ST_(14, 6, 1)); // mov.w r6,@r14
        bus.write16(kB + 6, LD_(8, 14, 1)); // mov.w @r14,r8
        s.step(bus); // o store abaixo de 0x8000 sobrescreve o byte 0x4000
        s.step(bus);
        CHECK("mov.w load sext", s.r[8] == 0xFFFFFEAB);
    }
    { // posinc load / predec store
        Sh4State s = fresh(bus);
        s.r[14] = 0x4000;
        s.r[15] = 0x4100;
        s.r[3] = 0xCAFEBABE;
        bus.write32(0x4000, 0x11223344);
        bus.write16(kB, LD_(2, 14, 6));    // mov.l @r14+,r2
        bus.write16(kB + 2, ST_(15, 2, 6)); // mov.l r2,@-r15
        s.step(bus); s.step(bus);
        CHECK("posinc le e avanca 4", s.r[2] == 0x11223344 && s.r[14] == 0x4004);
        CHECK("predec grava em r15-4", bus.read32(0x40FC) == 0x11223344 && s.r[15] == 0x40FC);
    }
    { // @(R0,Rm) nas duas direcoes, tres tamanhos
        Sh4State s = fresh(bus);
        s.r[0] = 0x100;
        s.r[12] = 0x3000;
        s.r[13] = 0x2000;
        s.r[5] = 0x5A5A00AA;
        bus.write16(kB, ST_I0(12, 5, 2));     // mov.l r5,@(r0,r12)
        bus.write16(kB + 2, LD_I0(11, 12, 2)); // mov.l @(r0,r12),r11 (mesma base)
        s.step(bus); s.step(bus);
        CHECK("st @(R0,Rn) .l", bus.read32(0x3100) == 0x5A5A00AA);
        CHECK("ld @(R0,Rm) .l", s.r[11] == 0x5A5A00AA);
        s.r[5] = 0x7F;
        s.r[4] = 0x80F0;
        bus.write16(kB + 4, ST_I0(12, 5, 0)); // .b
        bus.write16(kB + 6, LD_I0(9, 12, 0));
        bus.write16(kB + 8, ST_I0(12, 4, 1)); // .w
        bus.write16(kB + 10, LD_I0(10, 12, 1));
        for (int i = 0; i < 4; ++i) s.step(bus);
        CHECK("st/ld @(R0,Rn) .b", s.r[9] == 0x7F);
        CHECK("st/ld @(R0,Rn) .w", s.r[10] == 0xFFFF80F0); // load .w faz sext
    }
    { // disp .l com campo livre
        Sh4State s = fresh(bus);
        s.r[13] = 0x5000;
        s.r[9] = 0x1A2B3C4D;
        bus.write16(kB, ST_DL(13, 9, 3)); // mov.l r9,@(12,r13)
        bus.write16(kB + 2, LD_DL(8, 13, 3)); // mov.l @(12,r13),r8 (mesmo disp)
        s.step(bus); s.step(bus);
        CHECK("st disp .l", bus.read32(0x500C) == 0x1A2B3C4D);
        CHECK("ld disp .l", s.r[8] == 0x1A2B3C4D);
    }
    { // disp .b/.w com dado R0 fixo (dado sempre de r0)
        Sh4State s = fresh(bus);
        s.r[0] = 0x777700AA;
        s.r[14] = 0x6000; // bits 7-4 do byte de endereco
        s.r[12] = 0x6100;
        bus.write16(kB, ST_DB(14, 5));  // mov.b r0,@(5,r14) -> 0x6005
        bus.write16(kB + 2, ST_DW(12, 3)); // mov.w r0,@(6,r12) -> 0x6106
        s.step(bus); s.step(bus);
        CHECK("st disp .b dado R0", bus.read8(0x6005) == 0xAA);
        CHECK("st disp .w dado R0", bus.read16(0x6106) == 0x00AA);
        bus.write8(0x6007, 0x80);
        bus.write16(kB + 4, LD_DB(14, 7)); // mov.b @(7,r14),r0
        s.step(bus);
        CHECK("ld disp .b sext em R0", s.r[0] == 0xFFFFFF80);
        bus.write16(0x6010, 0x0001);
        bus.write16(kB + 6, LD_DW(14, 8)); // mov.w @(8,r14),r0 -> 0x6010 (disp*2)
        s.step(bus);
        CHECK("ld disp .w", s.r[0] == 0x0001);
    }
    { // GBR: disp = byte inteiro (multiply por tamanho), dado/dest R0
        Sh4State s = fresh(bus);
        s.gbr = 0x20000;
        s.r[0] = 0x12345678;
        bus.write16(kB, GB_(0xC0, 0x14));     // mov.b r0,@(20,GBR) -> 0x20014
        bus.write16(kB + 2, GB_(0xC1, 0x15)); // mov.w r0,@(42,GBR) -> 0x2002A
        bus.write16(kB + 4, GB_(0xC2, 0x16)); // mov.l r0,@(88,GBR) -> 0x20058
        for (int i = 0; i < 3; ++i) s.step(bus);
        CHECK("gbr st .b", bus.read8(0x20014) == 0x78);
        CHECK("gbr st .w", bus.read16(0x2002A) == 0x5678);
        CHECK("gbr st .l", bus.read32(0x20058) == 0x12345678);
        bus.write8(0x20018, 0x90);
        bus.write32(0x20068, 0xDEADBEEF);
        bus.write16(kB + 6, GB_(0xC4, 0x18)); // mov.b @(24,GBR),r0
        bus.write16(kB + 8, GB_(0xC5, 0x15)); // mov.w @(42,GBR),r0
        bus.write16(kB + 10, GB_(0xC6, 0x1A)); // mov.l @(104,GBR),r0
        // Um passo por carga: cada check logo apos o passo dele (r0 e
        // sobrescrito pela carga seguinte).
        s.step(bus);
        CHECK("gbr ld .b sext", s.r[0] == 0xFFFFFF90);
        s.step(bus);
        CHECK("gbr ld .w", s.r[0] == 0x5678);
        s.step(bus);
        CHECK("gbr ld .l", s.r[0] == 0xDEADBEEF);
    }
    { // PC-relativo: mov.w/mov.l/mova
        Sh4State s = fresh(bus);
        bus.write32(kB + 0x24, 0xCAFED00D);
        bus.write16(kB + 0x14, 0xBEEF);
        bus.write16(kB, 0xD108); // mov.l @(8*4,PC),r1 -> 0x90024 (base alinhada)
        bus.write16(kB + 2, 0x9207); // mov.w @(7*2,PC),r2 -> A+4+14 = 0x90014
        bus.write16(kB + 4, 0xC70A); // mova @(10*4,PC),r0 -> (A&~3)+4+40
        for (int i = 0; i < 3; ++i) s.step(bus);
        CHECK("mov.l (PC) absoluto", s.r[1] == 0xCAFED00D);
        CHECK("mov.w (PC) sext", s.r[2] == 0xFFFFBEEF);
        CHECK("mova base alinhada", s.r[0] == kB + 0x30); // op em A+4: (A+4)+4+40
    }
}

// ---------- 2. bops @(R0,GBR) ----------
void testBopGbr(TestBus& bus) {
    Hooks h;
    g_hooks = &h;
    Sh4State s = fresh(bus);
    s.gbr = 0x70000;
    s.r[0] = 0x40;
    bus.write8(0x70040, 0xF0);
    bus.write16(kB, IMM0(0xD, 0x0F)); // and.b #0x0f,@(r0,GBR): 0xF0 & 0x0F = 0x00
    bus.write16(kB + 2, IMM0(0xE, 0x30)); // xor.b #0x30: 0x00 ^ 0x30 = 0x30
    bus.write16(kB + 4, IMM0(0xF, 0x80)); // or.b #0x80: 0x30 | 0x80 = 0xB0
    bus.write16(kB + 6, IMM0(0xC, 0x18)); // tst.b #0x18: 0xB0 & 0x18 = 0x10 -> T=0
    s.pc = kB;
    s.step(bus); // and.b
    CHECK("and.b", bus.read8(0x70040) == 0x00);
    s.step(bus); // xor.b
    CHECK("xor.b", bus.read8(0x70040) == 0x30);
    s.step(bus); // or.b
    s.step(bus); // tst.b
    CHECK("or.b e tst.b T=0", bus.read8(0x70040) == 0xB0 && !s.t());
    bus.write8(0x70040, 0x18);
    bus.write16(kB, IMM0(0xC, 0x18)); // intersecao != 0 -> T=0
    s.pc = kB;
    s.step(bus);
    CHECK("tst.b intersecao T=0", !s.t());
}

// ---------- 3. aritmetica com carry/overflow/comparacoes ----------
void testAritCarry(TestBus& bus) {
    Hooks h;
    g_hooks = &h;
    Sh4State s = fresh(bus);
    s.r[0] = 0xFFFFFFFF;
    s.r[1] = 1;
    s.set_t(false);
    bus.write16(kB, RR3(0, 1, 0xE)); // addc r1,r0 -> 0 + carry
    s.step(bus);
    CHECK("addc wrap T=1", s.r[0] == 0 && s.t());
    s.set_t(true);
    s.r[0] = 0;
    s.r[1] = 0;
    bus.write16(kB, RR3(0, 1, 0xE)); // 0+0+T = 1, carry apagado
    s.pc = kB;
    s.step(bus);
    CHECK("addc propaga e apaga T", s.r[0] == 1 && !s.t());
    s.r[0] = 0;
    s.r[1] = 1;
    s.set_t(false);
    bus.write16(kB, RR3(0, 1, 0xA)); // subc r1,r0 -> borrow
    s.pc = kB;
    s.step(bus);
    CHECK("subc borrow T=1", s.r[0] == 0xFFFFFFFF && s.t());
    s.r[0] = 0;
    s.set_t(true);
    bus.write16(kB, RR6(0, 0, 0xA)); // negc r0,r0: 0-T-0 -> 0xFFFFFFFF, empresta
    s.pc = kB;
    s.step(bus);
    CHECK("negc com T=1", s.r[0] == 0xFFFFFFFF && s.t());
    s.r[0] = 5;
    s.set_t(false);
    bus.write16(kB, RR6(0, 0, 0xA)); // negc: -5, mas o emprestimo em T=1
    s.pc = kB;
    s.step(bus);
    CHECK("negc sem T (T esgota)", s.r[0] == 0xFFFFFFFB && s.t());
    s.r[1] = 7;
    bus.write16(kB, RR6(1, 1, 0xB)); // neg r1,r1
    s.pc = kB;
    s.step(bus);
    CHECK("neg", s.r[1] == 0xFFFFFFF9);
    s.r[3] = 0x7FFFFFFF;
    s.r[4] = 1;
    bus.write16(kB, RR3(3, 4, 0xF)); // addv r4,r3 -> overflow
    s.pc = kB;
    s.step(bus);
    CHECK("addv overflow T=1", s.r[3] == 0x80000000 && s.t());
    s.set_t(false);
    s.r[5] = 0x80000000;
    s.r[6] = 1;
    bus.write16(kB, RR3(5, 6, 0xB)); // subv r6,r5 -> overflow
    s.pc = kB;
    s.step(bus);
    CHECK("subv overflow T=1", s.r[5] == 0x7FFFFFFF && s.t());

    struct C4 { uint32_t a, b; int eq, hs, ge, hi, gt; };
    const C4 v[] = {
        {3, 4, 0, 0, 0, 0, 0},
        {4, 3, 0, 1, 1, 1, 1},
        {4, 4, 1, 1, 1, 0, 0},
        {0xFFFFFFFF, 1, 0, 1, 0, 1, 0},
        {1, 0xFFFFFFFF, 0, 0, 1, 0, 1},
    };
    const unsigned low[5] = {0, 2, 3, 6, 7};
    for (const C4& c : v) {
        s.r[2] = c.a;
        s.r[3] = c.b;
        const bool want[5] = {c.eq != 0, c.hs != 0, c.ge != 0, c.hi != 0, c.gt != 0};
        for (int k = 0; k < 5; ++k) {
            bus.write16(kB, RR3(2, 3, low[k]));
            s.pc = kB;
            s.step(bus);
            CHECK("cmp quad", s.t() == want[k]);
        }
    }
    s.r[0] = 0x0FF0;
    s.set_t(true);
    bus.write16(kB, IMM0(0x8, 0x10)); // tst #0x10,R0: 0xFF0&0x10 != 0 -> T=0
    s.pc = kB;
    s.step(bus);
    CHECK("tst imm intersecao T=0", !s.t());
    s.r[0] = 0x0100;
    bus.write16(kB, IMM0(0x8, 0x20)); // sem intersecao -> T=1
    s.pc = kB;
    s.step(bus);
    CHECK("tst imm distinto T=1", s.t());
    s.r[0] = 0xFF;
    bus.write16(kB, IMM0(0x9, 0x03)); // and #3,R0 -> 0x03
    bus.write16(kB + 2, IMM0(0xA, 0x5A)); // xor #0x5A,R0 -> 0x59
    bus.write16(kB + 4, IMM0(0xB, 0x5A)); // or #0x5A,R0 -> 0x5B
    s.pc = kB;
    s.step(bus); s.step(bus); s.step(bus);
    CHECK("and/xor/or imm em R0", s.r[0] == 0x5B);
    s.r[0] = 7;
    s.r[1] = 7;
    bus.write16(kB, CMPEQ_I(7)); // cmp/eq #7,R0 -> T=1
    s.pc = kB;
    s.step(bus);
    CHECK("cmp/eq imm acerto", s.t());
    s.r[0] = 0xFFFFFFFB; // -5 sext em cima de imm 0xFB? imm sext: cmp/eq #-5
    bus.write16(kB, CMPEQ_I(0xFB));
    s.pc = kB;
    s.step(bus);
    CHECK("cmp/eq imm sext", s.t());
    s.r[4] = 0xFFFF8FA5;
    bus.write16(kB, RR6(6, 4, 0xC)); // extu.b r4,r6 -> 0xA5
    bus.write16(kB + 2, RR6(7, 4, 0xD)); // extu.w r4,r7 -> 0x8FA5
    bus.write16(kB + 4, RR6(8, 4, 0xE)); // exts.b r4,r8 -> 0xFFFFFFA5
    bus.write16(kB + 6, RR6(9, 4, 0xF)); // exts.w r4,r9 -> 0xFFFF8FA5
    bus.write16(kB + 8, RR6(10, 4, 0x7)); // not r4,r10 -> 0x0000705A
    s.pc = kB;
    for (int i = 0; i < 5; ++i) s.step(bus);
    CHECK("extu.b/extu.w", s.r[6] == 0xA5 && s.r[7] == 0x8FA5);
    CHECK("exts.b/exts.w", s.r[8] == 0xFFFFFFA5 && s.r[9] == 0xFFFF8FA5);
    CHECK("not", s.r[10] == 0x0000705A);
    s.r[11] = 1;
    bus.write16(kB, OP4(11, 0x10)); // dt r11
    s.pc = kB;
    s.step(bus);
    CHECK("dt zera e T=1", s.r[11] == 0 && s.t());
}

// ---------- 4. swap.b / swap.w / xtrct ----------
void testSwapXtrct(TestBus& bus) {
    Hooks h;
    g_hooks = &h;
    Sh4State s = fresh(bus);
    s.r[1] = 0x12345678;
    bus.write16(kB, RR6(2, 1, 0x8)); // swap.b r1,r2 -> 0x1234_7856
    bus.write16(kB + 2, RR6(3, 1, 0x9)); // swap.w r1,r3 -> 0x5678_1234
    bus.write16(kB + 4, RR2(4, 1, 0xD)); // xtrct r1,r4: r4 = (r1<<16)|(r4>>16)
    s.r[4] = 0x0000ABCD;
    s.pc = kB;
    s.step(bus); s.step(bus); s.step(bus);
    CHECK("swap.b", s.r[2] == 0x12347856);
    CHECK("swap.w", s.r[3] == 0x56781234);
    CHECK("xtrct", s.r[4] == 0x56780000);
}

// ---------- 5. shifts e rotacoes ----------
void testShiftsRots(TestBus& bus) {
    Hooks h;
    g_hooks = &h;
    Sh4State s = fresh(bus);

    // shad: s em r[m]; s >= 0 desloca a esquerda; s < 0 a direita com sinal.
    // Encodings literais (n=1, m=2): 0x4nnC ja carrega n em bits 11-8; ORs
    // adicionais em cima do helper OP4 duplicam o campo do destino.
    s.r[1] = 0x80000000;
    s.r[2] = 4;
    bus.write16(kB, 0x412C); // shad r2,r1: 0x80000000 << 4
    s.pc = kB;
    s.step(bus);
    CHECK("shad positivo", s.r[1] == 0);
    s.r[1] = 0x80000000;
    s.r[2] = 0xFFFFFFFC; // -4: ARS sobre numero negativo -> entra 1
    bus.write16(kB, 0x412C);
    s.pc = kB;
    s.step(bus);
    CHECK("shad negativo em negativo", s.r[1] == 0xF8000000);
    s.r[1] = 0x40000000;
    s.r[2] = 0xFFFFFFE0; // -32: shad com s=-32 -> preenche com bit de sinal
    bus.write16(kB, 0x412C);
    s.pc = kB;
    s.step(bus);
    CHECK("shad s=-32", s.r[1] == 0);
    s.r[1] = 0x40000000;
    s.r[2] = 32;
    bus.write16(kB, 0x412C);
    s.pc = kB;
    s.step(bus);
    // s=32: o campo usado sao os bits 4-0 (s5 = 0) -> sem deslocamento.
    // Fontes: MIT BS_OP(SHAD) e SLEIGH SuperH4.sinc (ambos "s & 0x1f").
    CHECK("shad s=32", s.r[1] == 0x40000000);

    // shld: logico; s = -32 -> 0; s<0 usa (~s+1)&31
    s.r[1] = 0x80000001;
    s.r[2] = 0xFFFFFFFC; // -4: SHR logico 4
    bus.write16(kB, 0x412D); // shld r2,r1
    s.pc = kB;
    s.step(bus);
    CHECK("shld negativo", s.r[1] == 0x08000000);
    s.r[1] = 0x80000001;
    s.r[2] = 0xFFFFFFE0; // -32 -> 0
    bus.write16(kB, 0x412D);
    s.pc = kB;
    s.step(bus);
    CHECK("shld s=-32 -> 0", s.r[1] == 0);
    s.r[1] = 0x0000FFFF;
    s.r[2] = 16;
    bus.write16(kB, 0x412D);
    s.pc = kB;
    s.step(bus);
    CHECK("shld positivo x16", s.r[1] == 0xFFFF0000);

    // shll/shlr e multiplos
    s.r[1] = 0x80000001;
    bus.write16(kB, OP4(1, 0x00)); // shll r1 -> 0x00000002, T=1
    bus.write16(kB + 2, OP4(1, 0x01)); // shlr r1 -> 0x00000001, T <- bit0=0
    s.pc = kB;
    s.step(bus); s.step(bus);
    CHECK("shll/shlr", s.r[1] == 1 && !s.t());
    s.r[2] = 0x000000AA;
    bus.write16(kB, OP4(2, 0x08)); // shll2 r2 -> 0x2A8
    bus.write16(kB + 2, OP4(2, 0x18)); // shll8 r2 -> 0x2A800
    bus.write16(kB + 4, OP4(2, 0x28)); // shll16 r2 -> 0xA8000000 (0x2A800<<16
                                       // estouro modulo 2^32, mesmo reg)
    s.pc = kB;
    for (int i = 0; i < 3; ++i) s.step(bus);
    CHECK("shll2/8/16", s.r[2] == 0xA8000000);
    s.r[5] = 0x80000001;
    bus.write16(kB, OP4(5, 0x09)); // shlr2: 0x20000000
    bus.write16(kB + 2, OP4(5, 0x19)); // shlr8: 0x00200000
    bus.write16(kB + 4, OP4(5, 0x29)); // shlr16: 0x00000020
    s.pc = kB;
    for (int i = 0; i < 3; ++i) s.step(bus);
    CHECK("shlr2/8/16", s.r[5] == 0x20);

    // shal/shar com sinal
    s.r[6] = 0xC0000000;
    bus.write16(kB, OP4(6, 0x20)); // shal: 0x80000000, T=1 (bit31 sai)
    bus.write16(kB + 2, OP4(6, 0x21)); // shar: ARS de 0x80000000 -> 0xC0000000
    s.pc = kB;
    s.step(bus); s.step(bus);
    CHECK("shal/shar", s.r[6] == 0xC0000000 && !s.t());

    // rotl/rotr com T passando pelo bit
    s.r[7] = 0x00000001;
    bus.write16(kB, OP4(7, 0x04)); // rotl: bit0 <- T (0)
    s.pc = kB;
    s.step(bus);
    CHECK("rotl", s.r[7] == 2 && !s.t());
    s.r[8] = 0x00000001;
    s.set_t(true);
    bus.write16(kB, OP4(8, 0x05)); // rotr: bit31 <- T, T <- bit0
    s.pc = kB;
    s.step(bus);
    CHECK("rotr com T", s.r[8] == 0x80000000 && s.t());

    // rotcl/rotcr com T
    s.r[9] = 0x00000001;
    s.set_t(true);
    bus.write16(kB, OP4(9, 0x24)); // rotcl: bit0 <- T=1, T <- bit31=0 -> 3
    s.pc = kB;
    s.step(bus);
    CHECK("rotcl com T=1", s.r[9] == 3 && !s.t());
    s.set_t(false);
    bus.write16(kB, OP4(9, 0x25)); // rotcr: bit31 <- T=0, T <- bit0=1 -> 1
    s.pc = kB;
    s.step(bus);
    CHECK("rotcr T sai do bit0", s.r[9] == 1 && s.t());
}

// ---------- 6. multiplicacoes: fatos de produto ----------
void testMulMacFacts(TestBus& bus) {
    Hooks h;
    g_hooks = &h;
    Sh4State s = fresh(bus);

    // mul.l: grava SOMENTE MACL (MACH intocado; evidencia dupla:
    // MIT BS_OP(MULL) e SLEIGH MACL = temp; o manual diverge e perde).
    s.mach = 0x1357;
    s.r[1] = 0xFFFFFFFE; // -2
    s.r[2] = 3;
    bus.write16(kB, 0x0127); // mul.l r2,r1: 0x0nm7 (n=1,m=2)
    s.pc = kB;
    s.step(bus);
    CHECK("mul.l -2*3", s.macl == 0xFFFFFFFA && s.mach == 0x1357);
    // muls.w: 16 bits com sinal (MACH tambem intocado)
    s.mach = 0x2468;
    s.r[1] = 0xFFFE;
    s.r[2] = 0x0003;
    bus.write16(kB, RR2(1, 2, 0xF)); // muls.w r2,r1
    s.pc = kB;
    s.step(bus);
    CHECK("muls.w -2*3", s.macl == 0xFFFFFFFA && s.mach == 0x2468);
    // mulu.w: 16 bits sem sinal: 0xFFFE * 3 = 0x2FFFA
    bus.write16(kB, RR2(1, 2, 0xE)); // mulu.w r2,r1
    s.pc = kB;
    s.step(bus);
    CHECK("mulu.w 65534*3", s.macl == 0x0002FFFA && s.mach == 0x2468);
    // dmuls.l: -2 * 3 = 0xFFFFFFFFFFFFFFFA
    s.mach = 0;
    s.macl = 0;
    s.r[1] = 0xFFFFFFFE;
    s.r[2] = 3;
    bus.write16(kB, RR3(1, 2, 0xD)); // dmuls.l r2,r1
    s.pc = kB;
    s.step(bus);
    CHECK("dmuls.l", s.mach == 0xFFFFFFFF && s.macl == 0xFFFFFFFA);
    // dmulu.l: 0xFFFFFFFE * 3 = 0x2FFFFFFFA
    bus.write16(kB, RR3(1, 2, 0x5)); // dmulu.l r2,r1
    s.pc = kB;
    s.step(bus);
    CHECK("dmulu.l", s.mach == 0x00000002 && s.macl == 0xFFFFFFFA);
    // dmulu.l: 0x10000 * 0x10000 = 2^32 -> mach=1, macl=0
    s.r[1] = 0x00010000;
    s.r[2] = 0x00010000;
    bus.write16(kB, RR3(1, 2, 0x5));
    s.pc = kB;
    s.step(bus);
    CHECK("dmulu.l 2^32*2^32", s.mach == 1 && s.macl == 0);

    // mac.l S=0: UM produto por instrucao: a = @r14+ primeiro, b = @r15+
    {
        Sh4State s2 = fresh(bus);
        s2.r[14] = 0x4000;
        s2.r[15] = 0x4008;
        bus.write32(0x4000, 0xFFFFFFFE); // @r14+ = -2
        bus.write32(0x4008, 4);          // @r15+ = 4
        bus.write16(kB, MAC_(0, 14, 15)); // mac.l @r15+,@r14+ -> MAC += -2*4 = -8
        s2.pc = kB;
        s2.step(bus);
        CHECK("mac.l S=0", s2.macl == 0xFFFFFFF8 && s2.mach == 0xFFFFFFFF &&
                               s2.r[14] == 0x4004 && s2.r[15] == 0x400C);
    }

    // mac.w S=0: UM produto de meia-palavras com sinal: a = @r14+ (0x4000),
    // b = @r15+ (0x4004): 0x0003 * 0x0002 = 6
    {
        Sh4State s2 = fresh(bus);
        s2.r[14] = 0x4000;
        s2.r[15] = 0x4004;
        bus.write32(0x4000, 0x00000003); // halfword em 0x4000 = 3
        bus.write32(0x4004, 0x00000002); // halfword em 0x4004 = 2
        bus.write16(kB, MAC_(4, 14, 15)); // mac.w @r15+,@r14+
        s2.pc = kB;
        s2.step(bus);
        CHECK("mac.w S=0", s2.macl == 6 && s2.mach == 0 && s2.r[14] == 0x4002 &&
                               s2.r[15] == 0x4006);
    }
    // clrmac
    s.mach = 1;
    s.macl = 2;
    bus.write16(kB, 0x0028); // clrmac
    s.pc = kB;
    s.step(bus);
    CHECK("clrmac", s.mach == 0 && s.macl == 0);
}

// ---------- 7. saturacao de mac com S=1 ----------
void testMacSaturation(TestBus& bus) {
    Hooks h;
    g_hooks = &h;

    // mac.l S=1: UM produto por instrucao; o acumulador 64 bits satura a
    // 48 bits: kMax = 0x00007FFFFFFFFFFF, kMin = 0xFFFF800000000000.
    { // estouro positivo -> gruda em kMax
        Sh4State s = fresh(bus);
        s.set_sr(s.sr | kSrS);
        s.r[14] = 0x4000;
        s.r[15] = 0x4008;
        s.mach = 0x00007F00;
        s.macl = 0; // acumulado = 0x00007F0000000000
        bus.write32(0x4000, 0x7FFFFFFF);
        bus.write32(0x4008, 0x7FFFFFFF);
        bus.write16(kB, MAC_(0, 14, 15)); // produto = 0x3FFFFFFF00000001
        s.pc = kB;
        s.step(bus);
        CHECK("mac.l S=1 sat pos", s.mach == 0x00007FFF && s.macl == 0xFFFFFFFF);
    }
    { // estouro negativo -> gruda em kMin
        Sh4State s = fresh(bus);
        s.set_sr(s.sr | kSrS);
        s.r[14] = 0x4000;
        s.r[15] = 0x4008;
        s.mach = 0xFFFF8000;
        s.macl = 0; // acumulado = 0xFFFF800000000000 = kMin
        bus.write32(0x4000, 0xFFFFFFFE); // -2
        bus.write32(0x4008, 3);
        bus.write16(kB, MAC_(0, 14, 15)); // produto = -6 -> abaixo do minimo
        s.pc = kB;
        s.step(bus);
        CHECK("mac.l S=1 sat neg", s.mach == 0xFFFF8000 && s.macl == 0);
    }

    // mac.w S=1: saturacao SINAL de 32 bits sobre MACL em cada passo, com
    // fixacao permanente do bit0 de MACH - a contagem usa MACL sozinho.
    { // estouro positivo: macl = INT32_MAX + produto
        Sh4State s = fresh(bus);
        s.set_sr(s.sr | kSrS);
        s.r[14] = 0x4000;
        s.r[15] = 0x4004;
        s.mach = 0;
        s.macl = 0x7FFFFFFF;
        bus.write32(0x4000, 0x00007FFF); // a = 0x7FFF
        bus.write32(0x4004, 0x00007FFF); // b = 0x7FFF
        bus.write16(kB, MAC_(4, 14, 15)); // produto = 0x3FFF0001 -> estoura
        s.pc = kB;
        s.step(bus);
        CHECK("mac.w S=1 sat pos", s.macl == 0x7FFFFFFF && s.mach == 1);
        // passo 2 no mesmo estado: sem estouro (produto -6), MACH bit0 fica
        bus.write16(0x4002, 0xFFFD); // a = 0xFFFD = -3
        bus.write16(0x4006, 0x0002); // b = 2
        bus.write16(kB + 2, MAC_(4, 14, 15));
        s.step(bus);
        CHECK("mac.w MACH bit0 persiste", s.macl == 0x7FFFFFF9 && s.mach == 1);
    }
    { // estouro negativo: macl = INT32_MIN + produto negativo
        Sh4State s = fresh(bus);
        s.set_sr(s.sr | kSrS);
        s.r[14] = 0x4000;
        s.r[15] = 0x4004;
        s.mach = 0;
        s.macl = 0x80000000;
        bus.write32(0x4000, 0x0000FFFB); // a = -5
        bus.write32(0x4004, 0x00000004); // b = 4
        bus.write16(kB, MAC_(4, 14, 15)); // produto = -20 -> abaixo do minimo
        s.pc = kB;
        s.step(bus);
        CHECK("mac.w S=1 sat neg", s.macl == 0x80000000 && s.mach == 1);
    }
    { // sem S: estouro igual NAO satura (mantem 64 bits) - caso negativo
        Sh4State s = fresh(bus);
        s.r[14] = 0x4000;
        s.r[15] = 0x4004;
        s.mach = 0;
        s.macl = 0x7FFFFFFF;
        bus.write32(0x4000, 0x00007FFF);
        bus.write32(0x4004, 0x00007FFF);
        bus.write16(kB, MAC_(4, 14, 15));
        s.pc = kB;
        s.step(bus);
        // 0x7FFFFFFF + 0x3FFF0001 = 0xBFFF0000 mantido...
        CHECK("mac.w S=0 sem sat", s.macl == 0xBFFF0000 && s.mach == 0);
    }
}

// ---------- 8. rotinas de divisao do manual (fatos matematicos) ----------
// Rotinas ideneticas às dos testes MIT (fonte de leitura, test_ops.cpp),
// com xorshift local deterministico. Encodings e registradores fixados:
//   32/16 s/ sinal: r1=dividendo, r0=divisor<<16; div0u; 16x div1 r0,r1;
//   rotcl r1; extu.w r1,r1 -> r1 = dd/dv
//   64/32 s/ sinal: r1=hi, r2=lo, r0=divisor (exige r1 < divisor);
//   div0u; 32x [rotcl r2; div1 r0,r1]; rotcl r2 -> r2 = quociente
//   32/32 c/ sinal: r1=dd, r0=dv; mov r1,r2; rotcl r2; subc r2,r2;
//   xor r3,r3; subc r3,r1; div0s r0,r2; 32x [rotcl r1; div1 r0,r2];
//   rotcl r1; addc r3,r1 -> r1 = dd/dv (truncado)
void testDivRoutines(TestBus& bus) {
    Hooks h;
    g_hooks = &h;
    uint32_t seed = 0x12345678u;

    { // 32/16 unsigned
        Sh4State s = fresh(bus);
        for (int k = 0; k < 400; ++k) {
            const uint32_t d16 = (rnd32(seed) | 1) & 0xFFFF;
            // Quociente tem que caber em 16 bits: dd < d16 * 2^16 (a rotina
            // do manual so calcula 16 bits de quociente).
            const uint32_t dd = rnd32(seed) % (65536u * d16);
            s.reset(kB);
            s.r[1] = dd;
            s.r[0] = d16; // divisor de 16 bits
            // Rotina do manual (idem MIT test_ops.cpp): shll16 r0; div0u;
            // 16x div1 r0,r1; rotcl r1; extu.w r1. Cada op no seu slot.
            bus.write16(kB, 0x4028);
            bus.write16(kB + 2, 0x0019);
            for (int st = 0; st < 16; ++st)
                bus.write16(kB + 4 + 2 * st, DIV1(1, 0));
            bus.write16(kB + 36, OP4(1, 0x24));
            bus.write16(kB + 38, RR6(1, 1, 0xD));
            s.pc = kB;
            // 20 instrucoes no total (o extu.w final limpa os bits altos do
            // quociente; sem ele r1 sai com 0xFFF8xxxx Jr. probe).
            for (int st = 0; st < 20; ++st) s.step(bus);
            CHECK("div 32/16 unsigned", s.r[1] == dd / d16);
        }
    }
    { // 64/32 unsigned: r1:R2 / r0 -> r2
        Sh4State s = fresh(bus);
        const uint16_t rotcl_r2 = OP4(2, 0x24);
        const uint16_t dv1 = DIV1(1, 0);
        for (int k = 0; k < 400; ++k) {
            const uint32_t dv = rnd32(seed) | 1;
            const uint64_t dd = (uint64_t(rnd32(seed) % dv) << 32) | rnd32(seed);
            s.reset(kB);
            s.r[1] = uint32_t(dd >> 32);
            s.r[2] = uint32_t(dd);
            s.r[0] = dv;
            s.pc = kB;
            bus.write16(kB, 0x0019); // div0u
            s.step(bus);
            for (int st = 0; st < 32; ++st) {
                bus.write16(kB, rotcl_r2);
                s.pc = kB;
                s.step(bus);
                bus.write16(kB, dv1);
                s.pc = kB;
                s.step(bus);
            }
            bus.write16(kB, rotcl_r2);
            s.pc = kB;
            s.step(bus);
            const uint32_t want = uint32_t(dd / dv);
            CHECK("div 64/32 unsigned", s.r[2] == want);
        }
    }
    { // 32/32 signed
        Sh4State s = fresh(bus);
        const uint16_t rotcl_r1 = OP4(1, 0x24);
        const uint16_t dv1 = DIV1(2, 0); // div1 r0,r2
        for (int k = 0; k < 600; ++k) {
            int32_t dv = int32_t(rnd32(seed));
            if (k % 3 == 0) dv = int32_t(rnd32(seed) % 2001) - 1000;
            if (dv == 0) dv = 7;
            int32_t dd = int32_t(rnd32(seed));
            if (dd == INT32_MIN && dv == -1) continue;
            s.reset(kB);
            s.r[1] = uint32_t(dd);
            s.r[0] = uint32_t(dv);
            s.r[2] = 0;
            s.r[3] = 0;
            // Cada op no seu slot, sem sobrescrever o prologo das iteracoes
            // seguintes (o protocolo e identico ao do teste MIT):
            // mov r1,r2; rotcl r2; subc r2,r2; xor r3,r3; subc r3,r1;
            // div0s r0,r2; 32x[rotcl r1; div1 r0,r2]; rotcl r1; addc r3,r1.
            bus.write16(kB, 0x6213);
            bus.write16(kB + 2, 0x4224);
            bus.write16(kB + 4, 0x322A);
            bus.write16(kB + 6, 0x233A);
            bus.write16(kB + 8, 0x313A);
            bus.write16(kB + 10, 0x2207);
            for (int st = 0; st < 32; ++st) {
                bus.write16(kB + 12 + 4 * st, rotcl_r1);
                bus.write16(kB + 14 + 4 * st, dv1);
            }
            bus.write16(kB + 12 + 4 * 32, rotcl_r1);
            bus.write16(kB + 14 + 4 * 32, 0x313E); // addc r3,r1
            s.pc = kB;
            for (int st = 0; st < 72; ++st) s.step(bus);
            CHECK("div 32/32 signed", int32_t(s.r[1]) == dd / dv);
        }
    }
}

// ---------- 9. registradores de controle via formula ----------
// O mapa stc/sts/ldc/lds vem de probes assimetricos (e ja coberto pela
// tabela); aqui validamos a semantica formula a formula.
enum class K {
    sr, gbr, vbr, ssr, spc, sgr, dbr, mach, macl, pr, fpul, fpscr,
    bank0, bank1, bank2, bank3, bank4, bank5, bank6, bank7,
};

const uint32_t* creg_ptr(const Sh4State& s, K k) {
    switch (k) {
    case K::sr: return &s.sr;
    case K::gbr: return &s.gbr;
    case K::vbr: return &s.vbr;
    case K::ssr: return &s.ssr;
    case K::spc: return &s.spc;
    case K::sgr: return &s.sgr;
    case K::dbr: return &s.dbr;
    case K::mach: return &s.mach;
    case K::macl: return &s.macl;
    case K::pr: return &s.pr;
    case K::fpul: return &s.fpul;
    case K::fpscr: return &s.fpscr;
    case K::bank0: return &s.rbank[0];
    case K::bank1: return &s.rbank[1];
    case K::bank2: return &s.rbank[2];
    case K::bank3: return &s.rbank[3];
    case K::bank4: return &s.rbank[4];
    case K::bank5: return &s.rbank[5];
    case K::bank6: return &s.rbank[6];
    case K::bank7: return &s.rbank[7];
    }
    return &s.gbr;
}

void testControlRegs(TestBus& bus) {
    Hooks h;
    g_hooks = &h;

    struct CVec { uint16_t op; K k; };
    std::vector<CVec> vs;

    // Produto por registrador de destino/origem fixo r15.
    // stc: nibble bits 7-4 idx 0..4 pega SR/GBR/VBR/SSR/SPC; SGR = 0x3A;
    // DBR = 0xFA; bancos 0x82|i<<4.
    for (unsigned i = 0; i < 5; ++i) vs.push_back({uint16_t(0x0F02 | (i << 4)), K(i)});
    vs.push_back({0x0F3A, K::sgr});
    vs.push_back({0x0FFA, K::dbr});
    for (unsigned i = 0; i < 8; ++i) vs.push_back({uint16_t(0x0F82 | (i << 4)),(K(int(K::bank0) + i))});
    // stc.l: 0x4F03|i<<4; SGR 0x4F32; DBR 0x4FF2; bancos 0x4F83|i<<4
    for (unsigned i = 0; i < 5; ++i) vs.push_back({uint16_t(0x4F03 | (i << 4)), K(i)});
    vs.push_back({0x4F32, K::sgr});
    vs.push_back({0x4FF2, K::dbr});
    for (unsigned i = 0; i < 8; ++i) vs.push_back({uint16_t(0x4F83 | (i << 4)), (K(int(K::bank0) + i))});
    // sts: {0,1,2,5,6}
    const unsigned sidx[5] = {0, 1, 2, 5, 6};
    const K skey[5] = {K::mach, K::macl, K::pr, K::fpul, K::fpscr};
    for (int i = 0; i < 5; ++i) vs.push_back({uint16_t(0x0F0A | (sidx[i] << 4)), skey[i]});
    // sts.l: 0x4F02|idx<<4
    for (int i = 0; i < 5; ++i) vs.push_back({uint16_t(0x4F02 | (sidx[i] << 4)), skey[i]});
    // ldc: 0x4F0E|idx<<4 (5 primeiros); DBR 0x4FFA; bancos 0x4F8E|i<<4
    for (unsigned i = 0; i < 5; ++i) vs.push_back({uint16_t(0x4F0E | (i << 4)), K(i)});
    vs.push_back({0x4FFA, K::dbr});
    for (unsigned i = 0; i < 8; ++i) vs.push_back({uint16_t(0x4F8E | (i << 4)), (K(int(K::bank0) + i))});
    // ldc.l: 0x4F07|idx<<4; DBR 0x4FF6; bancos 0x4F87|i<<4
    for (unsigned i = 0; i < 5; ++i) vs.push_back({uint16_t(0x4F07 | (i << 4)), K(i)});
    vs.push_back({0x4FF6, K::dbr});
    for (unsigned i = 0; i < 8; ++i) vs.push_back({uint16_t(0x4F87 | (i << 4)), (K(int(K::bank0) + i))});
    // lds/lds.l para {0,1,2,5,6}
    for (int i = 0; i < 5; ++i) vs.push_back({uint16_t(0x4F0A | (sidx[i] << 4)), skey[i]});
    for (int i = 0; i < 5; ++i) vs.push_back({uint16_t(0x4F06 | (sidx[i] << 4)), skey[i]});

    for (const CVec& v : vs) {
        // valor magico distinto por registrador (evita colisao de assert).
        const uint32_t magic = 0xA0000000u + uint32_t(v.k) * 1717u +
                               (v.op & 0xFF) * 17u + uint32_t(v.op >> 8);
        const bool masked_reg = (v.k == K::sr || v.k == K::fpscr);
        const uint32_t mask = v.k == K::sr ? kSrWritable : kFpscrWritable;
        const uint32_t want = masked_reg ? (magic & mask) : magic;
        // Ops .l usam r15 como PONTEIRO DE PILHA (push/pop); os demais
        // movem o valor direto entre r15 e o creg. A leitura de um stc.l
        // nao deixa r15 igual ao creg (r15 vira 0x7FFC); a escrita de um
        // ldc.l le o valor da pilha, nao de r15. Cada protocolo mira a
        // familia certa (nome do mnemonico decide st*/ld*).
        const bool stacked = std::strstr(mnem(v.op), ".l") != nullptr;
        auto seed = [&](Sh4State& s) {
            switch (v.k) {
            case K::sr: s.set_sr(want); break;
            case K::fpscr: s.set_fpscr(want); break;
            case K::gbr: s.gbr = magic; break;
            case K::vbr: s.vbr = magic; break;
            case K::ssr: s.ssr = magic; break;
            case K::spc: s.spc = magic; break;
            case K::sgr: s.sgr = magic; break;
            case K::dbr: s.dbr = magic; break;
            case K::mach: s.mach = magic; break;
            case K::macl: s.macl = magic; break;
            case K::pr: s.pr = magic; break;
            case K::fpul: s.fpul = magic; break;
            default: s.rbank[int(v.k) - int(K::bank0)] = magic; break;
            }
        };

        CHECK("mnem creg", mnem(v.op) != "?ILEGAL?");

        if (strncmp(mnem(v.op), "st", 2) == 0) {
            if (!stacked) {
                // leitura via r15 (stc/sts): r15 <- creg
                Sh4State s = fresh(bus);
                seed(s);
                bus.write16(kB, v.op);
                s.pc = kB;
                s.step(bus);
                CHECK_OP("stc/sts -> r15", s.r[15] == *creg_ptr(s, v.k), v.op);
            } else {
                // push (stc.l/sts.l): creg -> mem[r15-4], r15 -= 4
                Sh4State s = fresh(bus);
                seed(s);
                s.r[15] = 0x8000;
                bus.write16(kB, v.op);
                s.pc = kB;
                s.step(bus);
                CHECK_OP("push creg", bus.read32(0x7FFC) == *creg_ptr(s, v.k) &&
                                      s.r[15] == 0x7FFC, v.op);
            }
        } else {
            if (!stacked) {
                // escrita via ldc/lds: creg <- r15
                Sh4State s = fresh(bus);
                s.r[15] = want;
                bus.write16(kB, v.op);
                s.pc = kB;
                s.step(bus);
                CHECK_OP("ldc/lds <- r15",
                         *creg_ptr(s, v.k) == (masked_reg ? want : magic), v.op);
            } else {
                // pop (ldc.l/lds.l): le @r15 (pos-incremento) e r15 += 4.
                // Continuacao natural do push acima: r15 empilhado de 0x8000
                // para 0x7FFC, o pop le [0x7FFC] e volta para 0x8000.
                Sh4State s = fresh(bus);
                s.r[15] = 0x7FFC;
                bus.write32(0x7FFC, masked_reg ? want : magic);
                bus.write16(kB, v.op);
                s.pc = kB;
                s.step(bus);
                CHECK_OP("pop creg", *creg_ptr(s, v.k) == (masked_reg ? want : magic) &&
                                     s.r[15] == 0x8000 && s.pc == kB + 2, v.op);
            }
        }
    }

    // ldc SR: bits NAO gravaveis nao mudam e sem troca de banco (magic sem
    // MD/RB diferente do reset)
    {
        Sh4State s = fresh(bus);
        s.r[2] = 0x1111;
        s.rbank[2] = 0x2222;
        s.r[15] = 0x70000123; // T=1,S=1,IMASK=2,Q=1 (MD/RB/BL intactos)
        bus.write16(kB, 0x4F0E); // ldc r15,SR
        s.pc = kB;
        s.step(bus);
        CHECK("ldc SR mascarado", s.sr == 0x70000123);
        CHECK("ldc SR sem flip de banco", s.r[2] == 0x1111 && s.rbank[2] == 0x2222);
    }

    // ldc FPSCR mascarado (24 bits inferiores) via lds
    {
        Sh4State s = fresh(bus);
        s.r[15] = 0x00040104; // RM=0 (muda de reset 01), DN=1
        bus.write16(kB, 0x4F6A); // lds r15,FPSCR
        s.pc = kB;
        s.step(bus);
        CHECK("lds FPSCR mascarado", s.fpscr == 0x00040104);
    }
    // FRCHG alterna fr/xf (0xFBFD); FSCHG alterna SZ (0xF3FD); FPCHG PR (0xF7FD)
    {
        Sh4State s = fresh(bus);
        s.fr[3] = 0xAAA1;
        s.xf[3] = 0xBBB1;
        bus.write16(kB, 0xFBFD); // frchg
        s.pc = kB;
        s.step(bus);
        CHECK("frchg troca bancos", s.fr[3] == 0xBBB1 && s.xf[3] == 0xAAA1);
        bus.write16(kB, 0xF3FD); // fschg
        s.pc = kB;
        s.step(bus);
        CHECK("fschg alterna SZ", s.sz_bit());
        // fpchg e do SH-4A: ilegal no SH7091 (HH04), PR nao muda.
        bus.write16(kB, 0xF7FD);
        s.pc = kB;
        const bool pr_antes = s.pr_bit();
        const StepResult rf = s.step(bus);
        CHECK("fpchg ilegal no SH7091", rf.illegal && s.pr_bit() == pr_antes);

        CHECK("mnemonicos chg", strcmp(mnem(0xFBFD), "frchg") == 0 &&
                                   strcmp(mnem(0xF3FD), "fschg") == 0 &&
                                   lookup(0xF7FD) == nullptr);
    }
}

// ---------- 10. bancos e rte ----------
void testBanksRte(TestBus& bus) {
    Hooks h;
    g_hooks = &h;
    // set_sr troca r/rbank quando MD=1 && RB muda
    {
        Sh4State s = fresh(bus);
        s.r[2] = 0xAAAA;
        s.rbank[2] = 0xBBBB;
        s.set_sr(s.sr ^ kSrRB); // RB muda (MD=1 do reset)
        CHECK("set_sr troca bancos", s.r[2] == 0xBBBB && s.rbank[2] == 0xAAAA);
        s.set_sr(s.sr ^ kSrRB);
        CHECK("set_sr volta", s.r[2] == 0xAAAA && s.rbank[2] == 0xBBBB);
    }
    { // sem MD=1, mudanca de RB nao troca (reset tem MD=1, MD=0 explicito)
        Sh4State s = fresh(bus);
        s.set_sr(s.sr & ~kSrMD); // MD=0, RB intacto
        s.r[2] = 0xAAAA;
        s.rbank[2] = 0xBBBB;
        s.set_sr(s.sr ^ kSrRB); // RB muda com MD=0: sem troca
        CHECK("set_sr MD=0 sem troca", s.r[2] == 0xAAAA && s.rbank[2] == 0xBBBB);
    }

    // rte: ssr->SR (mascarado, troca banco efetivo se MD&&RB mudam),
    // spc->PC, com delay slot
    {
        Sh4State s = fresh(bus);
        s.r[2] = 0x1111;
        s.rbank[2] = 0x2222;
        s.ssr = 0x500001C0; // MD=1,RB=0: o reset tinha RB=1 -> troca de banco
        s.spc = 0x90100;
        s.r[15] = 0x3333;
        bus.write16(kB, 0x002B); // rte
        bus.write16(kB + 2, 0xDF10); // slot: mov.l @(64,PC),r15? inofensivo: le
        s.pc = kB;
        const StepResult r = s.step(bus);
        CHECK("rte tomado", !r.illegal);
        s.step(bus); // slot
        // rte restaura SR = SSR (aqui 0x500001C0) e PC = SPC
        CHECK("rte restaura SR/PC", s.pc == 0x90100 && s.sr == 0x500001C0);
        CHECK("rte trocou banco", s.r[2] == 0x2222 && s.rbank[2] == 0x1111);
        // volta: set_sr de volta
        s.set_sr(0x700000F0); // restaura RB=1
        CHECK("voltou", s.r[2] == 0x1111);
    }
}

// ---------- 11. desvios, pr e delay slots ----------
void testBranches(TestBus& bus) {
    Hooks h;
    g_hooks = &h;
    const uint32_t A = kB;

    // bra: alvo = A + 4 + 2*disp12 (disp = op & 0xFFF, sem sinal)
    { // alvo A+8: disp = 2; slot reescreve o registrador do braf depois
        Sh4State s = fresh(bus);
        bus.write16(A, 0xA002); // bra +2 (alvo A+8)
        bus.write16(A + 2, MOV_I(15, 0x77)); // slot: mov #0x77,r15
        bus.write16(A + 8, NOP);
        s.pc = A;
        s.step(bus); // bra: pc -> slot; alvo travado
        CHECK("bra entra no slot", s.pc == A + 2);
        s.step(bus); // slot roda e salt
        CHECK("bra alvo latched antes do slot", s.r[15] == 0x77 && s.pc == A + 8);
    }
    { // bsr: pr = A + 4
        Sh4State s = fresh(bus);
        bus.write16(A, 0xB003); // bsr +3 (alvo A+10)
        bus.write16(A + 2, NOP);
        bus.write16(A + 10, NOP);
        s.pc = A;
        s.step(bus);
        s.step(bus); // slot + salto
        CHECK("bsr pr e alvo", s.pr == A + 4 && s.pc == A + 10);
    }
    { // braf r15: alvo = A + 4 + r15(latched); slot mexe em r15
        Sh4State s = fresh(bus);
        s.r[15] = 0x30;
        bus.write16(A, 0x0F23); // braf r15
        bus.write16(A + 2, MOV_I(15, 0x40)); // slot: r15 = 0x40 (depois do latch)
        s.pc = A;
        s.step(bus);
        s.step(bus);
        CHECK("braf latched", s.r[15] == 0x40 && s.pc == A + 4 + 0x30);
    }
    { // bsrf r15: pr = A + 4
        Sh4State s = fresh(bus);
        s.r[15] = 0x40;
        bus.write16(A, 0x0F03); // bsrf r15 (encoding 0000nnnn00000011, n=0xF)
        bus.write16(A + 2, NOP);
        s.pc = A;
        s.step(bus);
        s.step(bus);
        CHECK("bsrf pr/alvo", s.pr == A + 4 && s.pc == A + 4 + 0x40);
    }
    { // jsr @r15: pr = A + 4; slot mexe em r15, alvo latched
        Sh4State s = fresh(bus);
        s.r[15] = 0x2A0;
        bus.write16(A, 0x4F0B); // jsr @r15
        bus.write16(A + 2, MOV_I(15, 9)); // slot: r15 = 9
        bus.write16(0x2A0, 0x000B); // rts no alvo
        bus.write16(0x2A2, NOP); // slot do rts
        s.pc = A;
        s.step(bus);
        s.step(bus); // slot: pc = 0x2A0
        CHECK("jsr latched/pr", s.r[15] == 9 && s.pc == 0x2A0 && s.pr == A + 4);
        s.step(bus); // rts: entra no slot
        s.step(bus); // slot do rts -> volta para pr
        CHECK("rts volta para pr", s.pc == A + 4);
    }
    { // jmp @r15 sem pr
        Sh4State s = fresh(bus);
        s.r[15] = 0x260;
        bus.write16(A, 0x4F2B); // jmp @r15
        bus.write16(A + 2, NOP);
        s.pr = 0x1234;
        s.pc = A;
        s.step(bus); s.step(bus);
        CHECK("jmp alvo e pr intacto", s.pc == 0x260 && s.pr == 0x1234);
    }
    { // bt sem delay slot: tomado pula direto, slot ignorado
        Sh4State s = fresh(bus);
        bus.write16(A, 0xC800);        // tst #0,r0: com r0=0, T = 1
        bus.write16(A + 2, 0x8901);    // bt +1 -> alvo = op + 4 + 2*disp = A+8
        bus.write16(A + 4, MOV_I(2, 7)); // "slot" que NAO deve rodar
        bus.write16(A + 8, NOP);       // alvo imediato
        bus.write16(A + 10, MOV_I(3, 8));
        s.pc = A;
        s.step(bus); // tst #0: T = 1
        CHECK("tst #0 seta T", s.t());
        s.step(bus); // bt tomado: alvo imediato
        CHECK("bt tomado sem slot", s.pc == A + 8 && s.r[2] == 0);
        s.step(bus); // NOP no alvo
        s.step(bus); // mov #8,r3
        CHECK("bt continua", s.pc == A + 12 && s.r[3] == 8);
    }
    { // bt nao tomado: segue linear (slot executado como prox instr)
        Sh4State s = fresh(bus);
        bus.write16(A, MOV_I(1, 0)); // r1 = 0, T = 0
        bus.write16(A + 2, 0x8901); // bt +1 (nao tomado)
        bus.write16(A + 4, MOV_I(2, 7)); // roda como sequencia normal
        s.pc = A;
        s.step(bus);
        s.step(bus);
        s.step(bus);
        CHECK("bt nao tomado", !s.t() && s.r[2] == 7 && s.pc == A + 6);
    }
    { // bt/s COM delay slot
        Sh4State s = fresh(bus);
        bus.write16(A, 0xC800);     // tst #0,r0: T = 1 (mov #imediato nao altera T)
        bus.write16(A + 2, 0x8D02); // bt/s +2 -> alvo = op + 4 + 2*disp = A+10
        bus.write16(A + 4, MOV_I(2, 7)); // slot (deve rodar)
        s.pc = A;
        s.step(bus); s.step(bus); s.step(bus);
        CHECK("bt/s slot roda", s.r[2] == 7 && s.pc == A + 10);
    }
    { // bf e bf/s
        Sh4State s = fresh(bus);
        bus.write16(A, MOV_I(1, 0)); // mov #0 nao altera T (fresh T = 0)
        bus.write16(A + 2, 0x8B01); // bf +1 (T=0 -> tomado, sem slot): alvo A+8
        bus.write16(A + 4, MOV_I(2, 7)); // slot, ignorado
        bus.write16(A + 8, NOP); // alvo
        s.pc = A;
        s.step(bus); s.step(bus);
        CHECK("bf tomado sem slot", s.pc == A + 8 && s.r[2] == 0);
        s.step(bus); // NOP no alvo
        CHECK("bf continua no alvo", s.pc == A + 10);
        bus.write16(A + 10, 0xC800); // tst #0,r0: com r0=0, T = 1
        bus.write16(A + 12, 0x8F02); // bf/s +2 (T=1 -> nao tomado)
        bus.write16(A + 14, MOV_I(2, 9)); // roda como instrucao normal (pc linear)
        s.pc = A + 10;
        s.step(bus); s.step(bus); s.step(bus);
        CHECK("bf/s nao tomado", s.r[2] == 9 && s.pc == A + 16);
    }
    { // desvio no slot: alvo do desvio externo vence
        Sh4State s = fresh(bus);
        bus.write16(A, 0xA003); // bra +3 -> alvo A + 4 + 6 = A+10
        bus.write16(A + 2, 0x0009); // slot: nop
        s.pc = A;
        s.step(bus); s.step(bus);
        CHECK("bra alvo A+10", s.pc == A + 10);
        bus.write16(A + 10, 0xA002);   // bra -> alvo A+18
        bus.write16(A + 12, 0xA000);   // slot: bra -> alvo A+16 (perde)
        bus.write16(A + 16, NOP);
        bus.write16(A + 18, NOP);
        s.pc = A + 10;
        s.step(bus); s.step(bus);
        CHECK("desvio no slot: externo vence", s.pc == A + 18);
    }
    { // PC-relativo com mova/mov.w/mov.l usando PC do endereco + 4
        Sh4State s = fresh(bus);
        bus.write32(A + 0x24, 0xCAFED00D);
        bus.write16(A + 0x14, 0xBEEF);
        bus.write16(A, 0xD108); // mov.l @(8*4,PC),r1 -> (A&~3)+4+32 = A+0x24
        bus.write16(A + 2, 0x9207); // mov.w @(7*2,PC),r2 -> (A+2)+4+14 = A+0x14
        bus.write16(A + 4, 0xC70A); // mova @(10*4,PC),r0 -> (A+4)+4+40 = A+0x30
        s.pc = A;
        s.step(bus); s.step(bus); s.step(bus);
        CHECK("mov.l (PC)", s.r[1] == 0xCAFED00D);
        CHECK("mov.w (PC)", s.r[2] == 0xFFFFBEEF);
        CHECK("mova (PC)", s.r[0] == A + 0x30); // base alinhada do endereco da op
    }
}

// ---------- 12. FPU precisao unica ----------
float fb(uint32_t b) { return std::bit_cast<float>(b); }
constexpr uint32_t FU(float v) { return std::bit_cast<uint32_t>(v); }

void testFpuSingle(TestBus& bus) {
    Hooks h;
    g_hooks = &h;
    Sh4State s = fresh(bus);

    // fldi0/fldi1
    bus.write16(kB, 0xF78D); // fldi0 fr7
    bus.write16(kB + 2, 0xF79D); // fldi1 fr7
    s.pc = kB;
    s.step(bus);
    CHECK("fldi0", s.fr[7] == 0);
    s.step(bus);
    CHECK("fldi1", s.fr[7] == 0x3F800000);

    // fsts/lds (padrao MIT + SLEIGH: FSTS = 0xFn0D grava FRn <- FPUL;
    // FLDS = 0xFn1D le FPUL <- FRn; ambas com o registrador em bits 11-8)
    s.fr[6] = 0xAABBCCDD;
    bus.write16(kB, 0xF61D); // flds fr6,FPUL: fpul <- fr6
    s.pc = kB;
    s.step(bus);
    CHECK("flds", s.fpul == 0xAABBCCDD);
    s.fpul = 0x12345678;
    bus.write16(kB, 0xFA0D); // fsts FPUL,fr10: fr10 <- fpul
    s.pc = kB;
    s.step(bus);
    CHECK("fsts", s.fr[10] == 0x12345678);

    // aritmetica: fadd/fsub/fmul/fdiv em fr2 = fr2 op fr3
    s.fr[2] = FU(1.5f);
    s.fr[3] = FU(0.25f);
    struct Fop { uint16_t op; uint32_t want; const char* name; };
    const Fop ops[] = {
        {0xF230, FU(1.75f), "fadd"},
        {0xF231, FU(1.25f), "fsub"},
        {0xF232, FU(0.375f), "fmul"},
    };
    for (const Fop& o : ops) {
        s.fr[2] = FU(1.5f);
        s.fr[3] = FU(0.25f);
        s.pc = kB; // op passa a operar no valor imediato lido? reescreve
        bus.write16(kB, o.op);
        s.pc = kB;
        s.step(bus);
        CHECK(o.name, s.fr[2] == o.want);
    }
    s.fr[2] = FU(1.0f);
    s.fr[3] = FU(4.0f);
    bus.write16(kB, 0xF233); // fdiv
    s.pc = kB;
    s.step(bus);
    CHECK("fdiv", s.fr[2] == FU(0.25f));

    // fcmp/eq e fcmp/gt
    s.fr[2] = FU(1.0f);
    s.fr[3] = FU(1.0f);
    bus.write16(kB, 0xF234); // fcmp/eq
    s.pc = kB;
    s.step(bus);
    CHECK("fcmp/eq igual", s.t());
    s.fr[3] = FU(1.5f);
    bus.write16(kB, 0xF234);
    s.pc = kB;
    s.step(bus);
    CHECK("fcmp/eq diferente", !s.t());
    s.fr[2] = FU(2.0f);
    s.fr[3] = FU(1.0f);
    bus.write16(kB, 0xF235); // fcmp/gt: fr2 > fr3
    s.pc = kB;
    s.step(bus);
    CHECK("fcmp/gt maior", s.t());
    s.fr[2] = FU(1.0f);
    s.fr[3] = FU(2.0f);
    bus.write16(kB, 0xF235);
    s.pc = kB;
    s.step(bus);
    CHECK("fcmp/gt menor", !s.t());

    // fmac: fr[n] += fr[0] * fr[m]
    s.fr[0] = FU(2.0f);
    s.fr[1] = FU(3.0f);
    s.fr[4] = FU(4.0f);
    bus.write16(kB, 0xF41E); // fmac fr0,fr1,fr4
    s.pc = kB;
    s.step(bus);
    CHECK("fmac", s.fr[4] == FU(10.0f));

    // fabs/fneg (FABS = 0xFn5D, FNEG = 0xFn4D, n em bits 11-8)
    s.fr[5] = FU(-3.5f);
    bus.write16(kB, 0xF55D); // fabs fr5
    s.pc = kB;
    s.step(bus);
    CHECK("fabs", s.fr[5] == FU(3.5f));
    bus.write16(kB, 0xF54D); // fneg fr5
    s.pc = kB;
    s.step(bus);
    CHECK("fneg", s.fr[5] == FU(-3.5f));

    // fsqrt (PR=0)
    s.fr[9] = FU(4.0f);
    bus.write16(kB, 0xF96D); // fsqrt fr9
    s.pc = kB;
    s.step(bus);
    CHECK("fsqrt", s.fr[9] == FU(2.0f));

    // fsrra: 1/sqrt
    s.fr[8] = FU(4.0f);
    bus.write16(kB, 0xF87D); // fsrra fr8 -> 0.5
    s.pc = kB;
    s.step(bus);
    CHECK("fsrra", std::fabs(fb(s.fr[8]) - 0.5f) < 1e-6f);

    // float/ftrc (PR=0)
    s.fpul = 5;
    bus.write16(kB, 0xF12D); // float FPUL,fr1
    s.pc = kB;
    s.step(bus);
    CHECK("float", s.fr[1] == FU(5.0f));

    struct FT { uint32_t in, want; const char* name; };
    const FT fts[] = {
        {FU(2.5f), 2, "ftrc positivo"},
        {FU(-2.5f), 0xFFFFFFFEu, "ftrc negativo"},
        {0x7FC00000u, 0x80000000u, "ftrc NaN"},
        {0x7F800000u, 0x7FFFFFFFu, "ftrc +inf"},
        {0xFF800000u, 0x80000000u, "ftrc -inf"},
        {FU(1e10f), 0x7FFFFFFFu, "ftrc grande+"},
        {FU(-1e10f), 0x80000000u, "ftrc grande-"},
    };
    for (const FT& f : fts) {
        s.fr[2] = f.in;
        bus.write16(kB, 0xF23D); // ftrc fr2,FPUL
        s.pc = kB;
        s.step(bus);
        CHECK(f.name, s.fpul == f.want);
    }

    // fsca: dr = (sin, cos) por tabela; fpul&0xFFFF e indice
    {
        Sh4State s2 = fresh(bus);
        s2.fpul = 0;
        bus.write16(kB, 0xF0FD); // fsca FPUL,dr0
        s2.pc = kB;
        s2.step(bus);
        CHECK("fsca idx 0 sin", s2.fr[0] == 0);
        CHECK("fsca idx 0 cos", s2.fr[1] == 0x3F800000);
        s2.fpul = 0x4000; // quarto de volta: sin ~ 1, cos ~ 0
        bus.write16(kB, 0xFEFD); // fsca FPUL,dr14 (n = 0xE par, manual)
        s2.pc = kB;
        s2.step(bus);
        CHECK("fsca dr14 sin", std::fabs(fb(s2.fr[14]) - 1.0f) < 1e-6f);
        CHECK("fsca dr14 cos", std::fabs(fb(s2.fr[15])) < 1e-6f);
        // PR=1: no-op (undefined pela spec)
        s2.set_fpscr(0x00040001 | kFpscrPR);
        s2.fr[0] = 0xDEAD;
        s2.fpul = 0x4000;
        bus.write16(kB, 0xF0FD);
        s2.pc = kB;
        s2.step(bus);
        CHECK("fsca PR=1 no-op", s2.fr[0] == 0xDEAD);
    }
}

// ---------- 13. FPU precisao dupla ----------
void testFpuDouble(TestBus& bus) {
    Hooks h;
    g_hooks = &h;
    Sh4State s = fresh(bus);
    s.r[1] = s.fpscr | 0x00080000u; // PR=1
    bus.write16(kB, 0x416A);        // lds r1,fpscr
    s.pc = kB;
    s.step(bus);
    CHECK("PR=1", s.pr_bit());

    // dp fadd: dr2 = fr[2..3] = 0.25 + dr0 (1.5) = 1.75 (n = 2, m = 0)
    s.fr[0] = 0x3FF80000; s.fr[1] = 0; // 1.5 dp
    s.fr[2] = 0x3FD00000; s.fr[3] = 0; // 0.25 dp
    bus.write16(kB + 4, 0xF200); // fadd dr2,dr0 -> grava dr2
    s.pc = kB + 4;
    s.step(bus);
    CHECK("dp fadd", s.fr[2] == 0x3FFC0000 && s.fr[3] == 0);

    // dp fsqrt: dr0 = 4.0 -> 2.0
    s.fr[0] = 0x40100000; s.fr[1] = 0;
    bus.write16(kB, 0xF06D); // fsqrt dr0
    s.pc = kB;
    s.step(bus);
    CHECK("dp fsqrt", s.fr[0] == 0x40000000 && s.fr[1] == 0);

    // dp float: fpul = 5 -> 5.0
    s.fpul = 5;
    bus.write16(kB, 0xF02D); // float FPUL,dr0
    s.pc = kB;
    s.step(bus);
    CHECK("dp float", s.fr[0] == 0x40140000 && s.fr[1] == 0);

    // dp ftrc: dr0 = 3.75 -> 3
    s.fr[0] = 0x400E0000; s.fr[1] = 0; // 3.75 dp
    bus.write16(kB, 0xF03D); // ftrc dr0,FPUL
    s.pc = kB;
    s.step(bus);
    CHECK("dp ftrc", s.fpul == 3);
    s.fr[0] = 0xC00E0000; s.fr[1] = 0; // -3.75
    bus.write16(kB, 0xF03D);
    s.pc = kB;
    s.step(bus);
    CHECK("dp ftrc neg", s.fpul == 0xFFFFFFFD);

    // dp fcmp/eq e fcmp/gt: dr0 vs dr8 (fr[8..9])
    s.fr[0] = 0x40000000; s.fr[1] = 0; // 2.0
    s.fr[8] = 0x40000000; s.fr[9] = 0; // 2.0
    bus.write16(kB, 0xF084); // fcmp/eq dr8,dr0
    s.pc = kB;
    s.step(bus);
    CHECK("dp fcmp/eq", s.t());
    s.fr[8] = 0x40080000; s.fr[9] = 0; // 3.0
    bus.write16(kB, 0xF085); // fcmp/gt dr8,dr0: dr0 (2.0) < dr8 -> falso
    s.pc = kB;
    s.step(bus);
    CHECK("dp fcmp/gt menor", !s.t());
    s.fr[0] = 0x40100000; s.fr[1] = 0; // 4.0
    bus.write16(kB, 0xF085);
    s.pc = kB;
    s.step(bus);
    CHECK("dp fcmp/gt maior", s.t());

    // fcnvsd: FPUL single -> dr0 double
    s.fpul = FU(1.5f);
    bus.write16(kB, 0xF0AD); // fcnvsd FPUL,dr0
    s.pc = kB;
    s.step(bus);
    CHECK("fcnvsd", s.fr[0] == 0x3FF80000 && s.fr[1] == 0);

    // fcnvds: dr0 double -> FPUL single
    s.fr[0] = 0x3FE00000; s.fr[1] = 0; // 0.5 dp
    bus.write16(kB, 0xF0BD); // fcnvds dr0,FPUL
    s.pc = kB;
    s.step(bus);
    CHECK("fcnvds", s.fpul == 0x3F000000);

    // dp fabs: dr0 = -1.5 -> +1.5
    s.fr[0] = 0xBFF80000; s.fr[1] = 0;
    bus.write16(kB, 0xF05D); // fabs dr0
    s.pc = kB;
    s.step(bus);
    CHECK("dp fabs", s.fr[0] == 0x3FF80000 && s.fr[1] == 0);
}

// ---------- 14. fipr e ftrv ----------
void testFiprFtrv(TestBus& bus) {
    Hooks h;
    g_hooks = &h;

    // fipr FV0,FV0: (1,2,3,4). = 30 -> fr[3]
    {
        Sh4State s = fresh(bus);
        s.fr[0] = FU(1.0f); s.fr[1] = FU(2.0f); s.fr[2] = FU(3.0f); s.fr[3] = FU(4.0f);
        bus.write16(kB, 0xF0ED); // fipr fv4,fv0 (dest FV0)
        s.pc = kB;
        s.step(bus);
        CHECK("fipr FV0.FV0", s.fr[3] == FU(30.0f) && s.fr[0] == FU(1.0f) &&
                                  s.fr[1] == FU(2.0f));
    }
    // fipr assimetrico: dest FV1 (bits 11-10 = 01), fonte FV2 (bits 9-8 = 10)
    {
        Sh4State s = fresh(bus);
        s.fr[4] = FU(2.0f); s.fr[5] = FU(2.0f); s.fr[6] = FU(2.0f); s.fr[7] = FU(2.0f);
        s.fr[8] = FU(3.0f); s.fr[9] = FU(3.0f); s.fr[10] = FU(3.0f); s.fr[11] = FU(3.0f);
        bus.write16(kB, 0xF6ED); // fipr fv8,fv4: dest FV1, fonte FV2
        s.pc = kB;
        s.step(bus);
        CHECK("fipr FV1.FV2", s.fr[7] == FU(24.0f) && s.fr[11] == FU(3.0f));
    }
    // PR=1: no-op
    {
        Sh4State s = fresh(bus);
        s.set_fpscr(0x00040001 | kFpscrPR);
        s.fr[0] = 1; s.fr[1] = 2; s.fr[2] = 3; s.fr[3] = FU(4.0f);
        bus.write16(kB, 0xF0ED);
        s.pc = kB;
        s.step(bus);
        CHECK("fipr PR=1 no-op", s.fr[3] == FU(4.0f));
    }

    // ftrv: fv = XMTRX * fv; XF identidade preserva FV0
    {
        Sh4State s = fresh(bus);
        for (int k = 0; k < 16; ++k) s.xf[k] = 0;
        s.xf[0] = FU(1.0f); s.xf[5] = FU(1.0f); s.xf[10] = FU(1.0f); s.xf[15] = FU(1.0f);
        s.fr[0] = FU(1.0f); s.fr[1] = FU(2.0f); s.fr[2] = FU(3.0f); s.fr[3] = FU(4.0f);
        bus.write16(kB, 0xF1FD); // ftrv xmtrx,fv0 (dest FV0)
        s.pc = kB;
        s.step(bus);
        CHECK("ftrv identidade", s.fr[0] == FU(1.0f) && s.fr[1] == FU(2.0f) &&
                                     s.fr[2] == FU(3.0f) && s.fr[3] == FU(4.0f));
    }
    // ftrv com diag(2): dobra o vetor
    {
        Sh4State s = fresh(bus);
        for (int k = 0; k < 16; ++k) s.xf[k] = 0;
        s.xf[0] = FU(2.0f); s.xf[5] = FU(2.0f); s.xf[10] = FU(2.0f); s.xf[15] = FU(2.0f);
        s.fr[0] = FU(1.0f); s.fr[1] = FU(-2.0f); s.fr[2] = FU(3.0f); s.fr[3] = FU(-4.0f);
        bus.write16(kB, 0xF1FD);
        s.pc = kB;
        s.step(bus);
        CHECK("ftrv diag2", s.fr[0] == FU(2.0f) && s.fr[1] == FU(-4.0f) &&
                                s.fr[2] == FU(6.0f) && s.fr[3] == FU(-8.0f));
    }
    // ftrv com FV3 (bits 11-8 = 13 -> base 12): fonte = destino = FVn.
    // Fontes: comentario MIT ("FVn = XMTRX * FVn, a = n*4") e SLEIGH (le e
    // escreve FR[FVN+k]); FV0 deve ficar intocado.
    {
        Sh4State s = fresh(bus);
        for (int k = 0; k < 16; ++k) s.xf[k] = 0;
        s.xf[0] = FU(2.0f); s.xf[5] = FU(2.0f); s.xf[10] = FU(2.0f); s.xf[15] = FU(2.0f);
        s.fr[0] = FU(1.0f); s.fr[1] = FU(2.0f); s.fr[2] = FU(3.0f); s.fr[3] = FU(4.0f);
        s.fr[12] = FU(5.0f); s.fr[13] = FU(6.0f); s.fr[14] = FU(7.0f); s.fr[15] = FU(8.0f);
        bus.write16(kB, 0xFDFD); // ftrv xmtrx,fv3
        s.pc = kB;
        s.step(bus);
        CHECK("ftrv dest FV3", s.fr[12] == FU(10.0f) && s.fr[13] == FU(12.0f) &&
                                   s.fr[14] == FU(14.0f) && s.fr[15] == FU(16.0f) &&
                                   s.fr[0] == FU(1.0f) && s.fr[1] == FU(2.0f) &&
                                   s.fr[2] == FU(3.0f) && s.fr[3] == FU(4.0f));
    }
    // PR=1: no-op
    {
        Sh4State s = fresh(bus);
        s.set_fpscr(0x00040001 | kFpscrPR);
        for (int k = 0; k < 16; ++k) s.xf[k] = 0;
        s.xf[0] = 7;
        s.fr[0] = 1; s.fr[1] = 2; s.fr[2] = 3; s.fr[3] = 4;
        bus.write16(kB, 0xF1FD);
        s.pc = kB;
        s.step(bus);
        CHECK("ftrv PR=1 no-op", s.fr[0] == 1 && s.fr[3] == 4);
    }
}

// ---------- 15. fmov (single e pares com SZ=1) ----------
void testFmovFamily(TestBus& bus) {
    Hooks h;
    g_hooks = &h;

    { // SZ=0: carga direta, grava, posinc, predec, indexado, reg-reg
        Sh4State s = fresh(bus);
        s.r[14] = 0x4000;
        bus.write32(0x4000, 0x41C00000); // 24.0f
        bus.write16(kB, 0xF5E8); // fmov.s @r14,fr5
        s.pc = kB;
        s.step(bus);
        CHECK("fmov.s carga", s.fr[5] == 0x41C00000);

        s.fr[5] = 0x41200000; // 10.0f
        s.r[14] = 0x4100;
        bus.write16(kB, 0xFE5A); // fmov.s fr5,@r14
        s.pc = kB;
        s.step(bus);
        CHECK("fmov.s grava", bus.read32(0x4100) == 0x41200000);

        s.r[15] = 0x4200;
        bus.write32(0x4200, 0x41A00000); // 20.0f
        bus.write16(kB, 0xF5F9); // fmov.s @r15+,fr5
        s.pc = kB;
        s.step(bus);
        CHECK("fmov.s posinc", s.fr[5] == 0x41A00000 && s.r[15] == 0x4204);

        s.r[14] = 0x4140;
        bus.write16(kB, 0xFE5B); // fmov.s fr5,@-r14
        s.pc = kB;
        s.step(bus);
        CHECK("fmov.s predec", bus.read32(0x413C) == 0x41A00000 && s.r[14] == 0x413C);

        s.r[0] = 0x10;
        s.r[12] = 0x5000;
        bus.write32(0x5010, 0x41C00000); // @(r0,r12) = 0x10 + 0x5000 = 0x5010
        bus.write16(kB, 0xF5C6); // fmov.s @(r0,r12),fr5
        s.pc = kB;
        s.step(bus);
        CHECK("fmov.s indexado carga", s.fr[5] == 0x41C00000);

        s.fr[9] = 0x4E000000; // 8.0e9f? so bits
        bus.write16(kB, 0xF79C); // fmov fr9,fr7 (SZ=0 copia simples)
        s.pc = kB;
        s.step(bus);
        CHECK("fmov reg-reg sz0", s.fr[7] == 0x4E000000 && s.fr[9] == 0x4E000000);
    }

    { // SZ=1: pares por paridade; impar -> XF, par -> FR
        Sh4State s = fresh(bus);
        bus.write16(kB, 0xF3FD); // fschg -> SZ=1
        s.pc = kB;
        s.step(bus);
        CHECK("SZ=1", s.sz_bit());

        s.r[15] = 0x4400;
        bus.write32(0x4400, 0x3F800000); // 1.0
        bus.write32(0x4404, 0xC0000000); // -2.0
        bus.write16(kB + 2, 0xFBF9); // fmov @r15+,xd11 (par impar -> XD[10])
        s.pc = kB + 2;
        s.step(bus);
        CHECK("fmov par carga +8", s.xf[10] == 0x3F800000 && s.xf[11] == 0xC0000000 &&
                                       s.r[15] == 0x4408);
        // predec: grava o par xd9 (xf[8..9]) em @-r14
        s.xf[8] = 0x1111;
        s.xf[9] = 0x2222;
        s.r[14] = 0x4500;
        bus.write16(kB, 0xFE9B); // fmov xd9,@-r14
        s.pc = kB;
        s.step(bus);
        CHECK("fmov par predec", bus.read32(0x44F8) == 0x1111 &&
                                     bus.read32(0x44FC) == 0x2222 && s.r[14] == 0x44F8);
        // reg-reg com SZ=1: copia pares (dest XD7 = xf[6..7], src XD9)
        s.xf[6] = 0x3333;
        s.xf[7] = 0x4444;
        s.xf[8] = 0xAAAA;
        s.xf[9] = 0xBBBB;
        bus.write16(kB, 0xF79C); // fmov xd9,xd7
        s.pc = kB;
        s.step(bus);
        CHECK("fmov pares reg-reg", s.xf[6] == 0xAAAA && s.xf[7] == 0xBBBB);
        // cruz de banco: FR[10..11] -> XD[12..13] (0xFDAC)
        s.fr[10] = 0x5555;
        s.fr[11] = 0x6666;
        bus.write16(kB, 0xFDAC);
        s.pc = kB;
        s.step(bus);
        CHECK("fmov par cruza banco", s.xf[12] == 0x5555 && s.xf[13] == 0x6666);
    }

    { // F00C com SZ=1: copia PAR; indices impares caem em XD (banco XF)
        Sh4State s = fresh(bus);
        s.fpscr = kFpscrSZ; // SZ=1 sem troca de banco (FR/XF intocados)
        s.xf[8] = 0x7777;
        s.xf[9] = 0x8888;
        s.fr[7] = 0x5555; // deve ficar intocado (XD7 vive em xf[6..7])
        bus.write16(kB, 0xF79C); // fmov XD7,XD9 (sz=1): par xd9 -> xd7
        s.pc = kB;
        s.step(bus);
        // sz=1 com indices 7/9: XD7 <- XD9 (nao toca fr[7])
        CHECK("fmov sz1 pares", s.xf[6] == 0x7777 && s.xf[7] == 0x8888 &&
                                   s.fr[7] == 0x5555);
    }
}

// ---------- 16. extensoes do SH-4A sao ilegais no SH7091 (HH04) ----------
// synco, icbi, movua.l e fpchg existem so no SH-4A; o SLEIGH SuperH4 as
// decodifica, o SH7091 nao. Cada uma precisa cair no tratador de ilegal sem
// efeito nos registradores nem na memoria.
void testSh4aIlegais(TestBus& bus) {
    Hooks h;
    g_hooks = &h;
    const uint16_t ops[] = {0x00AB, 0x05E3, 0x45A9, 0x45E9, 0x40E9, 0xF7FD};
    for (uint16_t op : ops) {
        Sh4State s = fresh(bus);
        s.r[0] = 0x5000;
        s.r[5] = 0x4004;
        bus.write32(0x4004, 0x11223344);
        const uint32_t fpscr_antes = s.fpscr;
        bus.write16(kB, op);
        s.pc = kB;
        const unsigned before = h.illegal;
        const StepResult res = s.step(bus);
        CHECK("SH-4A ilegal", lookup(op) == nullptr && res.illegal && h.illegal == before + 1 &&
                                  h.illegal_op == op && s.r[0] == 0x5000 && s.r[5] == 0x4004 &&
                                  s.fpscr == fpscr_antes);
    }
}

// ---------- 17. hooks: trapa/ldtlb/sleep/cache e tas.b ----------
void testHooksOps(TestBus& bus) {
    Hooks h;
    g_hooks = &h;

    { // trapa #imm
        Sh4State s = fresh(bus);
        bus.write16(kB, 0xC3FF); // trapa #0xff
        s.pc = kB;
        s.step(bus);
        CHECK("trapa", h.trapa == 1 && h.trap_imm == 0xFF);
    }
    { // ldtlb
        Sh4State s = fresh(bus);
        bus.write16(kB, 0x0038); // ldtlb
        s.pc = kB;
        s.step(bus);
        CHECK("ldtlb", h.ldtlb == 1);
    }
    { // sleep
        Sh4State s = fresh(bus);
        bus.write16(kB, 0x001B); // sleep
        s.pc = kB;
        s.step(bus);
        CHECK("sleep", h.sleep == 1);
    }
    { // cache: pref/ocbi/ocbp/ocbwb: um kind (o proprio opcode) por
      // chamada, addr = r[n] (icbi e do SH-4A: ilegal, ver testSh4aIlegais)
        Sh4State s = fresh(bus);
        s.r[15] = 0x6000;
        const uint16_t cacheops[] = {0x0F83, 0x0F93, 0x0FA3, 0x0FB3};
        for (uint16_t op : cacheops) {
            const unsigned before = h.cache;
            bus.write16(kB, op);
            s.pc = kB;
            s.step(bus);
            CHECK("cache kind/addr", h.cache == before + 1 && h.cache_kind == op &&
                                        h.cache_addr == 0x6000);
        }
        CHECK("cache 4 ops", h.cache == 4);
    }
    { // tas.b: byte 0 -> grava 0x80 e T=1; byte 0x80 -> T=0
        Sh4State s = fresh(bus);
        s.r[2] = 0x4000;
        bus.write8(0x4000, 0);
        bus.write16(kB, 0x421B); // tas.b @r2
        s.pc = kB;
        s.step(bus);
        CHECK("tas.b zero", bus.read8(0x4000) == 0x80 && s.t());
        bus.write8(0x4000, 0x80);
        bus.write16(kB + 2, 0x421B);
        s.pc = kB + 2;
        s.step(bus);
        CHECK("tas.b ja setado", bus.read8(0x4000) == 0x80 && !s.t());
    }
    { // rte atraves de pc conservado e hooks ilegais nao disparados
        Sh4State s = fresh(bus);
        s.ssr = 0x700000F0;
        s.spc = 0x92000;
        bus.write16(kB, 0x002B); // rte
        bus.write16(kB + 2, NOP); // slot
        s.pc = kB;
        s.step(bus); s.step(bus);
        CHECK("rte ok sem hooks", h.illegal == 0 && s.pc == 0x92000);
    }
}

// ---------- 18. varredura exaustiva dos 65.536 opcodes ----------
// Legalidade do oraculo (sleigh_truth2.txt): 59052 legais, 6484 ilegais,
// por major {0:1572, 2:256, 3:512, 4:2048, 8:1792, F:304}.
void testExhaustive(TestBus& bus) {
    Hooks h;
    g_hooks = &h;

    unsigned legal = 0;
    unsigned illegal = 0;
    unsigned illegal_major[16] = {};
    unsigned hook_failures = 0;
    unsigned illegal_failures = 0;

    for (unsigned op32 = 0; op32 < 0x10000; ++op32) {
        const uint16_t op = uint16_t(op32);
        bus.write16(kB, op);
        Sh4State s = fresh(bus);
        s.r[15] = 0xFFF0;
        const unsigned before = h.illegal;
        const StepResult res = s.step(bus);
        if (lookup(op) != nullptr) {
            ++legal;
            if (res.illegal) {
                ++hook_failures;
                if (hook_failures <= 5) std::printf("legal executa ilegal: %04X\n", op);
            }
            if (h.illegal != before) {
                ++hook_failures;
                if (hook_failures <= 5)
                    std::printf("legal chama on_illegal: %04X\n", op);
            }
        } else {
            ++illegal;
            ++illegal_major[op >> 12];
            if (!res.illegal) {
                ++illegal_failures;
                if (illegal_failures <= 5)
                    std::printf("ilegal sem res.illegal: %04X\n", op);
            }
            if (h.illegal != before + 1) {
                ++illegal_failures;
                if (illegal_failures <= 5) std::printf("ilegal sem hook: %04X\n", op);
            }
        }
    }

    CHECK("total legal 59001", legal == 59001);
    CHECK("total ilegal 6535", illegal == 6535);
    CHECK("majors ilegais", illegal_major[0] == 1589 && illegal_major[2] == 256 &&
                                illegal_major[3] == 512 && illegal_major[4] == 2080 &&
                                illegal_major[8] == 1792 && illegal_major[0xF] == 306);
    CHECK("sem falha de hook", hook_failures == 0);
    CHECK("sem falha de ilegal", illegal_failures == 0);
}

int main() {
    static TestBus bus; // 1 MB nao caberia na pilha do Windows
    testMovMemSizes(bus);
    testBopGbr(bus);
    testAritCarry(bus);
    testSwapXtrct(bus);
    testShiftsRots(bus);
    testMulMacFacts(bus);
    testMacSaturation(bus);
    testDivRoutines(bus);
    testControlRegs(bus);
    testBanksRte(bus);
    testBranches(bus);
    testFpuSingle(bus);
    testFpuDouble(bus);
    testFiprFtrv(bus);
    testFmovFamily(bus);
    testSh4aIlegais(bus);
    testHooksOps(bus);
    testExhaustive(bus);

    if (g_failed == 0) {
        std::printf("OK - hh03_semantics\n");
        return 0;
    }
    std::printf("FALHAS: %d\n", g_failed);
    return 1;
}


