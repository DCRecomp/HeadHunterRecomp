#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
"""HH09: portoes do tempo nativo no roteiro newgame_area1 (4 700 VBlanks).

Estavel (--tempo nativo) x legado (--tempo legado):
  A1  trace de ordem de eventos (HH_EVENT_TRACE) identico byte a byte;
  A3  94 hashes de quadro iguais (e iguais a referencia do HH06, se HH_AREA1_HASHES);
  A5  relogios consultaveis por VBlank (HH_CLOCK_LOG: TMU, SPG, MDST, RTC...) identicos;
  negativos (HH_TEMPO_MUTACAO, terminam normalmente): atrasar_render (o fim do render chega um
  quantum depois) e o A1 acusa; relogio_degraus (o relogio anda em degraus de 1/8 de campo) e o
  A5 acusa. (perder_vblank_out nao serve: o jogo nao usa a interrupcao de VBlank-Out.)
Experimental (--tempo experimental, o tempo so anda nas esperas declaradas):
  B7  sem falta de progresso nem parada (todas as esperas do roteiro declaradas);
  B8  duas execucoes com os mesmos 94 hashes; o quadro final nao e preto.

uso: hh09_tempo.py <hh-run.exe> <raiz do projeto>
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

tmp = tempfile.mkdtemp(prefix='hh09_tempo_')
fails = 0


def check(name, cond, info=''):
    global fails
    print('[%s] %s%s' % (' ok ' if cond else 'FAIL', name, (' (%s)' % info) if info else ''))
    fails += not cond


def run(tag, mode, extra=None):
    ev, ck = os.path.join(tmp, tag + '_ev.tsv'), os.path.join(tmp, tag + '_ck.tsv')
    tt = os.path.join(tmp, tag + '_tt.tsv')  # HH_TIME_TRACE (A2; so o provedor nativo grava)
    env = dict(os.environ, HH_INPUT_SCRIPT=SCRIPT, HH_EVENT_TRACE=ev, HH_CLOCK_LOG=ck, HH_TIME_TRACE=tt,
               **(extra or {}))
    if not extra:
        env.pop('HH_TEMPO_MUTACAO', None)
    p = subprocess.run([RUN, '--data', DATA, '--vblanks', str(VBLANKS), '--rtc', '1700000000', '--every', '50',
                        '--tempo', mode], capture_output=True, text=True, env=env)
    lines = [l for l in p.stdout.splitlines() if l.startswith('quadro ')]
    rd = lambda f: open(f, 'rb').read() if os.path.isfile(f) else b''
    return {'rc': p.returncode, 'hashes': [l.split()[4] for l in lines], 'black': [l.endswith('(preto)') for l in lines],
            'ev': rd(ev), 'ck': rd(ck), 'tt': rd(tt).decode('utf-8', 'replace'), 'err': p.stdout + p.stderr}


# A2: limite de atraso da entrega = 2 quanta de poll (kServiceQuantum = 2000 ciclos de
# compatibilidade): a entrega so acontece no poll seguinte ao devido (o BS mediu 2 120 contra 4 000).
A2_LIMIT = 4000


def a2(trace):
    """(eventos, VBlanks fora da linha do SPG, entregas antes do devido, maior atraso)."""
    rows = [l.split('\t') for l in trace.splitlines() if l and not l.startswith('#') and not l.startswith('seq')]
    off = sum(1 for r in rows if r[1] in ('VBlankIn', 'VBlankOut') and r[4] == '0')
    # RenderStart: devido = fim de render agendado, entrega = instante do STARTRENDER (cabecalho do
    # trace); nao e uma entrega de evento
    early = sum(1 for r in rows if r[1] != 'RenderStart' and int(r[3]) < int(r[2]))
    late = max((int(r[3]) - int(r[2]) for r in rows if r[1] != 'RenderStart'), default=0)
    return len(rows), off, early, late


try:
    with ThreadPoolExecutor(6) as ex:
        jobs = {
            'legado': ex.submit(run, 'legado', 'legado'),
            'nativo': ex.submit(run, 'nativo', 'nativo'),
            'mutacao': ex.submit(run, 'mutacao', 'nativo', {'HH_TEMPO_MUTACAO': 'atrasar_render'}),
            'degraus': ex.submit(run, 'degraus', 'nativo', {'HH_TEMPO_MUTACAO': 'relogio_degraus'}),
            'exp1': ex.submit(run, 'exp1', 'experimental'),
            'exp2': ex.submit(run, 'exp2', 'experimental'),
        }
        r = {k: v.result() for k, v in jobs.items()}
    L, N, M, D, E1, E2 = r['legado'], r['nativo'], r['mutacao'], r['degraus'], r['exp1'], r['exp2']
    check('legado e nativo terminam pelo limite', L['rc'] == 0 and N['rc'] == 0, 'rc %d/%d' % (L['rc'], N['rc']))
    check('A1: ordem de eventos identica (legado x nativo)', L['ev'] and L['ev'] == N['ev'], '%d bytes' % len(L['ev']))
    check('A3: 94 hashes iguais (legado x nativo)', len(N['hashes']) == VBLANKS // 50 and L['hashes'] == N['hashes'])
    if os.path.isfile(REF):
        with open(REF) as f:
            ref = [l.split()[1] for l in f if l.strip()]
        check('A3: nativo igual a referencia do HH06', N['hashes'] == ref)
    check('A5: relogios por VBlank identicos', L['ck'] and L['ck'] == N['ck'], '%d bytes' % len(L['ck']))
    n_ev, off, early, late = a2(N['tt'])
    check('A2: VBlank devido na linha do SPG, nada antes do devido, atraso <= %d' % A2_LIMIT,
          n_ev > 10000 and off == 0 and early == 0 and late <= A2_LIMIT,
          '%d eventos, %d fora da linha, %d antes, maior atraso %d' % (n_ev, off, early, late))
    check('negativo: atrasar_render termina e o A1 acusa', M['rc'] == 0 and M['ev'] and M['ev'] != L['ev'],
          'rc %d' % M['rc'])
    check('negativo: relogio_degraus termina e o A5 acusa', D['rc'] == 0 and D['ck'] and D['ck'] != L['ck'],
          'rc %d' % D['rc'])
    check('B7: experimental sem falta de progresso', E1['rc'] == 0 and 'PARADA' not in E1['err'], 'rc %d' % E1['rc'])
    check('B8: experimental deterministico (94 hashes iguais em duas execucoes)',
          len(E1['hashes']) == VBLANKS // 50 and E1['hashes'] == E2['hashes'])
    check('experimental: quadro final nao preto', bool(E1['black']) and not E1['black'][-1])
finally:
    shutil.rmtree(tmp, ignore_errors=True)  # traces com dados de execucao do jogo

print('%d falhas' % fails)
sys.exit(1 if fails else 0)
