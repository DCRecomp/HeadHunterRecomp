#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
"""HH12 V3: um quadro do jogo (Flycast, snap-free) contra o quadro k do FFmpeg reamostrado como o PVR desenha a textura
do filme (filtro bilinear, area exibida escalada min(640/w, 480/h) e centralizada) e quantizado para ARGB1555 (o formato
da textura do jogo, TCW 0x0600003E). Informa PSNR, erro medio e percentis do erro por canal (o maximo e dominado por
bordas e pelo pontilhado do PVR). Nenhum pixel sai da pasta privada.

  hh12_v3_bilinear.py <base_snap> <arquivo.4XM> <k> [--altura-exibida H] [--ffmpeg exe]
"""
import math
import os
import subprocess
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import hh05_fb  # noqa: E402


def arg(name, default=None):
    return sys.argv[sys.argv.index(name) + 1] if name in sys.argv else default


def main():
    if len(sys.argv) < 4:
        print(__doc__)
        return 2
    base, src, k = sys.argv[1], sys.argv[2], int(sys.argv[3])
    ffmpeg = arg('--ffmpeg', 'ffmpeg')
    tmp = base + '.fb.ppm'
    hh05_fb.decode(base, tmp)
    fw, fh, fb = hh05_fb.load(tmp)
    os.remove(tmp)
    probe = subprocess.run(['ffprobe', '-v', 'error', '-select_streams', 'v:0', '-show_entries',
                            'stream=width,height,r_frame_rate', '-of', 'csv=p=0', src],
                           capture_output=True, text=True, check=True).stdout.strip()
    w, h, fps = probe.split(',')
    w, h = int(w), int(h)
    sw, sh = w, int(arg('--altura-exibida', h))
    fr = subprocess.run([ffmpeg, '-v', 'error', '-nostdin', '-i', src, '-map', '0:v:0', '-vf',
                         'fps=%s,select=eq(n\\,%d)' % (fps, k), '-fps_mode', 'passthrough', '-f', 'rawvideo',
                         '-pix_fmt', 'rgb24', 'pipe:1'], capture_output=True, check=True).stdout
    q5 = lambda v: (v >> 3 << 3) | (v >> 5)  # 8 bits -> 5 bits -> 8 bits (ARGB1555)
    scale = min(640.0 / sw, 480.0 / sh)
    dw, dh = sw * scale, sh * scale
    x0, y0 = (640 - dw) / 2, (480 - dh) / 2
    erros = []
    for y in range(int(y0) + 1, min(fh, int(y0 + dh)) - 1):
        v = (y + 0.5 - y0) / scale - 0.5
        iy = max(0, min(sh - 1, int(math.floor(v))))
        jy, ay = min(sh - 1, iy + 1), max(0.0, min(1.0, v - math.floor(v)))
        for x in range(int(x0) + 1, int(x0 + dw) - 1):
            u = (x + 0.5 - x0) / scale - 0.5
            ix = max(0, min(sw - 1, int(math.floor(u))))
            jx, ax = min(sw - 1, ix + 1), max(0.0, min(1.0, u - math.floor(u)))
            j = (y * fw + x) * 3
            for c in range(3):
                p = lambda xx, yy: q5(fr[(yy * w + xx) * 3 + c])
                val = (p(ix, iy) * (1 - ax) + p(jx, iy) * ax) * (1 - ay) + (p(ix, jy) * (1 - ax) + p(jx, jy) * ax) * ay
                erros.append(abs(val - fb[j + c]))
    erros.sort()
    n = len(erros)
    mse = sum(e * e for e in erros) / n
    pct = lambda q: erros[min(n - 1, int(q * n))]
    print('quadro %d: %d amostras; PSNR %.2f dB, erro medio %.2f, p50 %.1f, p90 %.1f, p99 %.1f, p99.9 %.1f, max %.1f' %
          (k, n, 99.0 if mse == 0 else 10 * math.log10(255 * 255 / mse), sum(erros) / n, pct(.5), pct(.9), pct(.99),
           pct(.999), erros[-1]))
    return 0


if __name__ == '__main__':
    sys.exit(main())
