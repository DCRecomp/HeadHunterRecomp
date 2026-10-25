#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
"""HH03: gera translator/sh4/src/decoder_table.inc e
         translator/sh4/include/sh4/op_enum.hpp a partir do atlas de shapes
         (tools/sh4-tablegen/atlas_shapes.txt).

Uso (na raiz do repositorio):
    python tools/sh4-tablegen/gen_decoder_table.py --truth <sleigh_truth.txt>
A tabela de verdade SLEIGH e regenerada conforme tools/sh4-tablegen/README.md.

Cada linha do atlas e um retangulo puro (mask,value)->(mnem,shape) ja
verificado contra o oraculo SLEIGH. Aqui so autoramos o mapeamento
shape -> OperandFmt (taxonomia em translator/sh4/include/sh4/ops.hpp),
com flags delay-slot e fpu, e emitimos a tabela em C++.

Ordem de emissao: popcount(mask) desc, depois mask, value (casamento
linear; com retangulos disjuntos a ordem e so determinismo).

Verificacoes antes de gravar:
  - todos os shapes do atlas tem OperandFmt;
  - nenhuma divergencia mnemonica com sleigh_truth2.txt (spot completo);
  - cobertura completa dos 65536 opcodes (legal decodificado, ilegal fora).
"""
import argparse
import io
import os
import re
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
ap = argparse.ArgumentParser()
ap.add_argument("--truth", required=True, help="tabela de verdade SLEIGH (extract_truth.py)")
ap.add_argument("--atlas", default=os.path.join(HERE, "atlas_shapes.txt"))
ap.add_argument("--root", default=os.path.normpath(os.path.join(HERE, "..", "..")),
                help="raiz do repositorio (destino dos arquivos gerados)")
args = ap.parse_args()

sys.stdout = io.TextIOWrapper(sys.stdout.buffer, encoding="utf-8", errors="replace")

# --- leitura do oraculo para verificacao final ---
truth_mnem = {}
for line in io.open(args.truth, encoding="utf-8"):
    parts = line.rstrip().split("|")
    idx = int(parts[0], 16)
    truth_mnem[idx] = parts[2] if parts[2] != "ILLEGAL" else None

# Divergencia 0xFFFD (1 op entre 65536, documentada):
#   - SLEIGH (sleigh_truth2.txt) etiqueta 0xFFFD como "fsca FPUL,dr14";
#   - o padrao MIT do manual ("1111nnn011111101" em sh4_isa.inc) exige
#     bit 8 = 0, porque o bit 8 seleciona a paridade do par DRn (pares
#     de registrador double sao pares: dr0..dr14). Em 0xFFFD (bit 8 = 1)
#     NENHUM padrao MIT casa: FSCA exige 0, FTRV exige bits 9-8 = 01 e
#     FRCHG/FSCHG sao encodings unicos diferentes. Confirmado no modulo
#     GPL externo dream-recomp (decoder.map: FSCA = {0xF1FF,0xF0FD}).
#   No SLEIGH o simbolo FPUL do padrao nao restringe bits e a tripla
#   sobreposicao DRN_0/FRN_1/FRN_2 em (9,11) resolve para o mesmo dr14:
#   duplicacacao artificial do encoding par. Manual + MIT + uso real
#   vencem: nossa tabela deixa 0xFFFD ILEGAL.
MANUAL_WINS = {0xFFFD}

# Instrucoes so do SH-4A (HH04, 2026-10-24). O modulo SLEIGH SuperH4 tambem decodifica
# extensoes do SH-4A, mas o SH7091 do Dreamcast e um SH-4: nele estes opcodes sao
# ilegais. Evidencias independentes: (1) o decodificador do Blue Stinger (MIT, manual
# SH-4) nao as tem; (2) o decodificador do oraculo externo dream-recomp tambem nao;
# (3) o teste cruzado tests/hh04_decoder_cross.cpp mostrou que sao as UNICAS
# divergencias de legalidade entre os dois decodificadores (50 opcodes).
SH4A_ONLY = {"fpchg", "icbi", "movua.l", "synco"}

