#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
"""HH13.6: portoes do provedor nativo da camada Kamui (docs/planos/HH13-PLANO.md, "HH13.6") no roteiro newgame_area1.

K1  sombra (HH_KAMUI=sombra): os cinco funis cobrem tudo o que o render le; estado nativo igual ao do runtime em todos
    os STARTRENDER (registradores, parametros, memoria): 0 divergencias, conferidos > 0;
K2  nativo (HH_KAMUI=nativo, render em software): os 94 hashes iguais aos do caminho legado e a referencia do HH06
    (HH_AREA1_HASHES, quando definida); renders nativos = renders do provedor de tempo;
K4  conferir (HH_RENDER_IR=conferir, 1 render a cada 5) com o provedor nativo: a IR do estado nativo descreve o que o
    rasterizador usou, render a render: 0 divergencias;
K5  HH15.7b: o ListSink nativo (as listas decodificadas direto para a IR, o caminho do produto) igual a IR que a captura
    monta do decodificador do legado em TODO render do conferir no nativo: 0 divergencias; negativo
    HH_LISTA_NATIVA_MUTACAO=vertice acusado; e o nativo padrao (sem HH_RENDER_IR) desenha pela IR do ListSink;
K6  HH15.7c: o estado nativo (estado do quadro, memoria das texturas, imagem exibida) sem emprestimo ao PVR do runtime:
    no conferir, os parametros do quadro, os texels de cada textura, a gravacao do framebuffer e o quadro exibido pelas
    funcoes nativas iguais aos das do runtime em todo render, toda textura e todo VBlank (0 divergencias, 0 texturas sem
    conversao nativa); na sombra (o runtime com o banco e a VRAM dele), o quadro e as texturas do estado nativo iguais
    aos do runtime em todo render (o K1 confere os registradores, com a semantica do estado nativo); negativos
    HH_KAMUI_NATIVO_MUTACAO=quadro|textura|framebuffer|video, cada um acusado so pelo seu comparador;
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


def run(kamui, extra=None, vblanks=None):
    env = dict(os.environ, HH_INPUT_SCRIPT=SCRIPT)
    for k in ('HH_RENDER_IR', 'HH_RENDER_IR_CADA', 'HH_RENDER_IR_BMP', 'HH_KAMUI', 'HH_TA_FONTES'):
        env.pop(k, None)
    env['HH_KAMUI'] = kamui
    env.update(extra or {})
    p = subprocess.run([RUN, '--data', DATA, '--vblanks', str(vblanks or VBLANKS), '--rtc', str(RTC), '--every', '50',
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


def lista(err):
    m = re.search(r'\[render-lista\] ListSink conferido com a captura em (\d+) renders: divergentes (\d+)', err)
    return (int(m.group(1)), int(m.group(2))) if m else (None, None)


renders_ir = num(line, r'renders (\d+)')
lc, ld = lista(c['err'])
check('K5: ListSink igual a captura em todo render do conferir no nativo', lc and lc == renders_ir and ld == 0,
      '%s de %s renders, %s divergentes' % (lc, renders_ir, ld))
mut = run('nativo', {'HH_RENDER_IR': 'conferir', 'HH_RENDER_IR_CADA': '5', 'HH_LISTA_NATIVA_MUTACAO': 'vertice'}, 1500)
mc, md = lista(mut['err'])
check('K5 negativo: vertice mudado no ListSink acusado', mut['rc'] == 0 and mc and md and md > 0, '%s de %s' % (md, mc))
soft = re.search(r'\[render-soft\] quadros (\d+), falhas (\d+)', n['err'])
check('K5: o nativo padrao desenha pela IR do ListSink em todo render (software sobre a IR, sem o caminho do TA)',
      soft is not None and int(soft.group(1)) == nat and int(soft.group(2)) == 0 and '[render-lista]' not in n['err'],
      soft.group(0) if soft else 'sem resumo [render-soft]')


def nativo(err, kamui):
    m = re.search(r'\[render-nativo\] quadros conferidos (\d+) \(divergentes (\d+)\); texturas conferidas (\d+) de (\d+) '
                  r'\(divergentes (\d+), sem conversao nativa (\d+)\)', err)
    k = re.search(r'framebuffer conferido (\d+) \(divergentes (\d+)\); quadros exibidos conferidos (\d+) '
                  r'\(divergentes (\d+)\)', kamui)
    v = [int(x) for x in m.groups()] if m else [None] * 6
    w = [int(x) for x in k.groups()] if k else [None] * 4
    return {'quadro': v[1], 'quadros': v[0], 'textura': v[4], 'texturas': v[2], 'total_texturas': v[3],
            'sem_conversao': v[5], 'framebuffer': w[1], 'fbs': w[0], 'video': w[3], 'videos': w[2]}


k6 = nativo(c['err'], c['kamui'])
check('K6: as funcoes nativas iguais as do runtime em todo render, textura e VBlank do conferir',
      k6['quadros'] == renders_ir and k6['fbs'] == renders_ir and (k6['texturas'] or 0) > 0
      and k6['texturas'] == k6['total_texturas'] and k6['videos'] == VBLANKS
      and all(k6[x] == 0 for x in ('quadro', 'textura', 'sem_conversao', 'framebuffer', 'video')), str(k6))
# o estado nativo contra o do runtime de forma independente: na sombra o runtime usa o banco e a VRAM dele (sem
# emprestimo), e a captura confere o quadro e as texturas lidos do estado nativo com os lidos deles
ks = nativo(s['err'], s['kamui'])
check('K6: na sombra, o estado nativo (quadro e texturas) igual ao do runtime em todo render',
      ks['quadros'] and ks['quadros'] == vals['renders'] and ks['texturas'] == ks['total_texturas'] and
      (ks['texturas'] or 0) > 0 and all(ks[x] == 0 for x in ('quadro', 'textura', 'sem_conversao')), str(ks))
for campo in ('quadro', 'textura', 'framebuffer', 'video'):
    mm = run('nativo', {'HH_RENDER_IR': 'conferir', 'HH_RENDER_IR_CADA': '5', 'HH_KAMUI_NATIVO_MUTACAO': campo}, 1500)
    mv = nativo(mm['err'], mm['kamui'])
    outros = [x for x in ('quadro', 'textura', 'framebuffer', 'video') if x != campo]
    check('K6 negativo %s: acusado so pelo seu comparador' % campo,
          mm['rc'] == 0 and (mv[campo] or 0) > 0 and all(mv[x] == 0 for x in outros), str(mv))
check('K3: sonda gravou acessos', bool(rows))
check('K3: nada no TA nem na VRAM do runtime', not any(d.startswith(('ta_', 'vram_')) for d in destinos),
      sorted(d for d in destinos if d.startswith(('ta_', 'vram_'))))
check('K3: do PVR so os registradores de tempo do video (HH15)',
      all(d in SPG for d in destinos if d.startswith('pvr_')), sorted(d for d in destinos if d.startswith('pvr_') and d not in SPG))
check('K3: area 4 no destino nativo', any(d == 'kamui_poligonos' and int(r[3]) > 0 for r in rows for d in [r[0]]))

print('FALHOU (%d)' % fails if fails else 'OK')
sys.exit(1 if fails else 0)
