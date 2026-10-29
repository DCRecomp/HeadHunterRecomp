#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
"""HH13: portoes do renderizador nativo (docs/planos/HH13-PLANO.md) no roteiro newgame_area1 (4 700 VBlanks).

R1  conferir (HH_RENDER_IR=conferir, 1 render a cada 5): a IR valida descreve os triangulos, o estado, o quadro e as
    texturas que o rasterizador em software usou, e a imagem so da IR e identica: 0 divergencias; HH15.7a: a imagem do
    software sobre a IR (o dispositivo com as texturas convertidas, o software do produto) identica em TODO render da
    rota (sem amostragem), 0 falhas dele, e o negativo HH_RENDER_SOFT_MUTACAO=textura acusado com renders comparados;
R2  comparar (HH_RENDER_IR=comparar): o Vulkan contra o software em todos os renders: maior diferenca <= 2 niveis,
    nenhum quadro abaixo de 40 dB, 0 falhas do Vulkan;
R4  determinismo: duas execucoes do --render nativo com os mesmos quadros (hash a cada 50 VBlanks); equivalencia: iguais
    aos do software;
R5  desempenho: o --render nativo (Vulkan em todos os renders, 0 falhas) leva menos tempo de host que o software no mesmo
    roteiro (sem pacing).

uso: hh13_render.py <hh-run.exe> <raiz do projeto>
ambiente: HH_DATA_DIR (dados privados; sem ele = SKIP, rc 77). Sem Vulkan na maquina = SKIP (rc 77).
"""
import os
import re
import subprocess
import sys
import time

RUN, ROOT = (os.path.abspath(a) for a in sys.argv[1:3])
DATA = os.environ.get('HH_DATA_DIR', '')
SCRIPT = os.path.join(ROOT, 'tools', 'dist', 'roteiros', 'newgame_area1.txt')
VBLANKS, RTC = 4700, 1700000000
if not os.path.isfile(os.path.join(DATA, 'disc.manifest')):
    print('SKIP: defina HH_DATA_DIR')
    sys.exit(77)
fails = 0


def check(name, cond, info=''):
    global fails
    print('[%s] %s%s' % (' ok ' if cond else 'FAIL', name, (' (%s)' % info) if info else ''))
    fails += not cond


def run(render='software', env_extra=None, vblanks=VBLANKS):
    env = dict(os.environ, HH_INPUT_SCRIPT=SCRIPT)
    for k in ('HH_RENDER_IR', 'HH_RENDER_IR_CADA', 'HH_RENDER_IR_BMP'):
        env.pop(k, None)
    env.update(env_extra or {})
    t0 = time.monotonic()
    p = subprocess.run([RUN, '--data', DATA, '--vblanks', str(vblanks), '--rtc', str(RTC), '--every', '50',
                        '--render', render], capture_output=True, text=True, env=env)
    vk = re.search(r'\[render-vk\] [^\n]*', p.stderr)
    ir = re.search(r'\[render-ir\] renders (\d+)', p.stderr)
    return {'rc': p.returncode, 'err': p.stderr, 's': time.monotonic() - t0, 'vk': vk.group(0) if vk else '',
            'renders': int(ir.group(1)) if ir else None,
            'hashes': dict((int(v), h) for v, h in re.findall(r'quadro (\d+): \S+ hash ([0-9a-f]+)', p.stdout))}


def num(text, pat):
    m = re.search(pat, text)
    return int(m.group(1)) if m else None


# disponibilidade do Vulkan: uma execucao curta no nativo
# (SKIP so com a execucao valida e o Vulkan indisponivel dito pelo proprio render; qualquer outro erro falha)
probe = subprocess.run([RUN, '--data', DATA, '--vblanks', '300', '--rtc', str(RTC), '--render', 'nativo'],
                       capture_output=True, text=True, env=dict(os.environ, HH_INPUT_SCRIPT=SCRIPT))
if probe.returncode != 0:
    check('sonda do Vulkan executa (rc 0)', False, '%d: %s' % (probe.returncode, probe.stderr.strip()[-300:]))
    sys.exit(1)
if '[render] Vulkan:' not in probe.stderr and 'Vulkan indisponivel' in probe.stderr:
    print('SKIP: sem Vulkan nesta maquina')
    sys.exit(77)
check('sonda: o Vulkan abre', '[render] Vulkan:' in probe.stderr, probe.stderr.strip()[-300:])

