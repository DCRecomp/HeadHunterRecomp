#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
"""HH14: portoes do audio nativo (docs/planos/HH14-PLANO.md) no roteiro newgame_area1_nativo (filmes e audio nativos, o
padrao da janela; 5 600 VBlanks: logos, titulo com musica, menus, cena, primeira area com movimento).

A1  o driver reconstruido atende tudo: rc 0, nenhum comando desconhecido, key-on e eventos de reposicao dos streams,
    streams abertos em STREAMS (nenhum .ADP ausente);
A2  nenhum acesso a AICA alcancavel no modo nativo (sonda HH_HW_PROBE): RAM e registradores de som nunca; so o relogio da
    AICA (RTC, tempo: HH15); o Audio64 nunca programa o DMA do G2 (SB 0x005F7800-0x005F781C);
A3  determinismo: duas execucoes produzem o mesmo audio (SHA-256 do WAV);
A4  niveis: musica do titulo nos dois lados, sem lado dominante (|L/R| < 2) e no nivel da musica do Flycast (RMS 500-8000);
    gameplay com som; saturacao rara (< 0,1 % das amostras);
A5  oraculo (HH14_ORACULO, captura do Flycast com o driver real): a sombra das portas 14/15 da musica do titulo no primeiro
    MultiPlay (SA, PCMS, LSA, LEA, OCT, FNS, TL, DISDL, DIPAN, ISEL, IMXL) igual a do driver nativo.

uso: hh14_audio.py <hh-run.exe> <raiz do projeto>
ambiente: HH_DATA_DIR (sem ele = SKIP, rc 77); HH14_ORACULO (pasta com sombra_titulo.txt; obrigatoria com HH_EXIGIR_DADOS=1).
"""
import hashlib
import math
import os
import re
import struct
import subprocess
import sys
import tempfile

RUN, ROOT = (os.path.abspath(a) for a in sys.argv[1:3])
DATA = os.environ.get('HH_DATA_DIR', '')
SCRIPT = os.path.join(ROOT, 'tools', 'dist', 'roteiros', 'newgame_area1_nativo.txt')
VBLANKS, RTC = 5600, 1700000000
if not os.path.isfile(os.path.join(DATA, 'disc.manifest')):
    print('SKIP: defina HH_DATA_DIR')
    sys.exit(77)
fails = 0


def check(name, cond, info=''):
    global fails
    print('[%s] %s%s' % (' ok ' if cond else 'FAIL', name, (' (%s)' % info) if info else ''))
    fails += not cond


def run(tmp, tag, extra=None):
    wav = os.path.join(tmp, tag + '.wav')
    env = dict(os.environ, HH_INPUT_SCRIPT=SCRIPT, HH_AUDIO_WAV=wav)
    for k in ('HH_AUDIO_LOG', 'HH_HW_PROBE', 'HH_ARQUIVOS_LOG', 'HH_AUDIO_SEM_DSP'):
        env.pop(k, None)
    env.update(extra or {})
    p = subprocess.run([RUN, '--data', DATA, '--vblanks', str(VBLANKS), '--rtc', str(RTC), '--video', 'nativo', '--audio',
                        'nativo'], capture_output=True, text=True, env=env)
    return p, wav


def wav_pcm(path):
    with open(path, 'rb') as f:
        d = f.read()
    n = (len(d) - 44) // 4
    return struct.unpack_from('<%dh' % (2 * n), d, 44), n


def rms(x):
    return math.sqrt(sum(v * v for v in x) / max(len(x), 1))


