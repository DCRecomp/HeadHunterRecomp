#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
"""HH15.1: testes dos comparadores do portao hh15_tempo.py, sem o jogo (positivos e negativos).
uso: hh15_tempo_test.py <raiz do projeto>
"""
import importlib.util
import os
import sys

ROOT = os.path.abspath(sys.argv[1] if len(sys.argv) > 1 else os.path.join(os.path.dirname(__file__), '..', '..'))
spec = importlib.util.spec_from_file_location('hh15_tempo', os.path.join(ROOT, 'tests', 'diff', 'hh15_tempo.py'))
g = importlib.util.module_from_spec(spec)
spec.loader.exec_module(g)
fails = 0


def expect(name, cond):
    global fails
    fails += not cond
    print('[%s] %s' % (' ok ' if cond else 'FAIL', name))


def seq(pairs):
    return [(v, h, 0) for v, h in pairs]


TR = [{'nome': 'boot', 'estavel_de': 10, 'estavel_ate': 12, 'estavel_trocas': 2, 'logico_de': 3, 'logico_ate': 3,
       'logico_trocas': 1},
      {'nome': 'carga', 'estavel_de': 50, 'estavel_ate': 54, 'estavel_trocas': 2, 'logico_trocas': 0}]
EST = seq([(1, 'p'), (10, 'b1'), (12, 'b2'), (20, 'A'), (30, 'B'), (50, 'L1'), (54, 'L2'), (60, 'C'), (70, 'M1'), (72, 'M2')])
LOG = seq([(1, 'p'), (3, 'bx'), (8, 'A'), (15, 'B'), (25, 'C'), (35, 'M1'), (37, 'M2'), (39, 'M3')])

MV_E, MV_L = (68, 72), (33, 39)  # (inicio, pulo) do ultimo filme: o logico tocou mais


def cf(est, log, ce, cl, tr, me=MV_E, ml=MV_L):
    return g.compare_frames(est, log, ce, cl, tr, me, ml)


bad, eq, ap = cf(EST, LOG, 72, 39, TR)
expect('lista fechada e sobra do filme no fim passam', not bad and eq == 6 and len(ap) == 3)
bad, _, _ = cf(EST, seq([(1, 'p'), (4, 'bx'), (8, 'A'), (15, 'B'), (25, 'C'), (35, 'M1'), (37, 'M2'), (39, 'M3')]), 72, 39, TR)
expect('trecho com VBlanks do logico diferentes reprova', any('fora da lista' in x for x in bad))
bad, _, _ = cf(EST, LOG, 72, 39, TR, MV_E, (36, 39))
expect('sobra fora do intervalo do filme reprova', bool(bad))
bad, _, _ = cf(EST, LOG, 72, 39, TR, (60, 72), MV_L)
expect('sobra do lado cujo filme durou menos reprova', bool(bad))
LONG = LOG + seq([(39 + k, 'T%d' % k) for k in range(1, 13)])  # 13 trocas em 18 VBlanks de filme: limite 9
bad, _, _ = cf(EST, LONG, 72, 51, TR, MV_E, (33, 51))
expect('sobra maior que o limite reprova', bool(bad))
bad, _, _ = cf(EST, seq([(1, 'p'), (3, 'bx'), (8, 'A'), (15, 'X'), (25, 'C'), (35, 'M1')]), 72, 35, TR)
expect('troca diferente no meio reprova', any('fora da lista' in x for x in bad))
bad, _, _ = cf(EST, LOG, 72, 39, TR[:1])
expect('trecho nao listado (carga) reprova', any('fora da lista' in x for x in bad))
bad, _, _ = cf(EST, LOG, 72, 39, TR + [{'nome': 'extra', 'estavel_de': 90, 'estavel_ate': 91,
                                                     'estavel_trocas': 1, 'logico_trocas': 0}])
expect('trecho listado que nao aparece reprova', any('nao apareceu' in x for x in bad))
bad, _, _ = cf(EST, LOG, 72, 39, [dict(TR[0]), dict(TR[1], estavel_trocas=3)])
expect('contagem diferente da listada reprova', any('fora da lista' in x for x in bad))
bad, _, _ = cf(EST, seq([(1, 'p'), (3, 'bx'), (8, 'A'), (15, 'B'), (25, 'C'), (35, 'Z')]), 72, 35, TR)
expect('troca diferente no fim (nao so sobra) reprova', bool(bad))

expect('prefixo identico passa', g.compare_prefix(['a', 'b'], ['a', 'b', 'c']) == '')
expect('bloco diferente reprova', g.compare_prefix(['a', 'x'], ['a', 'b', 'c']) != '')
expect('sem leituras reprova', g.compare_prefix([], ['a']) != '')
expect('logico com leituras a menos reprova', g.compare_prefix(['a', 'b', 'c'], ['a', 'b']) != '')

A = [(1, 'c1'), (5, 'c2'), (9, 'c3'), (12, 'c4')]
expect('audio igual ate depois dos cortes passa', g.compare_audio(A, A + [(13, 'c5')], 9, 9)[0] == '')
expect('audio diferente antes do corte reprova', g.compare_audio(A, [(1, 'c1'), (5, 'cX')], 9, 9)[0] != '')
expect('audio igual so ate o VBlank do corte reprova', g.compare_audio([(9, 'x')], [(9, 'x')], 9, 9)[0] != '')
expect('audio igual so ate antes do corte reprova', g.compare_audio(A[:2] + [(9, 'e')], A[:2] + [(8, 'f')], 9, 8)[0] != '')

expect('corte pelo ultimo pulo', g.last_skip('[video] A: parado pelo jogo (skip) no VBlank 10\n'
                                             '[video] B: parado pelo jogo (skip) no VBlank 40\n') == 40)
sys.exit(1 if fails else 0)