# --- leitura do atlas de shapes (linhas de dados sem #-comentario) ---
rects = []  # (mnem, shape, mask, value)
for line in io.open(args.atlas, encoding="utf-8"):
    sline = line.strip()
    if not sline or sline.startswith("#"):
        continue
    head, shape, tail = [s.strip() for s in line.split("|")]
    mnem, mpart, vpart = head.split()
    mask = int(mpart.split("=")[1], 16)
    value = int(vpart.split("=")[1], 16)
    rects.append((mnem, shape, mask, value))

# --- mapeamento shape -> OperandFmt (autoria unica; verificado abaixo) ---
FMT = {
    # sem operando / registrador unico
    "nop": "none", "clrt": "none", "clrs": "none", "sets": "none",
    "sett": "none", "div0u": "none", "clrmac": "none", "sleep": "none",
    "ldtlb": "none", "synco": "none", "rts": "none", "rte": "none",
    "frchg": "none", "fpchg": "none", "fschg": "none",
    "dt rN": "rn", "cmp/pz rN": "rn", "cmp/pl rN": "rn", "movt rN": "rn",
    "shal rN": "rn", "shar rN": "rn", "shll rN": "rn",
    "shll2 rN": "rn", "shll8 rN": "rn", "shll16 rN": "rn",
    "shlr rN": "rn", "shlr2 rN": "rn", "shlr8 rN": "rn",
    "shlr16 rN": "rn", "rotl rN": "rn", "rotr rN": "rn",
    "rotcl rN": "rn", "rotcr rN": "rn",
    # dois registradores
    "add rN,rN": "rm_rn", "addc rN,rN": "rm_rn", "addv rN,rN": "rm_rn",
    "sub rN,rN": "rm_rn", "subc rN,rN": "rm_rn", "subv rN,rN": "rm_rn",
    "cmp/eq rN,rN": "rm_rn", "cmp/hs rN,rN": "rm_rn", "cmp/ge rN,rN": "rm_rn",
    "cmp/hi rN,rN": "rm_rn", "cmp/gt rN,rN": "rm_rn", "cmp/str rN,rN": "rm_rn",
    "and rN,rN": "rm_rn", "or rN,rN": "rm_rn", "xor rN,rN": "rm_rn",
    "tst rN,rN": "rm_rn", "xtrct rN,rN": "rm_rn",
    "mul.l rN,rN": "rm_rn", "muls.w rN,rN": "rm_rn", "mulu.w rN,rN": "rm_rn",
    "dmuls.l rN,rN": "rm_rn", "dmulu.l rN,rN": "rm_rn",
    "div1 rN,rN": "rm_rn", "div0s rN,rN": "rm_rn",
    "mov rN,rN": "rm_rn", "not rN,rN": "rm_rn",
    "swap.b rN,rN": "rm_rn", "swap.w rN,rN": "rm_rn",
    "neg rN,rN": "rm_rn", "negc rN,rN": "rm_rn",
    "extu.b rN,rN": "rm_rn", "extu.w rN,rN": "rm_rn",
    "exts.b rN,rN": "rm_rn", "exts.w rN,rN": "rm_rn",
    "shad rN,rN": "rm_rn", "shld rN,rN": "rm_rn",
    # imediato
    "add #A,rN": "imm_rn", "mov #A,rN": "imm_rn",
    "cmp/eq #A,rN": "imm_rn", "tst #A,rN": "imm_rn",
    "and #A,rN": "imm_rn", "or #A,rN": "imm_rn", "xor #A,rN": "imm_rn",
    "trapa #A": "trapa",
    # memoria -> registrador
    "mov.b @rN,rN": "m2r_at_rm", "mov.w @rN,rN": "m2r_at_rm",
    "mov.l @rN,rN": "m2r_at_rm",
    "mov.b @rN+,rN": "m2r_at_rm_pos", "mov.w @rN+,rN": "m2r_at_rm_pos",
    "mov.l @rN+,rN": "m2r_at_rm_pos",
    "mov.b @(rN,rN),rN": "m2r_at_r0_rm", "mov.w @(rN,rN),rN": "m2r_at_r0_rm",
    "mov.l @(rN,rN),rN": "m2r_at_r0_rm",
    "mov.b @(A,rN),rN": "m2r_at_disp_rm_r0", "mov.w @(A,rN),rN": "m2r_at_disp_rm_r0",
    "mov.l @(A,rN),rN": "m2r_at_disp_rm",
    "mov.b @(A,C),rN": "m2r_at_disp_gbr", "mov.w @(A,C),rN": "m2r_at_disp_gbr",
    "mov.l @(A,C),rN": "m2r_at_disp_gbr",
    "mov.w A,rN": "movw_pc", "mov.l A,rN": "movl_pc", "mova A,rN": "mova",
    # registrador -> memoria
    "mov.b rN,@rN": "r2m_at_rn", "mov.w rN,@rN": "r2m_at_rn",
    "mov.l rN,@rN": "r2m_at_rn",
    "mov.b rN,@-rN": "r2m_at_rn_neg", "mov.w rN,@-rN": "r2m_at_rn_neg",
    "mov.l rN,@-rN": "r2m_at_rn_neg",
    "mov.b rN,@(rN,rN)": "r2m_r0_rn", "mov.w rN,@(rN,rN)": "r2m_r0_rn",
    "mov.l rN,@(rN,rN)": "r2m_r0_rn",
    "mov.b rN,@(A,rN)": "r2m_disp_rn_r0", "mov.w rN,@(A,rN)": "r2m_disp_rn_r0",
    "mov.l rN,@(A,rN)": "r2m_disp_rn",
    "mov.b rN,@(A,C)": "r2m_disp_gbr", "mov.w rN,@(A,C)": "r2m_disp_gbr",
    "mov.l rN,@(A,C)": "r2m_disp_gbr",
    "and.b #A,@(rN,C)": "bop_gbr", "or.b #A,@(rN,C)": "bop_gbr",
    "xor.b #A,@(rN,C)": "bop_gbr", "tst.b #A,@(rN,C)": "bop_gbr",
    "mac.l @rN+,@rN+": "mac_post", "mac.w @rN+,@rN+": "mac_post",
    # registradores de controle
    "stc C,rN": "stc_creg", "stc CK,rN": "stc_bank",
    "sts C,rN": "sts_creg",
    "stc.l C,@-rN": "stcl_creg", "stc.l CK,@-rN": "stcl_bank",
    "sts.l C,@-rN": "stsl_creg",
    "ldc rN,C": "ldc_creg", "ldc rN,CK": "ldc_bank",
    "ldc.l @rN+,C": "ldcl_creg", "ldc.l @rN+,CK": "ldcl_bank",
    "lds rN,C": "lds_creg", "lds.l @rN+,C": "ldsl_creg",
    # sistema
    "pref @rN": "cache_abs", "icbi @rN": "cache_abs", "ocbi @rN": "cache_abs",
    "ocbp @rN": "cache_abs", "ocbwb @rN": "cache_abs",
    "tas.b @rN": "tas", "movca.l rN,@rN": "movca",
    "movua.l @rN+,rN": "movua",
    # desvios
    "bt A": "branch_disp8", "bf A": "branch_disp8",
    "bt/s A": "branch_disp8", "bf/s A": "branch_disp8",
    "bra A": "branch_disp12", "bsr A": "call_disp12",
    "braf rN": "branch_reg", "bsrf rN": "branch_reg",
    "jmp @rN": "jump_abs", "jsr @rN": "jump_abs",
    # FPU
    "fadd frN,frN": "fp_rr", "fsub frN,frN": "fp_rr",
    "fmul frN,frN": "fp_rr", "fdiv frN,frN": "fp_rr",
    "fcmp/eq frN,frN": "fp_rr", "fcmp/gt frN,frN": "fp_rr",
    "fmac frN,frN,frN": "fp_mac",
    "fneg frN": "fp_1", "fabs frN": "fp_1", "fsqrt frN": "fp_1",
    "fsrra frN": "fp_1", "fldi0 frN": "fp_1", "fldi1 frN": "fp_1",
    "float C,frN": "fp_float", "ftrc frN,C": "fp_ftrc",
    "flds frN,C": "fp_flds", "fsts C,frN": "fp_fsts",
    "fcnvsd C,drN": "fp_fcnvsd", "fcnvds drN,C": "fp_fcnvds",
    "fsca C,drN": "fp_fsca",
    "fmov frN,frN": "fp_fmov_dr", "fmov frN,@(rN,rN)": "fp_fmov_dr_r2m_idx",
    "fmov.s frN,frN": "fp_fmov_s_rr",
    "fmov.s @rN,frN": "fp_fmov_s_m2r", "fmov.s @rN+,frN": "fp_fmov_s_m2r_pos",
    "fmov.s @(rN,rN),frN": "fp_fmov_s_m2r_idx",
    "fmov.s frN,@rN": "fp_fmov_s_r2m", "fmov.s frN,@-rN": "fp_fmov_s_r2m_neg",    "fipr fvN,fvN": "fp_ipr", "ftrv xmtrx,fvN": "fp_trv",
}

