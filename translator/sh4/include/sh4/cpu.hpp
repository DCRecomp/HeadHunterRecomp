// SPDX-License-Identifier: MIT
// Estado do nucleo SH-4 (SH7091) - HH03.
//
// Fontes dos fatos de estado e semantica (duas evidencias independentes):
//   1. manual Renesas SH-4 (via oraculo SLEIGH SuperH4 lido integralmente:
//      layout de bits, formulas, guardas);
//   2. referencia MIT C:\Source\recomp-BStinger (runtime/cpu/sh4_ops.h,
//      sh4_context.cpp) - so leitura, sem copia de codigo.
//
// Divergencias SLEIGH-vs-manual implementadas pelo MANUAL e registradas no
// relatorio HH03: braf/bsrf com base A+4, PR = A+4 para bsr/bsrf/jsr,
// mac.w M x N (a spec tem o bug tmpN*tmpN). As extensoes do SH-4A que o
// SLEIGH tambem decodifica (synco, icbi, movua.l, fpchg) sao ilegais no SH7091
// (HH04, tools/sh4-tablegen/gen_decoder_table.py SH4A_ONLY).
#pragma once

#include <bit>
#include <cstdint>
#include <optional>

#include "sh4/bus.hpp"
#include "sh4/ops.hpp"

namespace hh::sh4 {

// Campos de SR (bits gravaveis = kSrWritable; reservados ficam intactos).
enum : uint32_t {
    kSrT     = 1u << 0,
    kSrS     = 1u << 1,
    kSrIMask = 0xFu << 4,
    kSrQ     = 1u << 8,
    kSrM     = 1u << 9,
    kSrFD    = 1u << 15,
    kSrBL    = 1u << 28,
    kSrRB    = 1u << 29,
    kSrMD    = 1u << 30,
    kSrWritable = 0x700083F3u, // T,S,IMASK,Q,M,FD,BL,RB,MD
};

// Campos de FPSCR (24 bits inferiores gravaveis).
enum : uint32_t {
    kFpscrRM       = 3u << 0,
    kFpscrFlag     = 0x1Fu << 2,
    kFpscrEnable   = 0xFu << 7,
    kFpscrCause    = 0x3Fu << 12,
    kFpscrDN       = 1u << 18,
    kFpscrPR       = 1u << 19,
    kFpscrSZ       = 1u << 20,
    kFpscrFR       = 1u << 21,
    kFpscrWritable = 0x003FFFFFu,
};

// Resultado de step().
struct StepResult {
    uint16_t opcode = 0;
    uint32_t addr = 0;
    bool illegal = false; // opcode sem linha da tabela (ILEGAL)
};

// Estado completo; campos publicos por serem dados simples. Os invariantes
// (bancos de R0-R7 e de FR/XF, bits reservados) ficam em set_sr/set_fpscr.
struct Sh4State {
    void reset(uint32_t entry);
    // Escreve SR mascarado; troca os bancos de R0-R7 quando o banco
    // efetivo muda (MD=1 && RB mudan); T via bit 0.
    void set_sr(uint32_t value);
    // Escreve FPSCR mascarado; troca os bancos de FR/XF quando FR muda.
    void set_fpscr(uint32_t value);

    // R[0..7] = banco ativo (por SR.RB); R8-R15 independem de RB.
    uint32_t r[16] = {};
    // Banco inativo (R0_BANK..R7_BANK): acessado por stc/ldc *_BANK.
    uint32_t rbank[8] = {};

    // Bit T e auxiliares de SR (evitam multiplicar helpers externos).
    bool t() const { return (sr & kSrT) != 0; }
    void set_t(bool v) { sr = (sr & ~kSrT) | (v ? kSrT : 0u); }
    void set_sr_bit(uint32_t bit, bool v) {
        sr = v ? (sr | bit) : (sr & ~bit);
    }

    uint32_t pc = 0;
    uint32_t pr = 0;
    uint32_t sr = kSrMD | kSrRB | kSrBL | (0xFu << 4); // 0x700000F0 (reset)
    uint32_t fpscr = 0x00040001; // DN=1, RM=01 (para zero); PR=SZ=FR=0

    uint32_t gbr = 0, ssr = 0, spc = 0, vbr = 0, sgr = 0, dbr = 0;
    uint32_t mach = 0, macl = 0, fpul = 0;

    // fr[16] = banco atualmente endereçado pelo bit FR do FPSCR (o que as
    // instruções veem por "fr[i]"); xf[16] = banco oposto (XMTRX sempre usa
    // xf). set_fpscr troca os dois inteiros quando FR muda (modelo da
    // referência MIT, confirmado contra o manual via SLEIGH).
    uint32_t fr[16] = {};
    uint32_t xf[16] = {};

    bool pr_bit() const { return (fpscr & kFpscrPR) != 0; }
    bool sz_bit() const { return (fpscr & kFpscrSZ) != 0; }
    // DRn: palavra alta em fr[n&14], baixa em fr[(n&14)+1] (quirk LE).
    // O valor vai e vem na forma de double (padrao da referencia MIT:
    // getDR/setDR fazem bit_cast; sem isso a conversao uint64->double
    // truncaria o padrao de bits numericamente e o resultado do FADD dp
    // viraria a soma das palavras).
    double get_dr(unsigned n) const {
        const unsigned b = n & 14u;
        return std::bit_cast<double>((uint64_t(fr[b]) << 32) | fr[b + 1]);
    }
    void set_dr(unsigned n, double v) {
        const uint64_t w = std::bit_cast<uint64_t>(v);
        const unsigned b = n & 14u;
        fr[b] = uint32_t(w >> 32);
        fr[b + 1] = uint32_t(w);
    }
    // Escrita em FRn simples (indice de 16 registradores).
    void set_fr(unsigned i, uint32_t v) { fr[i & 0xF] = v; }
    // Par de 8 bytes com SZ=1: reg impar = XD (banco XF), par = DR (banco FR).
    uint32_t* fpair(unsigned rn) { return (rn & 1u) ? &xf[rn & 14u] : &fr[rn & 14u]; }
    const uint32_t* fpair(unsigned rn) const {
        return (rn & 1u) ? &xf[rn & 14u] : &fr[rn & 14u];
    }

    // Executa uma instrucao; desvios com delay slot rodam o slot antes de
    // saltar (o alvo e travado na execucao do desvio, antes do slot);
    // bt/bf saltam imediatamente (sem delay slot).
    StepResult step(Sh4Bus& bus);

    // Hooks documentados (HH05+ implementa de verdade):
    void (*on_trapa)(Sh4State&, uint32_t imm) = nullptr;
    void (*on_ldtlb)(Sh4State&) = nullptr;
    void (*on_cache)(Sh4State&, uint32_t addr, uint32_t kind) = nullptr;
    void (*on_sleep)(Sh4State&) = nullptr;
    // Chamado em opcode ILEGAL e em caso sem semantica (bug do nucleo).
    void (*on_illegal)(Sh4State&, uint32_t addr, uint16_t op) = nullptr;

private:
    // Execucao concreta: retorna alvo se for desvio tomado.
    std::optional<uint32_t> execute(Sh4Bus& bus, uint32_t addr,
                                    const InstrInfo& row, const Decoded& d);

    // delay slot em curso: pc roda o slot e salta para branch_target_.
    bool in_delay_ = false;
    uint32_t branch_target_ = 0;
};

} // namespace hh::sh4
