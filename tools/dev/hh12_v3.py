#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
"""HH12 V3: fidelidade do filme convertido contra o decodificador 4X do proprio jogo.

Referencia: o quadro exibido pelo jogo original no Flycast (snapshot na k-esima execucao de 0x8C11E482, a funcao vazia
que o laco do filme chama uma vez por quadro, logo depois de desenhar: mostra o quadro k-1), decodificado da VRAM pela
regra do runtime (hh05_fb.py). Comparado: o quadro do FFmpeg (a fonte do MP4 do Builder) e, com --mp4, o do MP4,
compostos como o provedor nativo (area exibida no canto superior esquerdo, escala min(640/w, 480/h), centralizado).
Procura o melhor alinhamento em k-1 +- janela e informa PSNR e erro maximo por canal. Nenhum pixel sai da pasta privada.

  hh12_v3.py <base_snap> <arquivo.4XM> <k> [--largura-exibida W --altura-exibida H] [--janela N] [--ffmpeg exe]
             [--salvar <pasta>]
rc: 0 = medido (o veredito e de quem chama, pelos limites do plano); 2 = entrada invalida."""
import math
import os
import subprocess
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import hh05_fb  # noqa: E402


def arg(name, default=None):
    return sys.argv[sys.argv.index(name) + 1] if name in sys.argv else default


def ffmpeg_frames(ffmpeg, src, first, count, w, h, fps):
    """Quadros [first, first+count) do 4X em RGB24, a taxa constante (como o conversor do Builder)."""
    # fps= repete o quadro sem chunk (taxa constante, como o -fps_mode cfr do Builder) antes de escolher pelo indice
    cmd = [ffmpeg, '-v', 'error', '-nostdin', '-i', src, '-map', '0:v:0', '-vf',
           'fps=%s,select=between(n\\,%d\\,%d)' % (fps, first, first + count - 1), '-fps_mode', 'passthrough',
           '-f', 'rawvideo', '-pix_fmt', 'rgb24', 'pipe:1']
    raw = subprocess.run(cmd, capture_output=True, check=True).stdout
    n = w * h * 3
    return [raw[i * n:(i + 1) * n] for i in range(len(raw) // n)]


def compose(frame, w, h, sw, sh, filt):
    """640x480 RGB24 como o provedor nativo (nearest) ou com filtro bilinear (o PVR filtra a textura)."""
    out = bytearray(640 * 480 * 3)
    scale = min(640.0 / sw, 480.0 / sh)
    dw, dh = int(sw * scale + 0.5), int(sh * scale + 0.5)
    x0, y0 = (640 - dw) // 2, (480 - dh) // 2
    for y in range(dh):
        fy = (y + 0.5) / scale - 0.5
        for x in range(dw):
            fx = (x + 0.5) / scale - 0.5
            o = ((y0 + y) * 640 + x0 + x) * 3
            if filt:
                ix, iy = max(0, min(sw - 1, int(math.floor(fx)))), max(0, min(sh - 1, int(math.floor(fy))))
                jx, jy = min(sw - 1, ix + 1), min(sh - 1, iy + 1)
                ax, ay = max(0.0, min(1.0, fx - ix)), max(0.0, min(1.0, fy - iy))
                for c in range(3):
                    p00 = frame[(iy * w + ix) * 3 + c]
                    p01 = frame[(iy * w + jx) * 3 + c]
                    p10 = frame[(jy * w + ix) * 3 + c]
                    p11 = frame[(jy * w + jx) * 3 + c]
                    v = (p00 * (1 - ax) + p01 * ax) * (1 - ay) + (p10 * (1 - ax) + p11 * ax) * ay
                    out[o + c] = int(v + 0.5)
            else:
                sx, sy = min(sw - 1, int(x / scale)), min(sh - 1, int(y / scale))
                out[o:o + 3] = frame[(sy * w + sx) * 3:(sy * w + sx) * 3 + 3]
    return bytes(out)


def psnr(a, b):
    se = 0
    mx = 0
    for i in range(len(a)):
        d = a[i] - b[i]
        se += d * d
        if abs(d) > mx:
            mx = abs(d)
    mse = se / len(a)
    return (99.0 if mse == 0 else 10 * math.log10(255 * 255 / mse)), mx


def main():
    if len(sys.argv) < 4:
        print(__doc__)
        return 2
    base, src, k = sys.argv[1], sys.argv[2], int(sys.argv[3])
    ffmpeg = arg('--ffmpeg', 'ffmpeg')
    janela = int(arg('--janela', '3'))
    tmp = base + '.fb.ppm'
    hh05_fb.decode(base, tmp)
    fw, fh, fb = hh05_fb.load(tmp)
    os.remove(tmp)
    if fw != 640 or fh > 480:  # o jogo pode exibir menos linhas (FB_R_SIZE de 476): compara as primeiras fh
        print('quadro do Flycast %dx%d (esperado 640 x ate 480)' % (fw, fh))
        return 2
    probe = subprocess.run(['ffprobe', '-v', 'error', '-select_streams', 'v:0', '-show_entries', 'stream=width,height,r_frame_rate',
                            '-of', 'csv=p=0', src], capture_output=True, text=True, check=True).stdout.strip()
    w, h, fps = probe.split(',')
    w, h = int(w), int(h)
    sw, sh = int(arg('--largura-exibida', w)), int(arg('--altura-exibida', h))
    first = max(0, k - 1 - janela)
    frames = ffmpeg_frames(ffmpeg, src, first, 2 * janela + 1, w, h, fps)
    best = None
    for i, fr in enumerate(frames):
        for filt in (False, True):
            p, mx = psnr(compose(fr, w, h, sw, sh, filt)[:640 * fh * 3], fb)
            if best is None or p > best[0]:
                best = (p, mx, first + i, filt)
            print('quadro %d %-9s PSNR %.2f dB, erro maximo %d' % (first + i, 'bilinear' if filt else 'vizinho', p, mx))
    print('melhor: quadro %d (%s), PSNR %.2f dB, erro maximo %d; esperado k-1 = %d' %
          (best[2], 'bilinear' if best[3] else 'vizinho', best[0], best[1], k - 1))
    if arg('--salvar'):
        d = arg('--salvar')
        os.makedirs(d, exist_ok=True)
        fr = frames[best[2] - first]
        with open(os.path.join(d, 'fly_%d.ppm' % k), 'wb') as f:
            f.write(b'P6\n640 %d\n255\n' % fh + bytes(fb))
        with open(os.path.join(d, 'ff_%d.ppm' % k), 'wb') as f:
            f.write(b'P6\n640 480\n255\n' + compose(fr, w, h, sw, sh, best[3]))
    return 0


if __name__ == '__main__':
    sys.exit(main())
