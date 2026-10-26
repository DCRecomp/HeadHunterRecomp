#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
"""HH12: portoes dos filmes nativos (docs/planos/HH12-PLANO.md).

Prepara (ou reaproveita) <HH_DATA_DIR>\\video com o hh-builder --filmes (FFmpeg como processo externo) e roda o hh-run.

V0  estatico: no gerado compilado, as chamadas de PlayScene (0x8C09E904) ao objeto do filme passam pelo slot (HHG_SLOT);
V1  no nativo, 0 entradas no stream do arquivo e no decodificador 4X recompilados; no legado (referencia) o stream e
    criado e aberto (negativo: o portao distingue os dois);
V2  contrato: SCENE0 sem entrada comeca no VBlank 282 (o mesmo do legado) e acaba no fim natural depois dos 512 quadros
    (instante do quadro 512 pelo relogio do jogo); com START (roteiro newgame_area1, leitura 1000) o callback do jogo o
    para, e o filme seguinte (SCENE01_06) e aberto; filme ausente (catalogo vazio): o jogo segue sem ele; mod
    (mods\\video\\SCENE0.mp4 = outro filme) toca no lugar, pela duracao do proprio arquivo;
V3  conversao: os 53 filmes no manifesto, com os quadros do vtrk (o Builder recusa diferente), PSNR medio >= 36 dB contra
    o decodificado pelo FFmpeg e SNR do audio >= 25 dB, atraso 0 (a referencia do decodificador do proprio jogo no
    Flycast continua em aberto: plano, "Referencia do V3");
V4  determinismo: duas execucoes do SCENE0 dao os mesmos quadros; o tempo experimental (espera declarada 0x8C11FC3A) da
    o mesmo inicio, o mesmo fim e os mesmos quadros do filme.

uso: hh12_video.py <hh-run.exe> <hh-builder.exe> <raiz do projeto> [<pasta do gerado>]
ambiente: HH_DATA_DIR (dados privados do disco 1; sem ele = SKIP, rc 77); HH_FFMPEG (opcional; padrao ffmpeg no PATH).
"""
import os
import re
import shutil
import struct
import subprocess
import sys
import tempfile

RUN, BUILDER, ROOT = (os.path.abspath(a) for a in sys.argv[1:4])
GEN = os.path.abspath(sys.argv[4]) if len(sys.argv) > 4 else ''
DATA = os.environ.get('HH_DATA_DIR', '')
SCRIPT = os.path.join(ROOT, 'tools', 'dist', 'roteiros', 'newgame_area1.txt')
RTC = 1700000000
if not os.path.isfile(os.path.join(DATA, 'disc.manifest')):
    print('SKIP: defina HH_DATA_DIR')
    sys.exit(77)

# chamadas de PlayScene ao objeto do filme atendidas pelo provedor (transitional/video_adapter.h)
SLOTS = ('8C11E9D2', '8C11EBE0', '8C11ECA4', '8C11EB32')
# stream do arquivo (criar e abrir: so o open 0x8C11E9D2; proximo quadro e fim: so o play) e as partes do decodificador 4X
# chamadas so pelo laco 0x8C11ECA4 (varredura dos chamadores no gerado; 0x8C120BCA, 0x8C121270, 0x8C121464 e 0x8C121A82
# ficam de fora: o jogo tambem as chama fora do filme)
INTERNAS = ('8C11FFA0', '8C11FCC6', '8C11FEF2', '8C11FED4', '8C11FDDA', '8C120610', '8C121156')
fails = 0
tmp = tempfile.mkdtemp(prefix='hh12_v_')


def check(name, cond, info=''):
    global fails
    print('[%s] %s%s' % (' ok ' if cond else 'FAIL', name, (' (%s)' % info) if info else ''))
    fails += not cond