missing = sorted({r[1] for r in rects} - set(FMT))
if missing:
    print("ERRO: shapes sem mapeamento:")
    for s in missing:
        print("   ", s)
    sys.exit(1)
print("shapes mapeados:", len({r[1] for r in rects}))

# --- flags ---
DELAY = {"bra", "bsr", "bt/s", "bf/s", "rts", "rte", "jmp", "jsr",
         "braf", "bsrf"}
FPU = {"fadd", "fsub", "fmul", "fdiv", "fcmp/eq", "fcmp/gt", "fmac",
       "fneg", "fabs", "fsqrt", "fsrra", "fldi0", "fldi1", "float",
       "ftrc", "flds", "fsts", "fcnvsd", "fcnvds", "fsca",
       "fmov", "fmov.s", "fipr", "ftrv", "frchg", "fpchg", "fschg"}

rows = []
excluded = set()
for mnem, shape, mask, value in rects:
    if mnem in SH4A_ONLY:
        for op in range(0x10000):
            if (op & mask) == value:
                excluded.add(op)
        continue
    rows.append({
        "mask": mask, "value": value, "mnem": mnem,
        "fmt": FMT[shape], "shape": shape,
        "delay": 1 if mnem in DELAY else 0,
        "fpu": 1 if mnem in FPU else 0,
    })

