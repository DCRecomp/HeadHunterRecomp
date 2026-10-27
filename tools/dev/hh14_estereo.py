#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
"""HH14: leiaute do ADPCM estereo dos WAV do Head Hunter (docs/planos/HH14-PLANO.md, fase C). Imprime so a correlacao
esquerda/direita de cada leiaute candidato (nenhum byte de audio).

  python -I tools/dev/hh14_estereo.py <arquivo .ADP estereo>
"""
import math
import struct
import sys

# Decodifica com o ADPCM da AICA (mesmo algoritmo de
# native/audio/aica_adpcm.cpp) cada canal segundo um leiaute candidato e mede a correlacao L/R.
DIFF = [1, 3, 5, 7, 9, 11, 13, 15, -1, -3, -5, -7, -9, -11, -13, -15]
SCALE = [0x0E6, 0x0E6, 0x0E6, 0x0E6, 0x133, 0x199, 0x200, 0x266] * 2


def decode(nibbles):
    pred, step, out = 0, 0x7F, []
    for n in nibbles:
        pred += int(step * DIFF[n] / 8)
        pred = max(-32768, min(32767, pred))
        step = max(127, min(24576, step * SCALE[n] // 256))
        out.append(pred)
    return out


def nib(bs):
    for b in bs:
        yield b & 15
        yield b >> 4


def corr(l, r):
    n = min(len(l), len(r))
    sl = sum(x * x for x in l[:n]); sr = sum(x * x for x in r[:n]); slr = sum(l[i] * r[i] for i in range(n))
    return slr / math.sqrt(max(sl * sr, 1))


raw = open(sys.argv[1], 'rb').read()
i = raw.index(b'data')
size = struct.unpack_from('<I', raw, i + 4)[0]
d = raw[i + 8:i + 8 + min(size, 400000)]
cands = {'nibble (L baixo, R alto)': (decode([b & 15 for b in d]), decode([b >> 4 for b in d])),
         'byte alternado': (decode(nib(d[0::2])), decode(nib(d[1::2]))),
         'metades': (decode(nib(d[:len(d) // 2])), decode(nib(d[len(d) // 2:])))}
for blk in (16, 32, 64, 128, 256, 512, 1024, 2048, 4096, 8192, 16384, 32768):
    L = b''.join(d[k:k + blk] for k in range(0, len(d), 2 * blk))
    R = b''.join(d[k + blk:k + 2 * blk] for k in range(0, len(d), 2 * blk))
    cands['blocos de %d' % blk] = (decode(nib(L)), decode(nib(R)))
for k, (l, r) in sorted(cands.items(), key=lambda kv: -corr(*kv[1])):
    print('%-28s %.3f' % (k, corr(l, r)))