# R1
c = run(env_extra={'HH_RENDER_IR': 'conferir', 'HH_RENDER_IR_CADA': '5'})
ir = re.search(r'\[render-ir\] [^\n]*', c['err'])
line = ir.group(0) if ir else ''
zeros = ['IR invalida', 'triangulos divergentes', 'imagens divergentes', 'divergentes', 'quadros divergentes']
vals = {
    'invalida': num(line, r'IR invalida (\d+)'), 'tri': num(line, r'triangulos divergentes (\d+)'),
    'img': num(line, r'imagens divergentes (\d+)'), 'tex': num(line, r'texturas conferidas \d+ \(divergentes (\d+)'),
    'quadros': num(line, r'quadros divergentes (\d+)'), 'conferidos': num(line, r'conferidos (\d+)'),
    'volumes_abertos': num(line, r'sem fechamento (\d+)'),
}
check('R1 execucao termina (rc 0)', c['rc'] == 0, str(c['rc']))
check('R1 renders conferidos', (vals['conferidos'] or 0) >= 400, str(vals['conferidos']))
check('R1 IR valida e igual ao rasterizador (triangulos, imagem, texturas, quadro): 0 divergencias',
      all(vals[k] == 0 for k in ('invalida', 'tri', 'img', 'tex', 'quadros', 'volumes_abertos')), str(vals))

def soft_line(err):
    m = re.search(r'\[render-soft\] [^\n]*', err)
    return m.group(0) if m else ''


def soft_vals(line):
    return {'quadros': num(line, r'quadros (\d+)'), 'falhas': num(line, r'falhas (\d+)'),
            'comparados': num(line, r'conferidos com o caminho do TA (\d+)'),
            'divergentes': num(line, r'imagens divergentes (\d+)\)')}


sv = soft_vals(soft_line(c['err']))
check('R1 software sobre a IR identico ao rasterizador sobre o TA em todo render da rota, 0 falhas',
      c['renders'] and sv['comparados'] == c['renders'] and sv['falhas'] == 0 and sv['divergentes'] == 0,
      '%s renders, %s' % (c['renders'], sv))
m = run(env_extra={'HH_RENDER_IR': 'conferir', 'HH_RENDER_IR_CADA': '5', 'HH_RENDER_SOFT_MUTACAO': 'textura'}, vblanks=1500)
mv = soft_vals(soft_line(m['err']))
check('R1 negativo: textura mudada no software sobre a IR acusada (renders comparados, sem falha do software)',
      m['rc'] == 0 and (mv['comparados'] or 0) > 0 and mv['falhas'] == 0 and (mv['divergentes'] or 0) > 0, str(mv))

# R2
k = run(env_extra={'HH_RENDER_IR': 'comparar'})
vk = re.search(r'\[render-vk\] [^\n]*', k['err'])
vline = vk.group(0) if vk else ''
comp, falhas = num(vline, r'comparados (\d+)'), num(vline, r'falhas (\d+)')
renders = k['renders']
maior, abaixo = num(vline, r'maior diferenca (\d+)'), num(vline, r'quadros abaixo de 40 dB (\d+)')
check('R2 Vulkan x software em todos os renders', k['rc'] == 0 and renders and renders >= 2000 and comp == renders,
      '%s comparados de %s renders' % (comp, renders))
check('R2 maior diferenca <= 2 niveis, nenhum quadro < 40 dB, 0 falhas',
      maior is not None and maior <= 2 and abaixo == 0 and falhas == 0, 'maior %s, abaixo %s, falhas %s' % (maior, abaixo, falhas))

# R4 e R5
sw = run('software')
nat = run('nativo')
nat2 = run('nativo')
check('R4 determinismo: duas execucoes do nativo com os mesmos quadros',
      nat['rc'] == 0 and nat2['rc'] == 0 and len(nat['hashes']) >= 90 and nat['hashes'] == nat2['hashes'],
      '%d e %d quadros' % (len(nat['hashes']), len(nat2['hashes'])))
check('R4 equivalencia: os quadros do nativo iguais aos do software (hash a cada 50 VBlanks)',
      sw['rc'] == 0 and len(sw['hashes']) >= 90 and sw['hashes'] == nat['hashes'],
      '%d e %d quadros' % (len(sw['hashes']), len(nat['hashes'])))
vkq, vkf = num(nat['vk'], r'quadros (\d+), falhas'), num(nat['vk'], r'falhas (\d+)')
check('R5 o nativo usou o Vulkan em todos os renders, sem falha nem recuo',
      nat['renders'] and vkq == nat['renders'] and vkf == 0 and 'renderizador em software' not in nat['err'],
      'Vulkan %s de %s renders, falhas %s' % (vkq, nat['renders'], vkf))
best_nat = min(nat['s'], nat2['s'])
check('R5 o nativo mais rapido que o software (tempo de host, sem pacing)', best_nat < sw['s'],
      'software %.1f s, nativo %.1f/%.1f s' % (sw['s'], nat['s'], nat2['s']))

print('hh13_render: %d falhas' % fails)
sys.exit(1 if fails else 0)
