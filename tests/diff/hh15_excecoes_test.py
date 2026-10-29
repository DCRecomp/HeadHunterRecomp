#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
"""HH15.1: testes do comparador de excecoes de SPC (tools/dev/hh15_referencias.py), sem o jogo.

Cada negativo tem de reprovar: posicao deslocada, INTEVT trocado, outro tipo de evento, SPC fora da lista, SPC novo
sem justificativa, excecao listada que nao aparece, excecao de outro caso.
uso: hh15_excecoes_test.py <raiz do projeto>
"""
import importlib.util
import os
import sys

ROOT = os.path.abspath(sys.argv[1] if len(sys.argv) > 1 else os.path.join(os.path.dirname(__file__), '..', '..'))
spec = importlib.util.spec_from_file_location('hh15_ref', os.path.join(ROOT, 'tools', 'dev', 'hh15_referencias.py'))
mod = importlib.util.module_from_spec(spec)
spec.loader.exec_module(mod)
check = mod.order_with_exceptions

REF = 'V\t00000001\nI\t00000320\t8C000100\nV\t00000002\nI\t00000360\t8C09F91E\nT\t00000000\t00000001\n'
NEW = 'V\t00000001\nI\t00000320\t8C000100\nV\t00000002\nI\t00000360\t8C121964\nT\t00000000\t00000001\n'
EXC = {'retornos': [{'spc': '8C121964'}],
       'excecao': [{'caso': 'c', 'vblank': 2, 'intevt': '360', 'spc_ref': '8C09F91E', 'spc_novo': '8C121964'}]}
fails = 0


def expect(name, bad, want_ok):
    global fails
    ok = (not bad) == want_ok
    fails += not ok
    print('[%s] %s%s' % (' ok ' if ok else 'FAIL', name, '' if want_ok else ' (reprovado: %s)' % (bad[0] if bad else '-')))


expect('excecao listada e justificada passa', check('c', REF, NEW, EXC), True)
expect('trace igual sem uso da excecao reprova (excecao velha)', check('c', REF, REF, EXC), False)
expect('outro caso nao herda a excecao', check('d', REF, NEW, EXC), False)
expect('SPC novo diferente do listado', check('c', REF, NEW.replace('8C121964', '8C121968'), EXC), False)
expect('INTEVT trocado', check('c', REF, NEW.replace('I\t00000360\t8C121964', 'I\t00000320\t8C121964'), EXC), False)
expect('outro tipo de evento', check('c', REF, NEW.replace('T\t00000000\t00000001', 'T\t00000000\t00000002'), EXC), False)
expect('posicao deslocada (linha a mais)', check('c', REF, NEW + 'V\t00000003\n', EXC), False)
sem_ret = dict(EXC, retornos=[])
expect('SPC novo sem justificativa em [[retornos]]', check('c', REF, NEW, sem_ret), False)
outro_vb = {'retornos': EXC['retornos'], 'excecao': [dict(EXC['excecao'][0], vblank=1)]}
expect('VBlank diferente do listado', check('c', REF, NEW, outro_vb), False)
DUP_REF = REF + 'I\t00000360\t8C09F91E\n'
DUP_NEW = NEW + 'I\t00000360\t8C121964\n'
expect('duas diferencas iguais com uma so excecao reprova', check('c', DUP_REF, DUP_NEW, EXC), False)
expect('duas diferencas iguais com duas excecoes passa',
       check('c', DUP_REF, DUP_NEW, dict(EXC, excecao=EXC['excecao'] * 2)), True)
sys.exit(1 if fails else 0)
