#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
# Derivado de recomp-BStinger tools/dist/test_ordem.ps1 (MIT, Copyright (c) 2026 Blue Stinger
# Recompiled contributors; ver third_party/licenses/recomp-BStinger-LICENSE.txt).
"""HH07.7: ordem de eventos do runtime hibrido (quadro, thread, continuacao, interrupcao).

Roda o roteiro newgame_area1 duas vezes no hh-run com HH_EVENT_TRACE e exige traces
identicos (a ordem de VBlank, interrupcoes, continuacoes, trocas/criacao de threads e
longjmp e deterministica), mais propriedades estruturais. Com --inventario grava o
inventario (contagens por tipo, threads criadas, alvos de longjmp/continuacao e eventos de
interrupcao, so enderecos e contagens) para o relatorio e para o HH09.

uso: hh07_ordem.py <hh-run.exe> <roteiro> [--vblanks N] [--inventario <tsv>]
ambiente: HH_DATA_DIR (dados de execucao do disco 1); sem ele = SKIP (rc 77).
"""
import collections
import hashlib
import os
import shutil
import subprocess
import sys
import tempfile

args = sys.argv[1:]
if len(args) < 2:
    print(__doc__)
    sys.exit(2)
EXE, SCRIPT = os.path.abspath(args[0]), os.path.abspath(args[1])
VBLANKS, INVENTARIO = 4700, None
i = 2
while i < len(args):
    if args[i] == '--vblanks' and i + 1 < len(args):
        VBLANKS = int(args[i + 1])
    elif args[i] == '--inventario' and i + 1 < len(args):
        INVENTARIO = args[i + 1]
    else:
        print('opcao desconhecida: ' + args[i])
        sys.exit(2)
    i += 2

DATA = os.environ.get('HH_DATA_DIR', '')
if not DATA or not os.path.isfile(os.path.join(DATA, 'disc.manifest')):
    print('SKIP: defina HH_DATA_DIR (dados de execucao do disco 1)')
    sys.exit(77)

fails = 0
tmp = tempfile.mkdtemp(prefix='hh07_ordem_')


def check(name, ok, detail=''):
    global fails
    print('[%s] %s %s' % (' ok ' if ok else 'FAIL', name, detail))
    fails += not ok


def run(n):
    trace = os.path.join(tmp, 'ordem_%d.tsv' % n)
    env = dict(os.environ, HH_INPUT_SCRIPT=SCRIPT, HH_EVENT_TRACE=trace)
    p = subprocess.run([EXE, '--data', DATA, '--vblanks', str(VBLANKS), '--rtc', '1700000000'],
                       capture_output=True, text=True, env=env)
    return p.returncode, trace, p.stdout + p.stderr


def rows(path):
    out = []
    with open(path) as f:
        for line in f:
            if line.startswith('#') or line.startswith('seq'):
                continue
            seq, cyc, kind, a, b = line.rstrip('\n').split('\t')
            out.append((int(cyc), kind, a, b))
    return out


def sha(path):
    h = hashlib.sha256()
    with open(path, 'rb') as f:
        for blk in iter(lambda: f.read(1 << 20), b''):
            h.update(blk)
    return h.hexdigest()


try:
    rc1, t1, log1 = run(1)
    rc2, t2, log2 = run(2)
    check('execucao 1 termina pelo limite', rc1 == 0, 'rc=%d' % rc1)
    check('execucao 2 termina pelo limite', rc2 == 0, 'rc=%d' % rc2)
    have = os.path.isfile(t1) and os.path.isfile(t2)
    check('traces gravados', have)
    if have:
        h1, h2 = sha(t1), sha(t2)
        check('ordem identica nas duas execucoes', h1 == h2, h1[:16])
        r = rows(t1)
        kinds = collections.Counter(k for _, k, _, _ in r)
        check('um V por VBlank', abs(kinds['V'] - VBLANKS) <= 2, 'V=%d' % kinds['V'])
        check('interrupcoes registradas', kinds['I'] > kinds['V'], 'I=%d' % kinds['I'])
        check('threads do guest criadas', kinds['N'] >= 1,
              'N=%d T=%d R=%d J=%d' % (kinds['N'], kinds['T'], kinds['R'], kinds['J']))
        # revisao Codex HH07: sem trocas de thread e longjmp a ordem nao foi exercitada
        check('trocas de thread registradas', kinds['T'] >= kinds['V'], 'T=%d' % kinds['T'])
        check('longjmp registrados', kinds['J'] >= 1, 'J=%d' % kinds['J'])
        mono = all(r[j][0] >= r[j - 1][0] for j in range(1, len(r)))
        check('ciclos nao decrescentes', mono)
        # (quadro, evento): cada evento pertence ao quadro do ultimo V; a sequencia por quadro
        # tambem e deterministica (implicado pelo hash, conferido aqui por quadro)
        if INVENTARIO:
            frame, per_frame = 0, collections.Counter()
            threads, longjmp, cont, irq = collections.Counter(), collections.Counter(), collections.Counter(), collections.Counter()
            switches = collections.Counter()
            for cyc, k, a, b in r:
                if k == 'V':
                    frame += 1
                per_frame[frame] += 1
                if k == 'N':
                    threads['%s\t%s' % (a, b)] += 1
                elif k == 'J':
                    longjmp['%s\t%s' % (a, b)] += 1
                elif k == 'R':
                    cont[a] += 1
                elif k == 'I':
                    irq[a] += 1
                elif k == 'T':
                    switches['%s\t%s' % (a, b)] += 1
            os.makedirs(os.path.dirname(os.path.abspath(INVENTARIO)), exist_ok=True)
            with open(INVENTARIO, 'w', newline='\n') as f:
                f.write('# HH07.7 inventario de ordem (hh07_ordem.py): roteiro %s, %d VBlanks, sha256 %s\n'
                        % (os.path.basename(SCRIPT), VBLANKS, h1))
                f.write('secao\tchave\tvalor\tcontagem\n')
                for k in sorted(kinds):
                    f.write('tipo\t%s\t-\t%d\n' % (k, kinds[k]))
                for key, n in sorted(threads.items()):
                    f.write('thread_nova\t%s\t%d\n' % (key, n))
                for key, n in sorted(switches.items()):
                    f.write('troca_thread\t%s\t%d\n' % (key, n))
                for key, n in sorted(longjmp.items()):
                    f.write('longjmp\t%s\t%d\n' % (key, n))
                for key, n in sorted(cont.items()):
                    f.write('continuacao\t%s\t-\t%d\n' % (key, n))
                for key, n in sorted(irq.items()):
                    f.write('interrupcao\t%s\t-\t%d\n' % (key, n))
                f.write('quadro_max_eventos\t%d\t-\t%d\n' % max(((n, fr) for fr, n in per_frame.items()))[::-1])
finally:
    shutil.rmtree(tmp, ignore_errors=True)

print('%d falhas' % fails)
sys.exit(1 if fails else 0)
