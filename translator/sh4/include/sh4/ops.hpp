// SPDX-License-Identifier: MIT
// Taxonomia do nucleo SH-4 (SH7091) - HH03.
// Encodings sao fatos de arquitetura (manual SuperH-4/Renesas). A tabela
// declarativa (mask,value)->instrucao fica em src/decoder_table.cpp e e
// verificada por dif contra o oraculo SLEIGH (analysis/hh03/).
#pragma once
#include <cstdint>

#include "sh4/op_enum.hpp"

namespace hh::sh4 {

enum { kNumGpr = 16 };

// Campos padrao de uma instrucao SH-4 de 16 bits.
struct OpcodeFields {
    uint32_t major() const { return op >> 12; }      // bits 15..12
    uint32_t rn() const { return (op >> 8) & 0xF; }  // bits 11..8
    uint32_t rm() const { return (op >> 4) & 0xF; }  // bits 7..4
    uint32_t low4() const { return op & 0xF; }       // bits 3..0
    uint32_t low8() const { return op & 0xFF; }      // bits 7..0
    int32_t imm8() const { return static_cast<int8_t>(op & 0xFF); }
    int32_t disp12() const { return static_cast<int16_t>(op << 4) >> 4; }
    uint32_t op;
};

// Gravura de operandos: como a instrucao trata rn/rm/imm e o que faz.
// Taxonomia derivada mecanicamente das gravuras do oraculo SLEIGH
// (analysis/hh03/shape_list.txt); a semantica do opcode depende dos
// formatos + campos.
enum class OperandFmt : uint16_t {
    none = 0,        // sem operandos (nop, clrt, sett, rte, synco...)
    rn,              // um registrador GPR: dt, cmp/pz, cmp/pl, movt, deslocamentos
    rm_rn,           // Rm, Rn: add, cmp/*, and, or, xor, tst, mov, swap, ext*, shad/shld
    imm_rn,          // #imm8, Rn: add, mov, cmp/eq, tst, and, or, xor
    // memoria -> registrador
    m2r_at_rm,       // mov.X @Rm,Rn
    m2r_at_rm_pos,   // mov.X @Rm+,Rn
    m2r_at_r0_rm,    // mov.X @(R0,Rm),Rn
    m2r_at_disp_rm,  // mov.X @(disp,Rm),Rn
    m2r_at_disp_gbr, // mov.X @(disp,GBR),R0 (destino R0 fixo)
    m2r_at_disp_rm_r0, // mov.b/.w @(disp,Rm),R0: bits 11-8 sao o nibble de
                       // tamanho (mask FF00), endereco em Rm (bits 7-4),
                       // disp = bits 3-0
    movw_pc,         // mov.w @(disp,PC),Rn (texto imprime endereco absoluto)
    movl_pc,         // mov.l @(disp,PC),Rn
    mova,            // mova @(disp,PC),R0
    // registrador -> memoria
    r2m_at_rn,       // mov.X Rm,@Rn
    r2m_at_rn_neg,   // mov.X Rm,@-Rn
    r2m_r0_rn,       // mov.X Rm,@(R0,Rn)
    r2m_disp_rn,     // mov.l Rm,@(disp,Rn) (0x1nm d: Rn livre)
    r2m_disp_rn_r0,  // mov.b/.w R0,@(disp,Rm): bits 11-8 = nibble de tamanho,
                     // endereco em Rm (bits 7-4), disp = bits 3-0
    r2m_disp_gbr,    // mov.X R0,@(disp,GBR) (dado R0 fixo)
    bop_gbr,         // tst.b/and.b/xor.b/or.b #imm8,@(R0,GBR)
    mac_post,        // mac.l/mac.w @Rm+,@Rn+
    // registradores de controle
    stc_creg,        // stc creg,Rn (SR/GBR/VBR/SSR/SPC/SGR/DBR/FPUL/...)
    stc_bank,        // stc Rn_BANK,Rn
    sts_creg,        // sts MACH/MACL/PR/FPUL,Rn
    stcl_creg,       // stc.l creg,@-Rn
    stcl_bank,       // stc.l Rn_BANK,@-Rn
    stsl_creg,       // sts.l creg,@-Rn
    ldc_creg,        // ldc Rm,creg
    ldc_bank,        // ldc Rm,Rn_BANK
    ldcl_creg,       // ldc.l @Rm+,creg
    ldcl_bank,       // ldc.l @Rm+,Rn_BANK
    lds_creg,        // lds Rm,creg (MACH/MACL/PR/FPUL/FPSCR)
    ldsl_creg,       // lds.l @Rm+,creg
    // transferencia / sistema
    trapa,           // trapa #imm8
    cache_abs,       // pref/icbi/ocbi/ocbp/ocbwb @Rm
    tas,             // tas.b @Rm
    movca,           // movca.l R0,@Rn
    movua,           // movua.l @Rn+,R0
    // desvios
    branch_disp8,    // bt/bf/bt.s/bf.s disp8
    branch_disp12,   // bra disp12
    call_disp12,     // bsr disp12
    branch_reg,      // braf/bsrf Rm
    jump_abs,        // jmp/jsr @Rm
    // FPU
    fp_rr,           // FRm, FRn: fadd/fsub/fmul/fdiv/fcmp/eq/fcmp/gt
    fp_mac,          // fmac FR0,FRm,FRn
    fp_1,            // FRn: fneg/fabs/fsqrt/fsrra/fldi0/fldi1
    fp_float,        // float FPUL,FRn
    fp_ftrc,         // ftrc FRn,FPUL
    fp_flds,         // flds FRn,FPUL
    fp_fsts,         // fsts FPUL,FRn
    fp_fcnvsd,       // fcnvsd FPUL,DRn
    fp_fcnvds,       // fcnvds DRn,FPUL
    fp_fsca,         // fsca FPUL,DRn
    fp_fmov_dr,      // fmov DRm,DRn (reg-reg)
    fp_fmov_s_rr,    // fmov.s FRm,FRn (sem linha no atlas; gravura unica)
    fp_fmov_s_m2r,   // fmov.s @Rm,FRn
    fp_fmov_s_m2r_pos, // fmov.s @Rm+,FRn
    fp_fmov_s_m2r_idx, // fmov.s @(R0,Rm),FRn
    fp_fmov_s_r2m,   // fmov.s FRm,@Rn
    fp_fmov_s_r2m_neg, // fmov.s FRm,@-Rn
    fp_fmov_dr_r2m_idx,// fmov DRm,@(R0,Rn) store indexado
    fp_ipr,          // fipr FVm,FVn
    fp_trv,          // ftrv XMTRX,FVn
};

// Registro de informacao de uma instrucao do decodificador.
struct InstrInfo {
    uint32_t mask;
    uint32_t value;
    const char* mnemonic;
    Op op;                   // discriminante semantico (sh4/op_enum.hpp)
    OperandFmt fmt;
    uint16_t has_delay_slot; // 1 = proximo slot e delay slot do fluxo
    uint16_t fpu;            // 1 = instrucao FPU (comporta-se por FPSCR)
};

// Resultado de decodificacao concreta.
struct Decoded {
    uint16_t op;
    uint16_t row;          // indice na tabela (0 = ilegal/nada)
    uint32_t rn;           // valor do campo Rn
    uint32_t rm;           // valor do campo Rm
    int32_t imm8;
    int32_t disp12;
};

// Consulta: qual linha da tabela casa com este opcode (com prioridade
// decidida em tempo de geracao). Retorna nullptr se nao casar (ILEGAL).
const InstrInfo* lookup(uint16_t op);

// Consulta completa: linha + campos extraidos do opcode.
Decoded decode(uint16_t op);

// Acesso a tabela declarativa (para testes de pureza e verificacao).
unsigned tableSize();
const InstrInfo* tableRow(unsigned index);

// Erro de classificacao reportado pela verificacao exaustiva.
struct ClassifyReport {
    unsigned total = 0;      // opcodes verificados
    unsigned illegal = 0;    // classificados ILEGAL
    unsigned decoded = 0;    // decodificados para uma instrucao
};

} // namespace hh::sh4
