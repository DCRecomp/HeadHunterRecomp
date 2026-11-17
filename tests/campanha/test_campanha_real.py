# SPDX-License-Identifier: MIT
"""HH16.1: o monitor de campanha (tools/dev/campanha.py) com o HeadHunter.exe de verdade, sem janela.

A "sessao gravada" e o roteiro newgame_area1_nativo jogado pelo exe sem janela (tests/campanha/gravador_headless.py; o
--gravar real exige a janela e fica para a sessao piloto da HH16.P). O resto e o caminho real: programa fixado, selos,
saves, --log, --estado, HH_FILE_LOG, reproducao, cadeia, verificacao paralela, reproduzir-em e copia de seguranca.

Positivos: duas sessoes REPRODUZIDA (estado conferido, cobertura com as areas e filmes do NEW GAME), cadeia integra com
--paralelo 2, reproduzir-em na mesma instalacao, copia de seguranca reproduzivel. Negativo: o analogico da leitura 4500
trocado na reproducao: DIVERGENTE, com a primeira leitura diferente logo depois de 4500.

uso: test_campanha_real.py <HeadHunter.exe> <raiz do projeto>
ambiente: HH_DATA_UNICO = instalacao com os dois discos (game-data/runtime/unico); sem ela = SKIP (rc 77).
"""
import json
import os
import shutil
import subprocess
import sys
import tempfile

EXE, ROOT = (os.path.abspath(a) for a in sys.argv[1:3])
DATA = os.environ.get('HH_DATA_UNICO', '')
if not os.path.isfile(os.path.join(DATA, 'arquivos.manifest')):
    print('SKIP: defina HH_DATA_UNICO (instalacao com os dois discos)')
    sys.exit(77)
MONITOR = os.path.join(ROOT, 'tools', 'dev', 'campanha.py')
AQUI = os.path.dirname(os.path.abspath(__file__))
VBLANKS = 4700  # passa da leitura 4500 do negativo e chega a primeira area

fails = 0
tmp = tempfile.mkdtemp(prefix='hh16_camp_real_')


def check(nome, cond, info=''):
    global fails
    print('[%s] %s%s' % (' ok ' if cond else 'FAIL', nome, (' (%s)' % info) if info else ''))
    fails += not cond


# instalacao de teste: o exe e as DLLs do build; data = juncao para os dados privados (nada copiado)
inst = os.path.join(tmp, 'instalacao')
os.makedirs(inst)
for n in os.listdir(os.path.dirname(EXE)):
    if n.lower().endswith(('.exe', '.dll')) and (n.lower() == os.path.basename(EXE).lower() or n.lower().endswith('.dll')):
        shutil.copy2(os.path.join(os.path.dirname(EXE), n), os.path.join(inst, n))
if os.path.basename(EXE) != 'HeadHunter.exe':
    os.replace(os.path.join(inst, os.path.basename(EXE)), os.path.join(inst, 'HeadHunter.exe'))
subprocess.run(['cmd', '/c', 'mklink', '/J', os.path.join(inst, 'data'), os.path.abspath(DATA)], check=True,
               capture_output=True)
camps = os.path.join(tmp, 'campanhas')
ENV = {k: v for k, v in os.environ.items() if not k.startswith('HH_')}
ENV.update(HH_CAMPANHAS=camps, HH_CAMPANHA_PROGRAMA=os.path.join(AQUI, 'gravador_headless.py'),
           CAMPANHA_TESTE_ROTEIRO=os.path.join(ROOT, 'tools', 'dist', 'roteiros', 'newgame_area1_nativo.txt'),
           CAMPANHA_TESTE_VBLANKS=str(VBLANKS))


def monitor(*args, **extra):
    p = subprocess.run([sys.executable, '-I', MONITOR] + list(args), capture_output=True, text=True,
                       env=dict(ENV, **extra))
    return p.returncode, p.stdout + p.stderr


rc, out = monitor('nova', '--instalacao', inst)
check('campanha criada', rc == 0, out.strip()[-200:])
cid = sorted(d for d in os.listdir(camps) if not d.startswith('_'))[0]
for n in (1, 2):
    rc, out = monitor('jogar')
    check('sessao %d gravada' % n, rc == 0 and 'sessao %d gravada' % n in out, out.strip()[-300:])

rc, out = monitor('verificar', '--paralelo', '2')
check('verificar: as duas sessoes reproduzidas', rc == 0 and 'sessao 1: REPRODUZIDA' in out and
      'sessao 2: REPRODUZIDA' in out and 'INCOMPLETA' in out, out.strip()[-400:])
rel = json.load(open(os.path.join(camps, cid, 'relatorio.json'), encoding='utf-8'))
s1 = rel['sessoes'][0]
check('estado conferido em toda a sessao', s1['com_estado'] and s1['estados_conferidos'] > 70,
      '%d estados' % s1['estados_conferidos'])
check('quadros iguais e saves iguais', s1['primeiro_quadro_diferente'] is None and s1['saves_iguais'])
check('cobertura: areas e filmes do NEW GAME', len(rel['cobertura']['areas']) >= 2 and
      any('SCENE' in f for f in rel['cobertura']['filmes']), json.dumps(rel['cobertura'])[:300])
check('sem creditos (o roteiro nao chega la)', not s1['creditos'])

rc, out = monitor('reproduzir-em', cid, '--instalacao', inst, '--rotulo', 'mesma')
check('reproduzir-em na mesma instalacao', rc == 0 and 'REPRODUZIDA EM OUTRO PROGRAMA' in out, out.strip()[-300:])

rc, out = monitor('reproduzir-em', cid, '--instalacao', inst, '--rotulo', 'trocada', CAMPANHA_TESTE_TROCA='1')
r = json.load(open(os.path.join(camps, cid, 'outros', 'trocada', 'sessao_001', 'resultado.json'), encoding='utf-8'))
check('negativo: entrada trocada acusada', rc == 1 and 'DIVERGENTE' in out and r['veredito'] == 'REPROVADA', out.strip()[-200:])
check('negativo: a primeira leitura diferente logo depois de 4500',
      r['primeira_leitura_diferente'] is not None and 4500 <= r['primeira_leitura_diferente'] <= 4620,
      'leitura %s' % r['primeira_leitura_diferente'])

copia = os.path.join(camps, '_copias')
rc, out = subprocess.run([sys.executable, '-I', MONITOR, 'verificar', '--sessao', '1'], capture_output=True, text=True,
                         env=dict(ENV, HH_CAMPANHAS=copia)).returncode, ''
check('a copia de seguranca reproduz a sessao 1', rc == 0)

print('%d falhas' % fails)
sys.exit(1 if fails else 0)