with tempfile.TemporaryDirectory() as tmp:
    probe = os.path.join(tmp, 'sonda.tsv')
    a, wav_a = run(tmp, 'a', {'HH_HW_PROBE': probe, 'HH_AUDIO_LOG': '80', 'HH_ARQUIVOS_LOG': '1'})
    b, wav_b = run(tmp, 'b')
    summ = re.search(r'\[audio\] nativo: [^\n]*', a.stderr)
    line = summ.group(0) if summ else ''
    num = lambda pat: int(m.group(1)) if (m := re.search(pat, line)) else None
    # A1
    check('A1: executa (rc 0) e resume o audio', a.returncode == 0 and bool(line), a.stderr.strip()[-300:])
    check('A1: nenhum comando desconhecido', num(r'desconhecidos (\d+)') == 0, line[:200])
    check('A1: key-on e eventos', (num(r'key-on (\d+)') or 0) >= 40 and (num(r'eventos (\d+)') or 0) >= 50, line[:200])
    opens = re.findall(r'\[arquivos\] abre (\S+\.ADP) \((\w+)\)', a.stderr)
    check('A1: streams abertos em STREAMS', bool(opens) and all(n.startswith('STREAMS/') and r == 'ok' for n, r in opens),
          str(opens[:6]))
    # A2
    rows = []
    if os.path.isfile(probe):
        with open(probe, encoding='utf-8') as f:
            rows = [l.rstrip('\n').split('\t') for l in f if l[:1] in 'rwq' and '\t' in l]
    aica = sorted({(r[1], r[4]) for r in rows if r[4].startswith('aica')})
    check('A2: sonda gravou acessos', bool(rows))
    check('A2: da AICA so o relogio (RTC, HH15)', all(reg == 'aica_rtc' for _, reg in aica), str(aica))
    g2dma = sorted({(r[1], r[2]) for r in rows if r[0] == 'w' and 0x005F7800 <= (int(r[2], 16) & 0x1FFFFFFF) <= 0x005F781C
                    and 0x8C120000 <= int(r[1], 16) < 0x8C126000})
    check('A2: o Audio64 nunca programa o DMA do G2', not g2dma, str(g2dma))
    # A3
    sha = lambda p: hashlib.sha256(open(p, 'rb').read()).hexdigest() if os.path.isfile(p) else ''
    check('A3: duas execucoes com o mesmo audio', b.returncode == 0 and sha(wav_a) and sha(wav_a) == sha(wav_b),
          '%s x %s' % (sha(wav_a)[:16], sha(wav_b)[:16]))
    # A4
    pcm, n = wav_pcm(wav_a)
    seg = lambda t0, t1: pcm[2 * int(t0 * 44100):2 * int(t1 * 44100)]
    t = seg(24, 40)
    tl, tr = rms(t[0::2]), rms(t[1::2])
    check('A4: musica do titulo nos dois lados, no nivel da musica', 500 <= tl <= 8000 and 500 <= tr <= 8000 and
          max(tl, tr) / max(min(tl, tr), 1) < 2, 'RMS L %.0f R %.0f' % (tl, tr))
    g = seg(72, n / 44100)
    check('A4: gameplay com som', rms(g) > 300, 'RMS %.0f' % rms(g))
    sat = sum(1 for v in pcm if v in (32767, -32768))
    check('A4: saturacao rara (< 0,1 %)', sat < 0.001 * len(pcm), '%d de %d' % (sat, len(pcm)))
    # A5
    ora = os.environ.get('HH14_ORACULO', '')
    if os.environ.get('HH_EXIGIR_DADOS') == '1':
        check('A5: captura do oraculo disponivel (HH14_ORACULO)', os.path.isfile(os.path.join(ora, 'sombra_titulo.txt')), ora)
    if ora and os.path.isfile(os.path.join(ora, 'sombra_titulo.txt')):
        words = []
        for l in open(os.path.join(ora, 'sombra_titulo.txt'), encoding='utf-8'):
            if re.match(r'^[0-9A-F]{8}:', l):
                words += [int(w, 16) for w in l.split(':', 1)[1].split()]
        def decode(w):  # sombra do driver (0x117C + porta * 0x48): registradores do canal da AICA
            oct4 = (w[6] >> 11) & 15
            return {'SA': ((w[0] & 0x7F) << 16) | (w[1] & 0xFFFF), 'PCMS': (w[0] >> 7) & 3, 'LSA': w[2] & 0xFFFF,
                    'LEA': w[3] & 0xFFFF, 'OCT': oct4 - 16 if oct4 & 8 else oct4, 'FNS': w[6] & 0x3FF,
                    'TL': (w[10] >> 8) & 0xFF, 'DISDL': (w[9] >> 8) & 15, 'DIPAN': w[9] & 0x1F, 'ISEL': w[8] & 15,
                    'IMXL': (w[8] >> 4) & 15}
        oracle = {14: decode(words[0:18]), 15: decode(words[18:36])}
        native = {}
        for m in re.finditer(r'key-on \(amostra \d+\) porta (1[45]): SA (\w+) PCMS (\d+) laco \d LSA (\d+) LEA (\d+) OCT (-?\d+) '
                             r'FNS (\d+) TL (\d+) DISDL (\d+) DIPAN (\w+) ISEL (\d+) IMXL (\d+)', a.stderr):
            port = int(m.group(1))
            if port in native:
                continue
            native[port] = {'SA': int(m.group(2), 16), 'PCMS': int(m.group(3)), 'LSA': int(m.group(4)),
                            'LEA': int(m.group(5)), 'OCT': int(m.group(6)), 'FNS': int(m.group(7)), 'TL': int(m.group(8)),
                            'DISDL': int(m.group(9)), 'DIPAN': int(m.group(10), 16), 'ISEL': int(m.group(11)),
                            'IMXL': int(m.group(12))}
        for port in (14, 15):
            diff = {k: (oracle[port][k], native.get(port, {}).get(k)) for k in oracle[port]
                    if native.get(port, {}).get(k) != oracle[port][k]}
            check('A5: sombra da porta %d igual a do driver real (Flycast)' % port, port in native and not diff, str(diff))

print('FALHOU (%d)' % fails if fails else 'OK')
sys.exit(1 if fails else 0)
