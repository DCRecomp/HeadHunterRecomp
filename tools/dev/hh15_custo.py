#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
"""HH15.11: a estrutura de custo de uma funcao gerada, para escrever a elevada com o mesmo caminho observavel.

Imprime, da funcao f_<endereco> do C++ gerado (privado), so o esqueleto: rotulos, HH_CYCLES, HH_POLL (back-edges),
desvios, chamadas e retornos, com a instrucao SH-4 de cada desvio como comentario. Nao imprime bytes do executavel.

uso: hh15_custo.py <pasta do gerado> <endereco> [<endereco> ...]
"""
import glob
import os
import re
import sys

KEEP = re.compile(r'^(L_[0-9A-F]+:;|.*HH_CYCLES|.*HH_POLL|.*goto |.*HH_CALL|.*HH_RET|.*HH_WAIT|.*if \(|.*return;|.*switch|'
                  r'.*case |.*sh4_dispatch|.*tgt)')


def main():
    gen, addrs = sys.argv[1], [a.upper().removeprefix('0X') for a in sys.argv[2:]]
    files = sorted(glob.glob(os.path.join(gen, 'funcs_*.cpp')))
    for a in addrs:
        sig = 'void f_%s(' % a
        for f in files:
            lines = open(f, encoding='utf-8').read().splitlines()
            try:
                i = next(k for k, l in enumerate(lines) if l.startswith(sig))
            except StopIteration:
                continue
            print('== %s (%s:%d)' % (a, os.path.basename(f), i + 1))
            last = ''
            for l in lines[i + 1:]:
                if l.startswith('}'):
                    break
                s = l.strip()
                if s.startswith('// 8C'):
                    last = s
                    continue
                if KEEP.match(s):
                    note = ('   ' + last) if ('if (' in s or 'goto' in s or 'HH_RET' in s or 'HH_CALL' in s) else ''
                    print('  ' + s + note)
            break
        else:
            print('== %s: nao encontrada' % a)


if __name__ == '__main__':
    main()
