#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
"""HH05.5: classifica TODAS as diferencas entre dois traces (Flycast x recompilado), sem
parar na primeira. 'espelho' = so muda o segmento (P1 0x8C.. x P2 0xAC..) do mesmo
endereco fisico; o resto e listado para revisao. Linhas: funcao, r0-r15, pr, sr, fpscr, fpul.
uso: hh05_trace_classify.py <flycast.tsv> <recompilado.tsv>"""
import collections, sys
NAMES = ["func"] + ["r%d" % i for i in range(16)] + ["pr", "sr", "fpscr", "fpul"]
A = [l.rstrip("\n").split("\t") for l in open(sys.argv[1])]
B = [l.rstrip("\n").split("\t") for l in open(sys.argv[2])]
mirror = collections.Counter(); other = []
for i, (x, y) in enumerate(zip(A, B)):
    if x[0] != y[0]:
        other.append((i, x[0], "func", x[0], y[0])); break
    for k, (u, v) in enumerate(zip(x, y)):
        if u == v: continue
        uu, vv = int(u, 16), int(v, 16)
        if (uu & 0x1FFFFFFF) == (vv & 0x1FFFFFFF) and {uu >> 29, vv >> 29} <= {4, 5}:
            mirror[(NAMES[k], x[0])] += 1
        else:
            other.append((i, x[0], NAMES[k], u, v))
n = min(len(A), len(B))
print("chamadas comparadas:", n, "| funcoes iguais em todas:", all(a[0] == b[0] for a, b in zip(A, B)))
print("diferencas so de espelho P1/P2:", sum(mirror.values()), "em", len(mirror), "(coluna, funcao)")
for (col, fn), c in sorted(mirror.items())[:20]: print("   ", col, fn, c)
print("outras diferencas:", len(other))
for o in other[:40]: print("   #%d %s %s flycast=%s recompilado=%s" % o)
