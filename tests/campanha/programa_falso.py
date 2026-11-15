# SPDX-License-Identifier: MIT
"""Programa falso para os testes do monitor de campanha (tools/dev/campanha.py, HH_CAMPANHA_PROGRAMA).

Imita o contrato do HeadHunter.exe usado pelo monitor, sem jogo: --gravar grava a entrada (a lista de FAKE_ENTRADA,
"<leitura> <estado>"), --reproduzir le uma entrada gravada; os quadros do --log ("quadro N: ... hash H") e o save
H_HUNTER.001 sao funcoes deterministicas dos saves de inicio e da entrada ate o VBlank (leitura = VBlank). Estados
especiais: SAVE grava o save; CREDITOS escreve no stderr o fim natural do SCENE84. FAKE_FIM = ultimo VBlank da sessao
gravada; FAKE_QUEDA = VBlank em que a sessao cai (rc 3); FAKE_DIVERGE = VBlank a partir do qual a reproducao diverge.
"""
import hashlib
import os
import sys


def opt(nome, padrao=None):
    a = sys.argv[1:]
    return a[a.index(nome) + 1] if nome in a else padrao


def digest_saves(pasta):
    h = hashlib.sha256()
    for f in sorted(os.listdir(pasta)):
        with open(os.path.join(pasta, f), 'rb') as g:
            h.update(f.encode() + g.read())
    return h.hexdigest()


saves = opt('--saves-pasta')
log = opt('--log')
cada = int(opt('--hash-every', '60'))
os.makedirs(saves, exist_ok=True)
base = digest_saves(saves)
if '--gravar' in sys.argv:
    entrada = [l.split(' ', 1) for l in os.environ.get('FAKE_ENTRADA', '').split(';') if l]
    entrada = [(int(v), s) for v, s in entrada]
    with open(opt('--gravar'), 'x', encoding='ascii') as f:  # 'x': recusa arquivo existente, como o --gravar
        f.write('# gravacao falsa\n')
        for v, s in entrada:
            f.write('%d %s\n' % (v, s))
    fim = int(os.environ.get('FAKE_FIM', '600'))
    queda = int(os.environ.get('FAKE_QUEDA', '0')) or None
    diverge = None
else:
    with open(opt('--reproduzir'), encoding='ascii') as f:
        entrada = [(int(l.split()[0]), l.split()[1]) for l in f if l.strip() and not l.startswith('#')]
    fim = int(opt('--vblanks'))
    queda = None
    diverge = int(os.environ.get('FAKE_DIVERGE', '0')) or None
diverge_est = None if '--gravar' in sys.argv else (int(os.environ.get('FAKE_DIVERGE_ESTADO', '0')) or None)
estado = None if ('--gravar' in sys.argv and os.environ.get('FAKE_SEM_ESTADO')) else opt('--estado')
est_cada = int(opt('--estado-cada', '60'))

rc = 0
ef = open(estado, 'w', encoding='ascii') if estado else None
with open(log, 'w', encoding='ascii') as lf:
    for v in range(0, fim + 1):
        if queda is not None and v >= queda:
            rc = 3
            break
        feitos = [e for e in entrada if e[0] <= v]
        if any(e[0] == v and e[1] == 'SAVE' for e in entrada):
            with open(os.path.join(saves, 'H_HUNTER.001'), 'w', encoding='ascii') as g:
                g.write(hashlib.sha256((base + repr(feitos)).encode()).hexdigest())
                if diverge is not None and v >= diverge:
                    g.write('x')  # a reproducao divergente tambem salva outra coisa
        if any(e[0] == v and e[1] == 'CREDITOS' for e in entrada):
            sys.stderr.write('[video] MOVIES/SCENE84.4XM: fim natural no VBlank %d\n' % v)
            if os.environ.get('HH_FILE_LOG') and not os.environ.get('FAKE_SEM_GAMEEND'):  # o comando 229 "GameEnd"
                with open(os.environ['HH_FILE_LOG'], 'a', encoding='ascii') as g:
                    g.write('0\t%d\tnativo\tdado\tAHFDATA/GAMEEND.AHF\t0\t2048\t-\n' % (v + 1))
        if v % cada == 0:
            h = hashlib.sha256((base + repr(feitos) + str(v)).encode()).hexdigest()[:16]
            if diverge is not None and v >= diverge:
                h = '0' * 16
            lf.write('quadro %d: 640x480 hash %s\n' % (v, h))
        if ef and v % est_cada == 0:  # a leitura do controle e o proprio VBlank no programa falso
            e = hashlib.sha256((base + repr(feitos) + 'estado' + str(v)).encode()).hexdigest()[:16]
            if diverge_est is not None and v >= diverge_est:
                e = 'f' * 16
            ef.write('estado %d %s\n' % (v, e))
if ef:
    ef.close()
sys.exit(rc)
