#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
"""HH12 V3 (referencia do decodificador do proprio jogo): um quadro exibido pelo jogo original no Flycast, capturado com
o emulador rodando livre (flycast_oracle.py snap-free; a captura por breakpoint deixa o framebuffer preto), contra todos
os quadros do mesmo filme decodificados pelo FFmpeg. O quadro do jogo e amostrado no centro de cada pixel do filme
(area exibida, escala min(640/w, 480/h), centralizada); procura o quadro do FFmpeg de maior PSNR e informa PSNR e erro
maximo por canal. Nenhum pixel sai da pasta privada.

  hh12_v3_livre.py <base_snap> <arquivo.4XM> [--altura-exibida H] [--passo N] [--ffmpeg exe]
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
    if len(sys.argv) < 3:
        print(__doc__)
        return 2
    base, src = sys.argv[1], sys.argv[2]
    ffmpeg = arg('--ffmpeg', 'ffmpeg')
    passo = int(arg('--passo', '2'))
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
    amostras = []  # (indice no quadro do filme, indice no framebuffer)
    for y in range(0, sh, passo):
        fy = int(y0 + (y + 0.5) * scale)
        if fy >= fh:
            continue
        for x in range(0, sw, passo):
            fx = int(x0 + (x + 0.5) * scale)
            amostras.append(((y * w + x) * 3, (fy * fw + fx) * 3))
    ref = bytes(b for _, j in amostras for b in fb[j:j + 3])
    cmd = [ffmpeg, '-v', 'error', '-nostdin', '-i', src, '-map', '0:v:0', '-vf', 'fps=%s' % fps, '-f', 'rawvideo',
           '-pix_fmt', 'rgb24', 'pipe:1']
    p = subprocess.Popen(cmd, stdout=subprocess.PIPE)
    n = w * h * 3
    melhor = None
    k = 0
    while True:
        fr = p.stdout.read(n)
        if len(fr) < n:
            break
        se, mx = 0, 0
        r = 0
        for i, _ in amostras:
            for c in range(3):
                d = fr[i + c] - ref[r]
                r += 1
                se += d * d
                if d > mx or -d > mx:
                    mx = abs(d)
        psnr = 99.0 if se == 0 else 10 * math.log10(255 * 255 * len(ref) / se)
        if melhor is None or psnr > melhor[0]:
            melhor = (psnr, mx, k)
        k += 1
    p.wait()
    print('%d quadros do FFmpeg, %d amostras; melhor: quadro %d, PSNR %.2f dB, erro maximo %d' %
          (k, len(amostras), melhor[2], melhor[0], melhor[1]))
    return 0


if __name__ == '__main__':
    sys.exit(main())
