#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
"""HH14: leitura estatica do AUDIO64.DRV (driver ARM7 da AICA; nunca executado, regra "recompilar, nunca emular").
Desmonta com o Capstone (ferramenta de analise fora do produto: pip install --user capstone==5.0.7, como o N6 do BS) para
um arquivo local (pasta ignorada: o resultado contem o codigo do driver e nao pode ser versionado).

  python tools/dev/hh14_drv.py <AUDIO64.DRV> <saida.asm> [--base 0]

Cada linha: endereco, instrucao e, nos literais ldr [pc, #x], o valor carregado (para seguir tabelas e constantes da AICA).
"""
import argparse
import struct

import capstone

ap = argparse.ArgumentParser()
ap.add_argument('drv')
ap.add_argument('out')
ap.add_argument('--base', type=lambda v: int(v, 0), default=0)
a = ap.parse_args()
with open(a.drv, 'rb') as f:
    code = f.read()
md = capstone.Cs(capstone.CS_ARCH_ARM, capstone.CS_MODE_ARM)
md.detail = True
with open(a.out, 'w', encoding='utf-8') as out:
    for off in range(0, len(code) - 3, 4):  # ARM de 32 bits alinhado; dados aparecem como instrucoes invalidas
        ins = next(md.disasm(code[off:off + 4], a.base + off), None)
        if ins is None:
            out.write('%06X: .word 0x%08X\n' % (a.base + off, struct.unpack_from('<I', code, off)[0]))
            continue
        note = ''
        if ins.mnemonic.startswith('ldr') and '[pc, #' in ins.op_str:
            disp = int(ins.op_str.split('[pc, #')[1].rstrip(']!').split(']')[0], 0)
            lit = off + 8 + disp
            if 0 <= lit <= len(code) - 4:
                note = '    ; =0x%08X' % struct.unpack_from('<I', code, lit)[0]
        out.write('%06X: %-8s %s%s\n' % (ins.address, ins.mnemonic, ins.op_str, note))
print('ok', a.out)