def run(vblanks, video='nativo', script=None, extra=(), env_extra=None, watch=False, every=10):
    env = dict(os.environ)
    env.pop('HH_INPUT_SCRIPT', None)
    env.pop('HH_VIDEO_DADOS', None)
    if script:
        env['HH_INPUT_SCRIPT'] = script
    env.update(env_extra or {})
    cmd = [RUN, '--data', DATA, '--vblanks', str(vblanks), '--rtc', str(RTC), '--every', str(every), '--video', video] + list(extra)
    if watch:
        for a in INTERNAS:
            cmd += ['--watch', a]
    p = subprocess.run(cmd, capture_output=True, text=True, env=env)
    movies = []
    for m in re.finditer(r'\[video\] (\S+): inicio no VBlank (\d+) \((\d+) quadros, \d+ legendas\)', p.stderr):
        movies.append({'nome': m.group(1), 'inicio': int(m.group(2)), 'quadros': int(m.group(3))})
    for i, m in enumerate(re.finditer(r'\[video\] (\S+): (fim natural|parado pelo jogo \(skip\)) no VBlank (\d+)', p.stderr)):
        if i < len(movies):
            movies[i].update(fim=int(m.group(3)), como='natural' if m.group(2) == 'fim natural' else 'jogo')
    st = re.search(r'\[video\] nativo: (\d+) filmes no catalogo, open (\d+) \(ausentes (\d+)\), play (\d+)', p.stderr)
    hashes = dict((int(v), h) for v, h in re.findall(r'quadro (\d+): \S+ hash ([0-9a-f]+)', p.stdout))
    return {'rc': p.returncode, 'out': p.stdout, 'err': p.stderr, 'movies': movies, 'hashes': hashes,
            'stats': tuple(map(int, st.groups())) if st else None,
            'watch': re.findall(r'watch vblank \d+: ([0-9A-F]{8})', p.stdout)}


def vblanks_for(frames, fps=30.0):
    """VBlanks (59,94 Hz) ate o instante do quadro `frames` (o fim natural do laco)."""
    t = int(frames * 1e7 / fps + 0.5)
    n = 0
    while n * 10000000 * 1001 // 60000 < t:
        n += 1
    return n


# preparacao e V3 por disco: os filmes convertidos do proprio disco (reaproveitados quando ja estao la); o disco 2
# (HH_DATA_DIR2, opcional fora da regressao) tem 28 filmes, o disco 1 tem 25
campo = lambda f, k, conv=float: conv(re.search(r'(?m)^%s = "?([^"\n]+)"?' % k, f).group(1)) if re.search(r'(?m)^%s = ' % k, f) else None


def disco(dados, esperado, nome):
    cmd = [BUILDER, '--filmes', dados] + (['--ffmpeg', os.environ['HH_FFMPEG']] if os.environ.get('HH_FFMPEG') else [])
    p = subprocess.run(cmd, capture_output=True, text=True)
    print(p.stdout.strip().splitlines()[-1] if p.stdout.strip() else p.stderr.strip())
    check('%s preparacao: filmes convertidos pelo hh-builder --filmes' % nome, p.returncode == 0, p.stderr.strip()[-300:])
    if p.returncode != 0:
        sys.exit(1)
    man = open(os.path.join(dados, 'video', 'manifest.toml'), encoding='utf-8').read()
    filmes = re.split(r'\n\[\[filme\]\]\n', man)[1:]
    psnr = [campo(f, 'psnr_med') for f in filmes]
    snr = [campo(f, 'snr_audio') for f in filmes if campo(f, 'canais', int)]
    atraso = {campo(f, 'atraso_audio_amostras', int) for f in filmes}
    vtrk_ok = True
    for f in filmes:  # quadros do manifesto == vtrk[4] do proprio arquivo (leitura independente aqui)
        src = os.path.join(dados, 'disc', *campo(f, 'original', str).split('/'))
        b = open(src, 'rb').read(65536)
        i = b.find(b'vtrk')
        vtrk_ok &= i > 0 and struct.unpack_from('<I', b, i + 8 + 16)[0] == campo(f, 'quadros', int)
    check('%s V3 os %d filmes do disco no manifesto, nenhum ausente' % (nome, esperado),
          len(filmes) == esperado and 'ausente' not in man, '%d filmes' % len(filmes))
    check('%s V3 quadros convertidos == vtrk[4] em todos' % nome, vtrk_ok)
    check('%s V3 PSNR medio >= 36 dB contra o FFmpeg' % nome, min(psnr) >= 36.0, 'menor %.2f' % min(psnr))
    check('%s V3 audio: SNR >= 25 dB e atraso 0' % nome, (not snr or min(snr) >= 25.0) and atraso == {0},
          'menor %.1f, atrasos %s' % (min(snr) if snr else 0, sorted(atraso)))


