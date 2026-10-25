#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
"""HH05: casos positivos e negativos dos comparadores dos portoes, com dados sinteticos
(nenhum byte do jogo). Roda sempre no CTest.
uso: test_gates.py <hh05_boot_check.py> <hh05_fb.py> <hh_boot_accepted.tsv>"""
import os
import random
import subprocess
import sys
import tempfile

CHECK, FB, ACCEPTED = sys.argv[1:4]
fails = 0


def rc(cmd):
    return subprocess.run([sys.executable, '-I'] + cmd, capture_output=True, text=True).returncode


def expect(name, got, want):
    global fails
    ok = (got == want) if want is not None else got != 0
    print('[%s] %s (rc=%d)' % (' ok ' if ok else 'FAIL', name, got))
    fails += not ok


def trace(n, seed):
    rnd = random.Random(seed)
    rows = []
    for i in range(n):
        f = ['8C1%05X' % rnd.randrange(0x10000, 0x8FFFF, 2)] + ['%08X' % rnd.getrandbits(32) for _ in range(20)]
        rows.append(f)
    rows[239][0] = '8C168940'
    rows[100][0] = '8C1793E0'
    return rows


def write(path, rows):
    with open(path, 'w') as f:
        for r in rows:
            f.write('\t'.join(r) + '\n')


def base_pair():
    a = trace(2800, 1)
    b = [list(r) for r in trace(3000, 1)]
    a[239][3] = '00001A5B'            # r2 aceito (registrador morto)
    b[239][3] = '00001AA2'
    a[558][0], b[558][0] = '8C00FA00', '8C00F400'
    a[50][17], b[50][17] = 'AC15EC10', '8C15EC10'    # PR espelho P2 x P1
    a[100][3], b[100][3] = '00001798', '0000073F'    # r2 no helper de tempo
    return a, b


with tempfile.TemporaryDirectory() as tmp:
    fa, fb_ = os.path.join(tmp, 'a.tsv'), os.path.join(tmp, 'b.tsv')

    def run_trace(a, b):
        write(fa, a)
        write(fb_, b)
        return rc([CHECK, fa, fb_, ACCEPTED, '--len-b', '3000'])

    a, b = base_pair()
    expect('trace: positivo', run_trace(a, b), 0)
    a, b = base_pair(); b[10][5] = '%08X' % (int(b[10][5], 16) ^ 1)
    expect('trace: registrador injetado', run_trace(a, b), 1)
    a, b = base_pair(); b[300][0] = '8C123456'
    expect('trace: funcao injetada', run_trace(a, b), 1)
    a, b = base_pair()
    expect('trace: recompilado truncado', run_trace(a, b[:2999]), 1)
    a, b = base_pair()
    expect('trace: flycast nao alcanca a divergencia', run_trace(a[:500], b), 1)
    a, b = base_pair(); b[400] = b[400][:20]
    expect('trace: coluna faltando', run_trace(a, b), None)
    a, b = base_pair(); b[239][3] = a[239][3]
    expect('trace: aceita que nao ocorre', run_trace(a, b), 1)
    a, b = base_pair(); b[558][0] = '8C00F500'
    expect('trace: divergencia diferente da declarada', run_trace(a, b), 1)
    a, b = base_pair(); a[50][17] = 'AC15EC14'
    expect('trace: PR em outro endereco fisico', run_trace(a, b), 1)

    # quadros: 100x100, 0555 expandido
    def ppm(path, px, w=100, h=100):
        with open(path, 'wb') as f:
            f.write(b'P6\n%d %d\n255\n' % (w, h) + bytes(px))

    rnd = random.Random(7)
    ref = [rnd.choice([0, 8, 16, 132, 140, 255]) for _ in range(100 * 100 * 3)]
    pa, pb = os.path.join(tmp, 'a.ppm'), os.path.join(tmp, 'b.ppm')
    ppm(pa, ref)
    rules = ['--deltas', '8,9', '--min-exact', '94']

    def run_fb(px, **kw):
        ppm(pb, px, **kw)
        return rc([FB, 'compare', pa, pb] + rules)

    def mutate(count, delta):
        px = list(ref)
        for p in range(count):
            i = p * 3
            v = px[i]
            px[i] = v + delta if v + delta <= 255 else (v - delta if v - delta >= 0 else (255 if v < 128 else 0))
        return px

    expect('quadro: identico', run_fb(ref), 0)
    expect('quadro: 5% com 1 passo (8)', run_fb(mutate(500, 8)), 0)
    expect('quadro: 7% com 1 passo (acima do limite de pixels)', run_fb(mutate(700, 8)), 1)
    expect('quadro: delta 5 fora dos aceitos', run_fb(mutate(10, 5)), 1)
    expect('quadro: delta 255', run_fb(mutate(1, 255)), 1)
    expect('quadro: dimensoes diferentes', run_fb(ref[:90 * 100 * 3], w=90), 1)
    with open(pb, 'wb') as f:
        f.write(b'P6\n100 100\n255\n' + bytes(ref[:-3]))
    expect('quadro: PPM truncado', rc([FB, 'compare', pa, pb] + rules), 2)

print('%d falha(s)' % fails)
sys.exit(1 if fails else 0)
