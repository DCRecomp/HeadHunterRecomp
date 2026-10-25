#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
"""Converte o log do analyzeHeadless com HH03Oracle.java (UTF-16) na
tabela de verdade SLEIGH: uma linha por opcode, "0xIIII|hhhh|MNEM|FULL|FLOW",
mantendo so os slots pares (o slot impar e o nop separador).
Uso: extract_truth.py <log_ghidra> <saida.txt>"""
import io
import re
import sys

pat = re.compile(r"HH03Oracle\.java> (0x[0-9a-f]+)\|(.*) \(GhidraScript\)\s*$")
out = []
for line in io.open(sys.argv[1], encoding="utf-16"):
    m = pat.search(line.rstrip("\r\n"))
    if not m:
        continue
    slot = int(m.group(1), 16)
    if slot % 2 == 0:
        out.append("0x%04x|%s" % (slot // 2, m.group(2)))
if len(out) != 0x10000:
    sys.exit("ERRO: esperado 65536 opcodes, obtido %d" % len(out))
with io.open(sys.argv[2], "w", encoding="utf-8", newline="\n") as f:
    f.write("\n".join(out) + "\n")
