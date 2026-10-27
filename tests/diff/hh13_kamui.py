#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
"""HH13.6: portoes do provedor nativo da camada Kamui (docs/planos/HH13-PLANO.md, "HH13.6") no roteiro newgame_area1.

K1  sombra (HH_KAMUI=sombra): os cinco funis cobrem tudo o que o render le; estado nativo igual ao do runtime em todos
    os STARTRENDER (registradores, parametros, memoria): 0 divergencias, conferidos > 0;
K2  nativo (HH_KAMUI=nativo, render em software): os 94 hashes iguais aos do caminho legado e a referencia do HH06
    (HH_AREA1_HASHES, quando definida); renders nativos = renders do provedor de tempo;
K4  conferir (HH_RENDER_IR=conferir, 1 render a cada 5) com o provedor nativo: a IR do estado nativo descreve o que o
    rasterizador usou, render a render: 0 divergencias;
K3  sonda de hardware (HH_TA_FONTES) no nativo: nenhum byte no TA nem na VRAM do runtime; do PVR so os registradores de
    tempo do video (SPG, 0x0C8-0x0E0, e SPG_STATUS), que saem no HH15; os bytes da area 4 chegam ao destino nativo.

uso: hh13_kamui.py <hh-run.exe> <raiz do projeto>
ambiente: HH_DATA_DIR (dados privados; sem ele = SKIP, rc 77); HH_AREA1_HASHES (referencia do HH06; obrigatoria com
HH_EXIGIR_DADOS=1, como na regressao).
"""
import os
import re
import subprocess
import sys
import tempfile

RUN, ROOT = (os.path.abspath(a) for a in sys.argv[1:3])
DATA = os.environ.get('HH_DATA_DIR', '')
SCRIPT = os.path.join(ROOT, 'tools', 'dist', 'roteiros', 'newgame_area1.txt')
VBLANKS, RTC = 4700, 1700000000
if not os.path.isfile(os.path.join(DATA, 'disc.manifest')):
    print('SKIP: defina HH_DATA_DIR')
    sys.exit(77)
fails = 0
SPG = {'pvr_escreve_%03X' % o for o in range(0x0C8, 0x0E4, 4)} | {'pvr_le_%03X' % o for o in range(0x0C8, 0x0E4, 4)} | \
    {'pvr_le_10C'}


def check(name, cond, info=''):
    global fails
    print('[%s] %s%s' % (' ok ' if cond else 'FAIL', name, (' (%s)' % info) if info else ''))
    fails += not cond


def run(kamui, extra=None):
    env = dict(os.environ, HH_INPUT_SCRIPT=SCRIPT)
    for k in ('HH_RENDER_IR', 'HH_RENDER_IR_CADA', 'HH_RENDER_IR_BMP', 'HH_KAMUI', 'HH_TA_FONTES'):
        env.pop(k, None)
    env['HH_KAMUI'] = kamui
    env.update(extra or {})
    p = subprocess.run([RUN, '--data', DATA, '--vblanks', str(VBLANKS), '--rtc', str(RTC), '--every', '50',
                        '--render', 'software'], capture_output=True, text=True, env=env)
    k = re.search(r'\[kamui\] [^\n]*', p.stderr)
    t = re.search(r'\[tempo\] [^\n]* render (\d+)', p.stderr)
    return {'rc': p.returncode, 'err': p.stderr, 'kamui': k.group(0) if k else '',
            'tempo_renders': int(t.group(1)) if t else None,
            'hashes': [(int(v), h) for v, h in re.findall(r'quadro (\d+): \S+ hash ([0-9a-f]+)', p.stdout)]}


def num(text, pat):
    m = re.search(pat, text)
    return int(m.group(1)) if m else None


# K1
s = run('sombra')
line = s['kamui']
check('K1: sombra executa (rc 0) e resume', s['rc'] == 0 and line.startswith('[kamui] sombra'), s['err'].strip()[-300:])
vals = {k: num(line, p) for k, p in (('renders', r'renders (\d+)'), ('reg', r'divergentes (\d+)\), lidos'),
                                      ('par_n', r'parametros conferidos (\d+)'), ('par', r'\(divergencias (\d+)\)'),
                                      ('mem_n', r'blocos de memoria conferidos (\d+)'),
                                      ('mem', r'blocos de memoria conferidos \d+ \(divergentes (\d+)\)'))}
