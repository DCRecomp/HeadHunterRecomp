#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
"""HH05.6: primeiro quadro natural (logo SEGA) contra o Flycast. Dados locais (fora do git):
    HH_SNAPSHOT_DIR/fb_sega.vram e fb_sega.pvr  (flycast_oracle.py snap-free --seconds 11)
    HH_DATA_DIR                                 (hh-extract --runtime)
Roda o hh-run ate o VBlank 400 (rc 0, pelo limite), decodifica o quadro do Flycast com a
mesma regra do pvr_video e compara com o quadro 200 (tela estatica do logo, VBlanks
180-240). Regras (relatorio HH05 §3.4): >= 94% dos pixels identicos e os demais diferindo
por exatamente um passo de quantizacao de 5 bits (8 ou 9 por canal; dither do PVR,
FB_W_CTRL bit 3, nao modelado). Caso negativo embutido: o quadro 400 (outra tela) precisa
ser REJEITADO (rc 1, nao erro de arquivo). Sem os dados, sai com 77 (SKIP).
uso: hh_first_frame.py <hh-run.exe> <hh05_fb.py>"""
import os
import subprocess
import sys
import tempfile

RULES = ['--deltas', '8,9', '--min-exact', '94']


def main():
    run, fb = os.path.abspath(sys.argv[1]), sys.argv[2]
    snap, data = os.environ.get('HH_SNAPSHOT_DIR'), os.environ.get('HH_DATA_DIR')
    if not snap or not data or not os.path.exists(os.path.join(snap, 'fb_sega.vram')):
        print('dados ausentes: teste ignorado')
        return 77
    with tempfile.TemporaryDirectory() as tmp:
        r = subprocess.run([run, '--data', data, '--vblanks', '400', '--rtc', '1700000000', '--capture', tmp,
                            '--every', '200'], capture_output=True, text=True, timeout=600)
        frames = [os.path.join(tmp, f) for f in ('frame_000200.ppm', 'frame_000400.ppm')]
        if r.returncode != 0 or 'ok: limite de VBlanks atingido' not in r.stdout or not all(map(os.path.exists, frames)):
            print('ERRO: hh-run nao chegou ao VBlank 400 (rc=%d)' % r.returncode)
            print(r.stdout[-2000:])
            return 1
        ref = os.path.join(tmp, 'flycast.ppm')
        subprocess.run([sys.executable, '-I', fb, 'decode', os.path.join(snap, 'fb_sega'), ref], check=True)
        cmp = lambda f: subprocess.run([sys.executable, '-I', fb, 'compare', f, ref] + RULES).returncode
        ok, neg = cmp(frames[0]), cmp(frames[1])
        if neg != 1:
            print('ERRO: caso negativo nao foi rejeitado (rc=%d)' % neg)
            return 1
        return ok


if __name__ == '__main__':
    sys.exit(main())
