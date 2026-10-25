#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
"""HH08.4: portoes P1/P2 do provedor nativo de arquivos no roteiro newgame_area1.

P1: a sequencia global de blocos de dado de arquivo (arquivo logico, deslocamento, bytes,
    SHA-256) e a mesma no legado (GD-ROM HLE) e no nativo (adaptador gdFs).
P2: no modo nativo, 0 setores de dado de arquivo passam pelo GD-ROM HLE.
Tambem: os hashes de quadro do nativo sao os do legado (HH_AREA1_HASHES, se definido) e o
negativo HH_ARQUIVOS_MUTACAO=corromper_bloco faz o P1 falhar, no bloco certo.

uso: hh08_arquivos.py <hh-run.exe> <raiz do projeto>
ambiente: HH_DATA_DIR (dados privados; sem ele = SKIP, rc 77), HH_AREA1_HASHES (opcional).
"""
import os
import shutil
import subprocess
import sys
import tempfile
from concurrent.futures import ThreadPoolExecutor

RUN, ROOT = (os.path.abspath(a) for a in sys.argv[1:3])
DATA = os.environ.get('HH_DATA_DIR', '')
REF = os.environ.get('HH_AREA1_HASHES', '')
SCRIPT = os.path.join(ROOT, 'tools', 'dist', 'roteiros', 'newgame_area1.txt')
VBLANKS = 4700

if not os.path.isfile(os.path.join(DATA, 'disc.manifest')):
    print('SKIP: defina HH_DATA_DIR')
    sys.exit(77)

tmp = tempfile.mkdtemp(prefix='hh08_arq_')
fails = 0


def check(name, cond, info=''):
    global fails
    print('[%s] %s%s' % (' ok ' if cond else 'FAIL', name, (' (%s)' % info) if info else ''))
    fails += not cond


def run(tag, provider, extra_env=None):
    log = os.path.join(tmp, tag + '.tsv')
    env = dict(os.environ, HH_INPUT_SCRIPT=SCRIPT, HH_FILE_LOG=log, **(extra_env or {}))
    if not extra_env:
        env.pop('HH_ARQUIVOS_MUTACAO', None)
    p = subprocess.run([RUN, '--data', DATA, '--vblanks', str(VBLANKS), '--rtc', '1700000000', '--every', '50',
                        '--arquivos', provider], capture_output=True, text=True, env=env)
    hashes = [l.split()[4] for l in p.stdout.splitlines() if l.startswith('quadro ')]
    rows = []
    if os.path.isfile(log):
        with open(log, encoding='utf-8') as f:
            for line in list(f)[1:]:
                c = line.rstrip('\n').split('\t')
                rows.append((c[2], c[3], tuple(c[4:8])))  # origem, tipo, (arquivo, desloc, bytes, sha)
    return p.returncode, hashes, rows, p.stderr


def data_seq(rows):
    return [r[2] for r in rows if r[1] == 'dado']


try:
    with ThreadPoolExecutor(3) as ex:
        fl = ex.submit(run, 'legado', 'legado')
        fn = ex.submit(run, 'nativo', 'nativo')
        fm = ex.submit(run, 'mutacao', 'nativo', {'HH_ARQUIVOS_MUTACAO': 'corromper_bloco'})
        (rl, hl, ll, el), (rn, hn, ln, en), (rm, hm, lm, em) = fl.result(), fn.result(), fm.result()
    check('legado termina pelo limite', rl == 0, 'rc=%d' % rl)
    check('nativo termina pelo limite', rn == 0, 'rc=%d' % rn)
    check('94 amostras de quadro em cada modo', len(hl) == len(hn) == VBLANKS // 50, '%d/%d' % (len(hl), len(hn)))
    sl, sn = data_seq(ll), data_seq(ln)
    check('legado le os dados pelo GD-ROM HLE', len(sl) > 1000 and all(r[0] == 'gdrom' for r in ll if r[1] == 'dado'),
          '%d blocos' % len(sl))
    first = next((i for i, (a, b) in enumerate(zip(sl, sn)) if a != b), None)
    check('P1: mesma sequencia de blocos (arquivo, deslocamento, bytes, SHA-256)', sl == sn,
          '%d x %d blocos; primeira diferenca %s' % (len(sl), len(sn), first))
    gd = sum(1 for r in ln if r[0] == 'gdrom' and r[1] == 'dado')
    check('P2: 0 setores de dado de arquivo pelo GD-ROM HLE no nativo', gd == 0, '%d' % gd)
    check('nativo registrou as leituras pelo adaptador', all(r[0] == 'nativo' for r in ln if r[1] == 'dado'))
    check('hashes de quadro do nativo iguais aos do legado', hl == hn)
    if os.path.isfile(REF):
        with open(REF) as f:  # "<vblank>: <hash>" (tools/dev/regressao.py)
            ref = [l.split()[1] for l in f if l.strip()]
        check('hashes do nativo iguais a referencia do HH06', len(ref) == len(hn) and hn == ref)
    # negativo: a mutacao troca um byte da 20a leitura nativa; o P1 tem de acusar exatamente ali
    # a execucao mutada termina normalmente, com a mesma quantidade de blocos, e difere em
    # exatamente um bloco (mesmo arquivo/deslocamento/tamanho, SHA diferente): o motivo e a mutacao
    sm = data_seq(lm)
    diff = [i for i, (a, b) in enumerate(zip(sl, sm)) if a != b]
    check('negativo: execucao mutada termina pelo limite', rm == 0 and len(hm) == VBLANKS // 50, 'rc=%d' % rm)
    check('negativo: bloco corrompido acusado pelo P1 (um so bloco)',
          len(sm) == len(sl) and len(diff) == 1 and sl[diff[0]][:3] == sm[diff[0]][:3],
          'diferencas %s' % diff[:5])
finally:
    shutil.rmtree(tmp, ignore_errors=True)  # os logs tem nomes de arquivo e hashes do jogo

print('%d falhas' % fails)
sys.exit(1 if fails else 0)