check('K1: conferidos (renders, parametros, blocos) > 0',
      all(vals[k] for k in ('renders', 'par_n', 'mem_n')), line)
check('K1: 0 divergencias (registradores, parametros, memoria)',
      vals['reg'] == 0 and vals['par'] == 0 and vals['mem'] == 0, line)

# K2 e K3
legado = run('legado')
with tempfile.TemporaryDirectory() as tmp:
    fontes = os.path.join(tmp, 'fontes.tsv')
    n = run('nativo', {'HH_TA_FONTES': fontes})
    rows = []
    if os.path.isfile(fontes):
        with open(fontes, encoding='utf-8') as f:
            rows = [l.rstrip('\n').split('\t') for l in f.readlines()[1:] if l.strip()]
check('legado executa com 94 hashes', legado['rc'] == 0 and len(legado['hashes']) == 94, '%d' % len(legado['hashes']))
check('K2: nativo executa (rc 0) e resume', n['rc'] == 0 and n['kamui'].startswith('[kamui] nativo'),
      n['err'].strip()[-300:])
check('K2: 94 hashes iguais legado x nativo', len(n['hashes']) == 94 and n['hashes'] == legado['hashes'],
      'diferentes em %s' % [v for (v, a), (_, b) in zip(n['hashes'], legado['hashes']) if a != b][:8])
ref = os.environ.get('HH_AREA1_HASHES', '')
if os.environ.get('HH_EXIGIR_DADOS') == '1':
    check('K2: referencia do HH06 disponivel (HH_AREA1_HASHES)', bool(ref) and os.path.isfile(ref), ref)
if ref and os.path.isfile(ref):
    with open(ref, encoding='utf-8') as f:
        want = [(int(v), h) for v, h in re.findall(r'^(\d+): ([0-9a-f]+)\s*$', f.read(), re.M)]
    check('K2: nativo igual a referencia do HH06 (HH_AREA1_HASHES)', n['hashes'] == want and len(want) == 94)
nat = num(n['kamui'], r'nativos (\d+)')
check('K2: renders nativos = renders do tempo', nat is not None and nat > 0 and nat == n['tempo_renders'],
      '%s x %s' % (nat, n['tempo_renders']))
destinos = {r[0] for r in rows}

# K4
c = run('nativo', {'HH_RENDER_IR': 'conferir', 'HH_RENDER_IR_CADA': '5'})
ir = re.search(r'\[render-ir\] [^\n]*', c['err'])
line = ir.group(0) if ir else ''
v4 = {k: num(line, p) for k, p in (('invalida', r'IR invalida (\d+)'), ('tri', r'triangulos divergentes (\d+)'),
                                    ('img', r'imagens divergentes (\d+)'),
                                    ('tex', r'texturas conferidas \d+ \(divergentes (\d+)'),
                                    ('quadros', r'quadros divergentes (\d+)'), ('conferidos', r'conferidos (\d+)'))}
check('K4: conferir no nativo executa (rc 0) e o Kamui nativo atende', c['rc'] == 0 and c['kamui'].startswith('[kamui] nativo'),
      c['err'].strip()[-300:])
check('K4: renders conferidos com o estado nativo', (v4['conferidos'] or 0) >= 400, line)
check('K4: IR igual ao rasterizador: 0 divergencias',
      all(v4[k] == 0 for k in ('invalida', 'tri', 'img', 'tex', 'quadros')), str(v4))
check('K3: sonda gravou acessos', bool(rows))
check('K3: nada no TA nem na VRAM do runtime', not any(d.startswith(('ta_', 'vram_')) for d in destinos),
      sorted(d for d in destinos if d.startswith(('ta_', 'vram_'))))
check('K3: do PVR so os registradores de tempo do video (HH15)',
      all(d in SPG for d in destinos if d.startswith('pvr_')), sorted(d for d in destinos if d.startswith('pvr_') and d not in SPG))
check('K3: area 4 no destino nativo', any(d == 'kamui_poligonos' and int(r[3]) > 0 for r in rows for d in [r[0]]))

print('FALHOU (%d)' % fails if fails else 'OK')
sys.exit(1 if fails else 0)
