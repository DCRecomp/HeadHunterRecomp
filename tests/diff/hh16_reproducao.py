#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
"""HH16.0: portao da reproducao da campanha sem janela (docs/planos/HH16-PLANO.md, HH16.0).

Um roteiro por leitura do controle tem o formato da gravacao do --gravar (ReadRecorder): o roteiro
newgame_area1_nativo faz o papel de uma sessao gravada. HeadHunter.exe --headless --reproduzir, no software e no tempo
logico (forcado), com --log (quadros a cada 50 VBlanks) e --estado (RAM do jogo a cada 60 leituras).

Positivos:
  - duas reproducoes: quadros e estados identicos (determinismo);
  - a reproducao e igual ao mesmo roteiro por HH_INPUT_SCRIPT com --tempo experimental (o --reproduzir e so o roteiro).
Negativos:
  - uma entrada trocada (o analogico da leitura 4500): estados iguais antes, o primeiro diferente logo depois;
  - --reproduzir com --tempo nativo e --gravar com --headless: recusados (rc 2);
  - --gravar sobre um arquivo existente: recusado sem tocar no arquivo.

uso: hh16_reproducao.py <HeadHunter.exe> <raiz do projeto>
ambiente: HH_DATA_UNICO = uma instalacao com os dois discos (game-data/runtime/unico: o HeadHunter.exe exige o disco 2);
sem eles = SKIP (rc 77).
"""
import os
import subprocess
import sys
import tempfile

EXE, ROOT = (os.path.abspath(a) for a in sys.argv[1:3])
DATA = os.environ.get('HH_DATA_UNICO', '')
ROTEIRO = os.path.join(ROOT, 'tools', 'dist', 'roteiros', 'newgame_area1_nativo.txt')
VBLANKS, RTC = 5300, 1700000000
if not os.path.isfile(os.path.join(DATA, 'disc.manifest')):
    print('SKIP: defina HH_DATA_UNICO (instalacao com os dois discos)')
    sys.exit(77)

fails = 0
tmp = tempfile.mkdtemp(prefix='hh16_rep_')


def check(nome, cond, info=''):
    global fails
    print('[%s] %s%s' % (' ok ' if cond else 'FAIL', nome, (' (%s)' % info) if info else ''))
    fails += not cond


def roda(tag, extra, env=None):
    log, est, saves = (os.path.join(tmp, tag + s) for s in ('.log', '.estado', '_saves'))
    os.makedirs(saves)
    e = {k: v for k, v in os.environ.items() if k != 'HH_INPUT_SCRIPT'}
    e.update(env or {})
    p = subprocess.run([EXE, '--headless', '--data', DATA, '--vblanks', str(VBLANKS), '--rtc', str(RTC), '--saves-pasta',
                        saves, '--log', log, '--hash-every', '50', '--estado', est] + extra,
                       capture_output=True, text=True, env=e)
    ler = lambda p_: open(p_).read().splitlines() if os.path.exists(p_) else []
    return p, ler(log), ler(est)


a, qa, ea = roda('a', ['--reproduzir', ROTEIRO])
check('reproducao A terminou pelo limite', a.returncode == 0 and 'limite de VBlanks' in a.stdout,
      'rc %d %s' % (a.returncode, a.stdout.strip()[-120:]))
# o --log comeca no quadro 50
check('quadros e estados gravados', len(qa) == VBLANKS // 50 and len(ea) > 80,
      '%d quadros, %d estados' % (len(qa), len(ea)))
b, qb, eb = roda('b', ['--reproduzir', ROTEIRO])
check('duas reproducoes: quadros identicos', qa == qb and qa)
check('duas reproducoes: estados identicos', ea == eb and ea)
c, qc, ec = roda('c', ['--tempo', 'experimental'], {'HH_INPUT_SCRIPT': ROTEIRO})
check('reproducao = roteiro por HH_INPUT_SCRIPT no tempo logico', qa == qc and ea == ec)

# negativo: uma entrada trocada na leitura 4500
mut = os.path.join(tmp, 'roteiro_trocado.txt')
with open(ROTEIRO) as f, open(mut, 'w') as g:
    for l in f:
        g.write('4500 ANALOG_LEFT\n' if l.strip() == '4500 ANALOG_UP' else l)
d, qd, ed = roda('d', ['--reproduzir', mut])
leit = lambda e: {int(l.split()[1]): l.split()[2] for l in e if l.startswith('estado ')}
la, ld = leit(ea), leit(ed)
prim = next((n for n in sorted(la) if ld.get(n) != la[n]), None)
check('entrada trocada: estados iguais antes da leitura 4500', all(ld.get(n) == la[n] for n in la if n < 4500))
check('entrada trocada: o primeiro estado diferente logo depois', prim is not None and 4500 <= prim <= 4620, 'leitura %s' % prim)

# negativos de uso
n1 = subprocess.run([EXE, '--headless', '--data', DATA, '--reproduzir', ROTEIRO, '--tempo', 'nativo', '--vblanks', '10'],
                    capture_output=True, text=True)
check('--reproduzir com --tempo nativo recusado', n1.returncode == 2 and 'tempo logico' in n1.stderr, n1.stderr.strip()[-100:])
n2 = subprocess.run([EXE, '--headless', '--data', DATA, '--gravar', os.path.join(tmp, 'g.txt'), '--vblanks', '10'],
                    capture_output=True, text=True)
check('--gravar com --headless recusado', n2.returncode == 2 and 'exige a janela' in n2.stderr, n2.stderr.strip()[-100:])
existente = os.path.join(tmp, 'existente.txt')
with open(existente, 'w') as f:
    f.write('nao mexer\n')
# a janela nao abre no teste: --gravar sobre arquivo existente falha antes do SDL (criacao exclusiva)
os.makedirs(os.path.join(tmp, 'n3_saves'))
n3 = subprocess.run([EXE, '--data', DATA, '--saves-pasta', os.path.join(tmp, 'n3_saves'), '--gravar', existente,
                     '--vblanks', '10'], capture_output=True, text=True)
check('--gravar sobre arquivo existente recusado sem tocar nele',
      n3.returncode != 0 and 'nunca e sobrescrita' in n3.stdout and open(existente).read() == 'nao mexer\n',
      (n3.stdout + n3.stderr).strip()[-120:])

print('%d falhas' % fails)
sys.exit(1 if fails else 0)
