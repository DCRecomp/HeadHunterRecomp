#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
"""Gera o binario sintetico de entrada do oraculo SLEIGH: os 65.536
opcodes SH-4 (little-endian), cada um seguido de um nop (0x0009) para
isolar o fluxo. Nenhum byte de jogo. Uso: make_all_ops.py <saida.bin>"""
import struct
import sys

with open(sys.argv[1], "wb") as f:
    for op in range(0x10000):
        f.write(struct.pack("<HH", op, 0x0009))
