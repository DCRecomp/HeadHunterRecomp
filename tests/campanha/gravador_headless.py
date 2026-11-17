# SPDX-License-Identifier: MIT
"""Adaptador do teste do monitor com o jogo de verdade (test_campanha_real.py, HH_CAMPANHA_PROGRAMA).

O --gravar do HeadHunter.exe exige a janela; no teste, a "sessao gravada" e o roteiro CAMPANHA_TESTE_ROTEIRO jogado pelo
proprio HeadHunter.exe sem janela (--headless --reproduzir, o mesmo tempo logico que o --gravar forca), com todos os
outros argumentos do monitor (--data, --rtc, --saves-pasta, --log, --estado), e o roteiro copiado como a gravacao.
Qualquer outra chamada (as reproducoes do monitor) passa direto ao executavel (HH_CAMPANHA_EXE); com CAMPANHA_TESTE_TROCA, a
reproducao recebe a entrada com o analogico da leitura 4500 trocado (divergencia real injetada).
"""
import os
import shutil
import subprocess
import sys

exe = os.environ['HH_CAMPANHA_EXE']
a = sys.argv[1:]
if '--gravar' in a:
    i = a.index('--gravar')
    gravacao = a[i + 1]
    a = a[:i] + a[i + 2:]
    if os.path.exists(gravacao):
        sys.exit('a gravacao %s ja existe' % gravacao)
    shutil.copyfile(os.environ['CAMPANHA_TESTE_ROTEIRO'], gravacao)
    a = ['--headless', '--reproduzir', gravacao, '--vblanks', os.environ['CAMPANHA_TESTE_VBLANKS']] + a
elif os.environ.get('CAMPANHA_TESTE_TROCA') and '--reproduzir' in a:
    i = a.index('--reproduzir') + 1
    trocada = os.path.join(os.path.dirname(os.path.abspath(a[a.index('--log') + 1])), 'entrada_trocada.txt')
    with open(a[i], encoding='ascii') as f, open(trocada, 'w', encoding='ascii') as g:
        for l in f:
            g.write('4500 ANALOG_LEFT\n' if l.strip() == '4500 ANALOG_UP' else l)
    a[i] = trocada
env = {k: v for k, v in os.environ.items() if k not in ('HH_INPUT_SCRIPT', 'CAMPANHA_TESTE_TROCA')}
p = subprocess.run([exe] + a, env=env, stderr=subprocess.PIPE, text=True, errors='replace')
sys.stderr.write(p.stderr)
if '--gravar' in sys.argv[1:]:
    # fecha a gravacao como o --gravar fecha (ReadRecorder::comment no fim da sessao, app/hh_window.cpp)
    ult = [l.split()[-1] for l in p.stderr.splitlines() if l.startswith('[entrada] ultima leitura ')]
    with open(gravacao, 'a', encoding='ascii') as f:
        f.write('# fim da sessao, ultima leitura %s\n' % (ult[-1] if ult else '18446744073709551615'))
sys.exit(p.returncode)
