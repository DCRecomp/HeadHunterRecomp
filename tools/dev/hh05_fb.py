#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
"""HH05.6: quadro exibido a partir de VRAM + registradores do PVR (snapshots do oraculo,
<base>.vram e <base>.pvr) com a mesma regra de runtime/pvr/pvr_video.cpp, e comparacao com
um quadro do hh-run (PPM). Nenhum pixel e gravado fora da pasta indicada (privada).

  hh05_fb.py decode  <base> <saida.ppm>
  hh05_fb.py compare <a.ppm> <b.ppm> [--deltas 8,9] [--min-exact P]
      --deltas: unicas diferencas maximas por canal aceitas num pixel (padrao: nenhuma)
      --min-exact: percentual minimo de pixels identicos (padrao 100)
rc de compare: 0 = dentro das regras; 1 = fora; 2 = arquivo invalido."""
import struct
import sys


def decode(base, out):
    vram = open(base + '.vram', 'rb').read()
    pvr = open(base + '.pvr', 'rb').read()
    if len(vram) != 8 << 20 or len(pvr) != 0x2000:
        raise SystemExit('captura invalida: vram %d, pvr %d bytes' % (len(vram), len(pvr)))
    R = lambda off: struct.unpack_from('<I', pvr, off)[0]
    ctrl, size = R(0x044), R(0x05C)
    fmt = (ctrl >> 2) & 3
    bpp = 3 if fmt == 2 else (4 if fmt == 3 else 2)
    words, lines, mod = (size & 0x3FF) + 1, ((size >> 10) & 0x3FF) + 1, (size >> 20) & 0x3FF
    stride = (words + mod - (1 if mod else 0)) * 4
    inter = (R(0x0D0) >> 4) & 1
    dbl = (not inter) and ((ctrl >> 1) & 1)
    w = words * 4 // bpp
    h = lines * (2 if inter or dbl else 1)
    on = (ctrl & 1) and not ((R(0x0E8) >> 3) & 1)
    sof1, sof2 = R(0x050) & 0x00FFFFFC, R(0x054) & 0x00FFFFFC
    e5 = lambda v: (v << 3) | (v >> 2)
    e6 = lambda v: (v << 2) | (v >> 4)
    at = lambda a: vram[a & (len(vram) - 1)]
    px = bytearray()
    for y in range(h):
        b = ((sof2 if y & 1 else sof1) + (y >> 1) * stride) if inter else sof1 + ((y >> 1) if dbl else y) * stride
        for x in range(w):
            if not on:
                px += b'\0\0\0'
                continue
            a = b + x * bpp
            if fmt == 0:
                v = at(a) | at(a + 1) << 8
                px += bytes((e5((v >> 10) & 31), e5((v >> 5) & 31), e5(v & 31)))
            elif fmt == 1:
                v = at(a) | at(a + 1) << 8
                px += bytes((e5(v >> 11), e6((v >> 5) & 63), e5(v & 31)))
            else:
                px += bytes((at(a + 2), at(a + 1), at(a)))
    open(out, 'wb').write(b'P6\n%d %d\n255\n' % (w, h) + px)
    print('quadro %dx%d fmt=%d entrelacado=%d video=%s FB_R_SIZE=%08X' % (w, h, fmt, inter, 'ligado' if on else 'desligado', size))


def load(p):
    d = open(p, 'rb').read()
    parts = d.split(b'\n', 3)
    if len(parts) != 4 or parts[0] != b'P6' or parts[2] != b'255':
        raise ValueError('%s: nao e PPM P6 de 8 bits' % p)
    w, h = map(int, parts[1].split())
    if len(parts[3]) != w * h * 3:
        raise ValueError('%s: %d bytes de pixels, esperado %d' % (p, len(parts[3]), w * h * 3))
    return w, h, parts[3]


def compare(a, b, deltas, min_exact):
    try:
        wa, ha, pa = load(a)
        wb, hb, pb = load(b)
    except (OSError, ValueError) as e:
        print('ERRO', e)
        return 2
    if (wa, ha) != (wb, hb):
        print('dimensoes diferentes: %dx%d x %dx%d' % (wa, ha, wb, hb))
        return 1
    diff, worst, outside = 0, 0, 0
    for i in range(0, len(pa), 3):
        d = max(abs(pa[i + k] - pb[i + k]) for k in range(3))
        if d:
            diff += 1
            worst = max(worst, d)
            if d not in deltas:
                outside += 1
    n = wa * ha
    exact = 100.0 * (n - diff) / n
    print('%dx%d: iguais %d (%.2f%%), diferentes %d, maior diferenca por canal %d, fora dos deltas aceitos %d'
          % (wa, ha, n - diff, exact, diff, worst, outside))
    return 0 if outside == 0 and exact >= min_exact else 1


if __name__ == '__main__':
    if len(sys.argv) >= 4 and sys.argv[1] == 'decode':
        decode(sys.argv[2], sys.argv[3])
    elif len(sys.argv) >= 4 and sys.argv[1] == 'compare':
        deltas = set()
        if '--deltas' in sys.argv:
            deltas = {int(x) for x in sys.argv[sys.argv.index('--deltas') + 1].split(',')}
        min_exact = float(sys.argv[sys.argv.index('--min-exact') + 1]) if '--min-exact' in sys.argv else 100.0
        sys.exit(compare(sys.argv[2], sys.argv[3], deltas, min_exact))
    else:
        print(__doc__)
        sys.exit(2)
