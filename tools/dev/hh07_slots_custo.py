#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
"""HH07.6: custo dos slots (chamada pelo slot em todas as funcoes geradas, HH_MODS=ON) contra o
build normal. Mesmo roteiro, mesmos VBlanks, execucoes alternadas (normal, slots, normal,
slots...). Exige hashes de quadro identicos entre os dois builds e informa o tempo por VBlank.

uso: hh07_slots_custo.py <hh-run normal> <hh-run slots> <roteiro> [--vblanks N] [--rodadas K]
ambiente: HH_DATA_DIR (dados de execucao do disco 1).
Saida: uma linha por execucao e o resumo; rc 0 = hashes iguais, 1 = diferentes, 77 = sem dados.
"""
import os
import statistics
import subprocess
import sys
import time

a = sys.argv[1:]
if len(a) < 3:
    print(__doc__)
    sys.exit(2)
NORMAL, SLOTS, SCRIPT = (os.path.abspath(x) for x in a[:3])
VBLANKS, ROUNDS = 4700, 2
i = 3
while i + 1 < len(a):
    if a[i] == '--vblanks':
        VBLANKS = int(a[i + 1])
    elif a[i] == '--rodadas':
        ROUNDS = int(a[i + 1])
    i += 2
DATA = os.environ.get('HH_DATA_DIR', '')
if not os.path.isfile(os.path.join(DATA, 'disc.manifest')):
    print('SKIP: defina HH_DATA_DIR')
    sys.exit(77)


def run(exe):
    env = dict(os.environ, HH_INPUT_SCRIPT=SCRIPT)
    t0 = time.perf_counter()
    p = subprocess.run([exe, '--data', DATA, '--vblanks', str(VBLANKS), '--rtc', '1700000000', '--every', '50'],
                       capture_output=True, text=True, env=env)
    dt = time.perf_counter() - t0
    hashes = [l.split()[4] for l in p.stdout.splitlines() if l.startswith('quadro ')]
    return p.returncode, dt, hashes


times = {'normal': [], 'slots': []}
ref = None
ok = True
for r in range(ROUNDS):
    for name, exe in (('normal', NORMAL), ('slots', SLOTS)):
        rc, dt, h = run(exe)
        times[name].append(dt)
        same = ref is None or h == ref
        ref = ref or h
        # uma amostra a cada 50 VBlanks: menos que isso = terminou cedo (revisao Codex HH07)
        ok &= rc == 0 and same and len(h) == VBLANKS // 50
        print('%s rodada %d: rc=%d %.2f s (%.3f ms/VBlank) %d hashes %s' %
              (name, r + 1, rc, dt, dt * 1000 / VBLANKS, len(h), 'iguais' if same else 'DIFERENTES'))
for name in times:
    print('%s: mediana %.3f ms/VBlank' % (name, statistics.median(times[name]) * 1000 / VBLANKS))
n, s = statistics.median(times['normal']), statistics.median(times['slots'])
print('diferenca slots/normal: %+.2f %%' % ((s / n - 1) * 100))
print('hashes: %s' % ('iguais nos dois builds' if ok else 'DIFERENTES ou falha'))
sys.exit(0 if ok else 1)