disco(DATA, 25, 'disco 1')
DATA2 = os.environ.get('HH_DATA_DIR2', '')
if os.path.isfile(os.path.join(DATA2, 'disc.manifest')):
    disco(DATA2, 28, 'disco 2')
elif os.environ.get('HH_EXIGIR_DADOS') == '1':
    check('disco 2: HH_DATA_DIR2 (exigido)', False)
else:
    print('[info] disco 2 pulado: sem HH_DATA_DIR2')

# V0: estatico
if GEN and os.path.isdir(GEN):
    import glob
    text = ''.join(open(f, encoding='utf-8', errors='replace').read() for f in glob.glob(os.path.join(GEN, 'funcs_*.cpp')))
    i = text.find('void f_8C09E904(')
    body = text[i:text.find('\nvoid f_', i + 1)]
    pelo_slot = i >= 0 and all(('HHG_SLOT(%s)' % a) in body for a in SLOTS)
    check('V0 PlayScene chama o objeto do filme pelo slot', pelo_slot)
else:
    print('[info] V0 pulado: sem a pasta do gerado')

# V1/V2: SCENE0 inteiro, sem entrada
n = vblanks_for(512)
a = run(282 + n + 40, watch=True)
b = run(282 + n + 40)
leg = run(330, video='legado', watch=True)
s0 = a['movies'][0] if a['movies'] else {}
check('execucoes terminam (rc 0)', a['rc'] == 0 and b['rc'] == 0 and leg['rc'] == 0, '%d %d %d' % (a['rc'], b['rc'], leg['rc']))
check('V1 nativo: 0 entradas no stream e no decodificador 4X', a['watch'] == [], ','.join(sorted(set(a['watch'])))[:200])
check('V1 legado (referencia): o stream do arquivo e criado e aberto', {'8C11FFA0', '8C11FCC6'} <= set(leg['watch']),
      ','.join(sorted(set(leg['watch']))))
check('V2 SCENE0 comeca no VBlank 282 (o do legado: primeira entrada no stream)', s0.get('nome') == 'MOVIES/SCENE0.4XM'
      and s0.get('inicio') == 282 and s0.get('quadros') == 512, str(s0))
check('V2 SCENE0 acaba no fim natural depois dos 512 quadros', s0.get('como') == 'natural' and
      s0.get('fim', 0) - s0.get('inicio', 0) == n, 'fim %s, esperado %d' % (s0.get('fim'), s0.get('inicio', 0) + n))
mid = [v for v in a['hashes'] if 300 <= v <= 282 + n]
check('V4 determinismo: duas execucoes, mesmos quadros no filme e depois',
      mid and all(a['hashes'][v] == b['hashes'].get(v) for v in a['hashes']), '%d quadros' % len(a['hashes']))
distintos = len({a['hashes'][v] for v in mid})
check('V2 o filme aparece na tela (quadros do filme variam)', distintos > 20, '%d quadros distintos' % distintos)

# V4: tempo experimental
x = run(282 + n + 40, extra=('--tempo', 'experimental'), every=1)
x0 = x['movies'][0] if x['movies'] else {}
# o experimental chega antes ao filme (o tempo so anda nas esperas declaradas): compara a partir do inicio
dx = x0.get('inicio', 0) - s0.get('inicio', 0)
check('V4 experimental: mesma duracao do SCENE0 (fim natural)', x0.get('como') == 'natural' and
      x0.get('fim', 0) - x0.get('inicio', 0) == s0.get('fim', 0) - s0.get('inicio', 0), str(x0))
rel = [v for v in mid if (v + dx) in x['hashes']]
check('V4 experimental: mesmos quadros do filme no mesmo instante relativo ao inicio',
      len(rel) > 50 and all(x['hashes'][v + dx] == a['hashes'][v] for v in rel), '%d quadros, deslocamento %d' % (len(rel), dx))