# Ordena as linhas por (mnem) para o enum Op ser estavel em geracoes novas.
def op_ident(m):
    out = []
    for ch in m:
        if ch.isalnum():
            out.append(ch.upper())
        else:
            out.append("_")
    return "OP_" + "".join(out)

op_names = sorted({r["mnem"] for r in rows})
op_enum = {m: op_ident(m) for m in op_names}

# ordem linear: mais especifico primeiro (determinismo)
rows.sort(key=lambda r: (-bin(r["mask"]).count("1"), r["mask"], r["value"]))

# --- verificacao final: cada opcode da tabela da o mnemonico do oraculo ---
bad = 0
for op in range(0x10000):
    if op in MANUAL_WINS or op in excluded:
        continue  # 0xFFFD: ilegal por manual/MIT/dream-recomp (documentado)
    hit = None
    for r in rows:
        if (op & r["mask"]) == r["value"]:
            hit = r
            break
    want = truth_mnem[op]
    got = hit["mnem"] if hit else None
    if got != want:
        bad += 1
        if bad < 10:
            print("DIF %04X: tabela=%s verdade=%s" % (op, got, want))
print("diferencas mnemonicas vs oraculo (fora da excecao):", bad)
if bad != 0:
    print("ERRO: divergencia nao documentada fora de MANUAL_WINS")
    sys.exit(1)
