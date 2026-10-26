#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
"""HH12 V3: diagnostico por bloco 8x8 de um quadro do jogo (Flycast, snap-free) contra quadros vizinhos do FFmpeg.

Para cada bloco da area exibida, o quadro do FFmpeg (de k-r a k+r) com menor erro e esse erro maximo. Separa as duas
hipoteses de diferenca: blocos que batem com outro quadro (o jogo mostra uma mistura de quadros) e blocos que nao batem
com nenhum (decodificacao diferente). Nenhum pixel sai da pasta privada.

  hh12_v3_blocos.py <base_snap> <arquivo.4XM> <k> [--altura-exibida H] [--raio R] [--ffmpeg exe]
"""
import collections
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
    raio = int(arg('--raio', '3'))
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
    scale = min(640.0 / sw, 480.0 / sh)
    x0, y0 = (640 - sw * scale) / 2, (480 - sh * scale) / 2
    a, b = max(0, k - raio), k + raio
    cmd = [ffmpeg, '-v', 'error', '-nostdin', '-i', src, '-map', '0:v:0', '-vf',
           'fps=%s,select=between(n\\,%d\\,%d)' % (fps, a, b), '-fps_mode', 'passthrough', '-f', 'rawvideo',
           '-pix_fmt', 'rgb24', 'pipe:1']
    raw = subprocess.run(cmd, capture_output=True, check=True).stdout
    n = w * h * 3
    frames = [raw[i * n:(i + 1) * n] for i in range(len(raw) // n)]

    def at_fb(x, y):
        fx, fy = int(x0 + (x + 0.5) * scale), int(y0 + (y + 0.5) * scale)
        return (fy * fw + fx) * 3 if fy < fh else None

    votos = collections.Counter()
    piores = []
    for by in range(0, sh - 7, 8):
        for bx in range(0, sw - 7, 8):
            best = None
            for fi, fr in enumerate(frames):
                mx = 0
                for y in range(by, by + 8, 2):
                    for x in range(bx, bx + 8, 2):
                        j = at_fb(x, y)
                        if j is None:
                            continue
                        i = (y * w + x) * 3
                        for c in range(3):
                            d = abs(fr[i + c] - fb[j + c])
                            if d > mx:
                                mx = d
                if best is None or mx < best[0]:
                    best = (mx, a + fi)
            if best[0] <= 12:
                votos[best[1]] += 1
            else:
                votos['nenhum'] += 1
                piores.append((best[0], bx, by, best[1]))
    total = sum(votos.values())
    print('blocos 8x8: %d; por quadro do FFmpeg que bate (erro maximo <= 12):' % total)
    for q, c in sorted(votos.items(), key=lambda t: str(t[0])):
        print('  %s: %d (%.1f%%)' % (q, c, 100.0 * c / total))
    piores.sort(reverse=True)
    print('piores (erro, x, y, quadro mais proximo):', piores[:8])
    return 0


if __name__ == '__main__':
    sys.exit(main())