# V2: START no roteiro (leitura 1000) para o SCENE0 pelo callback do jogo; o SCENE01_06 abre depois
r = run(1700, script=SCRIPT)
nomes = [m['nome'] for m in r['movies']]
check('V2 START: SCENE0 parado pelo jogo', r['movies'] and r['movies'][0].get('como') == 'jogo', str(r['movies'][:1]))
check('V2 depois do skip o jogo segue e abre o SCENE01_06', 'MOVIES/SCENE01_06.4XM' in nomes, str(nomes))

# V2: catalogo vazio -> ausente, o jogo segue (sem o filme na tela)
vazio = os.path.join(tmp, 'video_vazio')
os.makedirs(vazio)
z = run(600, env_extra={'HH_VIDEO_DADOS': vazio})
check('V2 ausente: open conta o ausente e o jogo segue', z['rc'] == 0 and z['stats'] and z['stats'][2] >= 1 and
      z['movies'] == [], str(z['stats']))

# V2: mod pelo nome (SCENE0.mp4 = o SCENE07_08 convertido, 1205 quadros): toca pela duracao do proprio arquivo
mods = os.path.join(tmp, 'mods')
os.makedirs(os.path.join(mods, 'video'))
os.makedirs(os.path.join(mods, 'assets'))
shutil.copyfile(os.path.join(DATA, 'video', 'SCENE07_08.mp4'), os.path.join(mods, 'video', 'SCENE0.mp4'))
nm = vblanks_for(1205)
m = run(282 + nm + 40, extra=('--mods', os.path.join(mods, 'assets')))
m0 = m['movies'][0] if m['movies'] else {}
check('V2 mod: SCENE0 trocado toca ate o fim do arquivo do mod', m0.get('como') == 'natural' and
      m0.get('fim', 0) - m0.get('inicio', 0) in (nm, nm + 1), 'fim %s, esperado ~%d' % (m0.get('fim'), m0.get('inicio', 0) + nm))

# V2: o MP4 do Builder trocado (um byte) nao toca: o open o recusa pelo SHA-256 do manifesto e o jogo segue sem ele
alt = os.path.join(tmp, 'video_alterado')
shutil.copytree(os.path.join(DATA, 'video'), alt, ignore=shutil.ignore_patterns('SCENE[1-9]*', 'SCENE0[1-9L]*'))
with open(os.path.join(alt, 'SCENE0.mp4'), 'r+b') as f:
    f.seek(4096)
    byte = f.read(1)
    f.seek(4096)
    f.write(bytes([byte[0] ^ 0x5A]))
q = run(600, env_extra={'HH_VIDEO_DADOS': alt})
check('V2 MP4 alterado: recusado pelo SHA-256 (ausente, com o motivo) e o jogo segue',
      q['rc'] == 0 and q['stats'] and q['stats'][2] >= 1 and 'nao confere com o manifesto' in q['err'] and q['movies'] == [],
      str(q['stats']))

# V2: legendas do jogo (opcao ligada por padrao): o SCENE01_06 traz as suas (lidas da lista do jogo), o SCENE0 nao tem;
# com a legenda na tela o quadro muda e sem ela (antes da primeira) e o do filme
srot = os.path.join(tmp, 'rot_legenda.txt')
with open(srot, 'w') as f:
    f.write('# SCENE0 pulado com START; NEW GAME nao: o titulo ocioso abre o SCENE01_06\n1000 START\n1030 -\n1300 A\n1330 -\n')
L = run(2400, script=srot, every=1)
s01 = [m for m in L['movies'] if m['nome'] == 'MOVIES/SCENE01_06.4XM']
leg = re.search(r'SCENE01_06\.4XM: inicio no VBlank \d+ \(\d+ quadros, (\d+) legendas\)', L['err'])
check('V2 legendas: o SCENE01_06 le as legendas do jogo', bool(s01) and leg and int(leg.group(1)) > 10,
      leg.group(1) if leg else 'sem linha')
S0 = re.search(r'SCENE0\.4XM: inicio no VBlank \d+ \(\d+ quadros, (\d+) legendas\)', L['err'])
check('V2 legendas: o SCENE0 nao tem', S0 and S0.group(1) == '0')

shutil.rmtree(tmp, ignore_errors=True)
print('hh12_video: %d falhas' % fails)
sys.exit(1 if fails else 0)