# A unica divergencia documentada: 0xFFFD e ILEGAL em nossa tabela
# (nenhuma linha casa) enquanto SLEIGH etiqueta fsca dr14:
if any((0xFFFD & r["mask"]) == r["value"] for r in rows):
    print("ERRO: 0xFFFD NAO pode casar nenhuma linha da tabela (manual vence)")
    sys.exit(1)
print("excecao aplicada: 0xFFFD ilegal (1 divergencia documentada vs SLEIGH)")
if any((op & r["mask"]) == r["value"] for op in excluded for r in rows):
    print("ERRO: opcode exclusivo do SH-4A ainda casa uma linha da tabela")
    sys.exit(1)
print("excluidos (so SH-4A, ilegais no SH7091):", len(excluded), "opcodes:", ", ".join(sorted(SH4A_ONLY)))

dec = sum(1 for r in rows)  # sanity
print("linhas:", len(rows))

# --- emissao ---
def op_ident(m):
    out = []
    for ch in m:
        if ch.isalnum():
            out.append(ch.upper())
        else:
            out.append("_")
    return "OP_" + "".join(out)

op_names = sorted({r["mnem"] for r in rows})

def fmt_c(f):
    return "OperandFmt::" + f

# O enum Op vive num header propriado (ligacao externa): o campo
# InstrInfo::op referencia o tipo; o .inc (por TU) so usa os valores.
with io.open(os.path.join(args.root, "translator/sh4/include/sh4/op_enum.hpp"), "w", encoding="utf-8") as f:
    f.write("// SPDX-License-Identifier: MIT\n")
    f.write("// GERADO por tools/sh4-tablegen/gen_decoder_table.py - HH03.\n")
    f.write("// Discriminante semantico de cada instrucao classificada pelo atlas\n")
    f.write("// (um identificador por mnemonico do oraculo). Nao editar a mao.\n")
    f.write("#pragma once\n#include <cstdint>\n\nnamespace hh::sh4 {\n\n")
    f.write("enum class Op : uint16_t {\n")
    for m in op_names:
        f.write("    %s,\n" % op_ident(m))
    f.write("};\n\n} // namespace hh::sh4\n")

with io.open(os.path.join(args.root, "translator/sh4/src/decoder_table.inc"), "w", encoding="utf-8") as f:
    f.write("// SPDX-License-Identifier: MIT\n")
    f.write("// GERADO por tools/sh4-tablegen/gen_decoder_table.py - HH03.\n")
    f.write("// Atlas: retangulos puros (mask,value)->(mnemonico,shape) verificados\n")
    f.write("// contra oraculo SLEIGH (tools/sh4-tablegen/README.md, derivado do\n")
    f.write("// modulo Ghidra SuperH4, manual Renesas SH-4). Nao editar a mao.\n")
    f.write("// Requer op_enum.hpp (enum Op) e ops.hpp (InstrInfo) incluidos antes.\n\n")
    f.write("constexpr unsigned kTableSize = %u;\n" % len(rows))
    f.write("constexpr InstrInfo kTable[kTableSize] = {\n")
    for r in rows:
        f.write('    {0x%04x, 0x%04x, "%s", Op::%s, %s, %u, %u},\n'
                % (r["mask"], r["value"], r["mnem"], op_ident(r["mnem"]),
                   fmt_c(r["fmt"]), r["delay"], r["fpu"]))
    f.write("};\n")

print("op_enum.hpp + decoder_table.inc gravados (%d ops distintos, %d linhas)"
      % (len(op_names), len(rows)))
