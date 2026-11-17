# SPDX-License-Identifier: MIT
"""Programa falso para os testes do monitor de campanha (tools/dev/campanha.py, HH_CAMPANHA_PROGRAMA).

Imita o contrato do HeadHunter.exe usado pelo monitor, sem jogo: --gravar grava a entrada (a lista de FAKE_ENTRADA,
"<leitura> <estado>"), --reproduzir le uma entrada gravada; os quadros do --log ("quadro N: ... hash H") e o save
H_HUNTER.001 sao funcoes deterministicas dos saves de inicio e da entrada ate o VBlank (leitura = VBlank). Estados
especiais: SAVE grava o save; CREDITOS escreve no stderr o fim natural do SCENE84. FAKE_FIM = ultimo VBlank da sessao
gravada; FAKE_QUEDA = VBlank em que a sessao cai (rc 3); FAKE_DIVERGE = VBlank a partir do qual a reproducao diverge;
FAKE_PARCIAL = texto final da gravacao sem fim de linha (queda no meio da escrita); FAKE_DORME = segundos com a "janela"
aberta no fim da gravacao (cria aberto.flag na pasta da sessao). A leitura da gravacao falha numa linha incompleta, como
o roteiro do produto. Como o --gravar, a gravacao fechada termina com "# fim da sessao, ultima leitura N" (FAKE_SEM_FIM
tira) e a reproducao para na leitura seguinte (FAKE_SEM_PARADA ignora); FAKE_ADIANTE defasa a leitura do VBlank, como no
jogo, para a sessao terminar fora da grade dos hashes. FAKE_JANELA abre uma janela (tkinter) no fim da gravacao e so sai
quando ela e fechada de fora (o "Parar gravacao" do HeadHunter-Gravador).
"""
import hashlib
import os
import sys


def opt(nome, padrao=None):
    a = sys.argv[1:]
    return a[a.index(nome) + 1] if nome in a else padrao


def digest_saves(pasta):
    h = hashlib.sha256()
    for raiz, _, arqs in sorted(os.walk(pasta)):  # o jogo grava em subpastas (card0)
        for f in sorted(arqs):
            with open(os.path.join(raiz, f), 'rb') as g:
                h.update(os.path.relpath(os.path.join(raiz, f), pasta).encode() + g.read())
    return h.hexdigest()


if os.environ.get('FAKE_ENV_SAIDA') and '--gravar' in sys.argv:  # que variaveis HH_ chegaram ao "jogo"
    with open(os.environ['FAKE_ENV_SAIDA'], 'w') as f:
        f.write('\n'.join(sorted(k for k in os.environ if k.upper().startswith('HH_'))))
if '--reproduzir' in sys.argv and os.environ.get('HH_INPUT_SCRIPT'):  # como o produto: recusa os dois juntos
    sys.stderr.write('--gravar/--reproduzir com HH_INPUT_SCRIPT\n')
    sys.exit(2)
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
        if os.environ.get('FAKE_PARCIAL'):  # a queda no meio da escrita de uma linha
            f.write(os.environ['FAKE_PARCIAL'])
    fim = int(os.environ.get('FAKE_FIM', '600'))
    queda = int(os.environ.get('FAKE_QUEDA', '0')) or None
    diverge = None
else:
    with open(opt('--reproduzir'), encoding='ascii') as f:
        entrada = [(int(l.split()[0]), l.split()[1]) for l in f if l.strip() and not l.startswith('#')]
    fim = int(opt('--vblanks'))
    queda = None
    # o --reproduzir do produto para na leitura seguinte a do comentario de fim da gravacao
    fim_leitura = None
    with open(opt('--reproduzir'), encoding='ascii') as f:
        for l in f:
            if l.startswith('# fim da sessao, ultima leitura ') and not os.environ.get('FAKE_SEM_PARADA'):
                fim_leitura = int(l.split()[-1])
    diverge = int(os.environ.get('FAKE_DIVERGE', '0')) or None
diverge_est = None if '--gravar' in sys.argv else (int(os.environ.get('FAKE_DIVERGE_ESTADO', '0')) or None)
estado = None if ('--gravar' in sys.argv and os.environ.get('FAKE_SEM_ESTADO')) else opt('--estado')
est_cada = int(opt('--estado-cada', '60'))

adiante = int(os.environ.get('FAKE_ADIANTE', '0'))  # leitura = VBlank + adiante (no jogo elas se defasam)
if '--gravar' in sys.argv:
    fim_leitura = None
rc = 0
ef = open(estado, 'w', encoding='ascii') if estado else None
with open(log, 'w', encoding='ascii') as lf:
    for v in range(0, fim + 1):
        leitura = v + adiante
        if queda is not None and v >= queda:
            rc = 3
            break
        if fim_leitura is not None and leitura > fim_leitura:
            print('fim da gravacao reproduzido: leitura %d, VBlank %d' % (fim_leitura, v))
            break
        feitos = [e for e in entrada if e[0] <= v]
        if any(e[0] == v and e[1] == 'SAVE' for e in entrada):
            os.makedirs(os.path.join(saves, 'card0'), exist_ok=True)  # como o jogo: saves/card0/...
            with open(os.path.join(saves, 'card0', 'H_HUNTER.001'), 'w', encoding='ascii') as g:
                g.write(hashlib.sha256((base + repr(feitos)).encode()).hexdigest())
                if diverge is not None and v >= diverge:
                    g.write('x')  # a reproducao divergente tambem salva outra coisa
        if any(e[0] == v and e[1] == 'CREDITOS' for e in entrada):
            sys.stderr.write('[video] MOVIES/SCENE84.4XM: fim natural no VBlank %d\n' % v)
            if os.environ.get('HH_FILE_LOG') and not os.environ.get('FAKE_SEM_GAMEEND'):  # o comando 229 "GameEnd"
                with open(os.environ['HH_FILE_LOG'], 'a', encoding='ascii') as g:
                    g.write('0\t%d\tnativo\tdado\tAHFDATA/GAMEEND.AHF\t0\t2048\t-\n' % (v + 1))
        if v % cada == 0 and not ('--gravar' in sys.argv and os.environ.get('FAKE_SEM_QUADROS')):
            h = hashlib.sha256((base + repr(feitos) + str(v)).encode()).hexdigest()[:16]
            if diverge is not None and v >= diverge:
                h = '0' * 16
            if '--gravar' in sys.argv and os.environ.get('FAKE_JANELA_VK') and v > 0:  # o Vulkan da janela != software
                h = hashlib.sha256(('vk' + h).encode()).hexdigest()[:16]
            dq = int(os.environ.get('FAKE_DIVERGE_QUADROS', '0')) if '--reproduzir' in sys.argv else 0
            if dq and v >= dq:  # so a imagem diverge (o estado e os saves nao)
                h = '1' * 16
            lf.write('quadro %d: 640x480 hash %s\n' % (v, h))
        if ef and leitura % est_cada == 0:
            e = hashlib.sha256((base + repr(feitos) + 'estado' + str(leitura)).encode()).hexdigest()[:16]
            if diverge_est is not None and leitura >= diverge_est:
                e = 'f' * 16
            ef.write('estado %d %s\n' % (leitura, e))
if ef:
    ef.close()
if '--gravar' in sys.argv and rc and os.environ.get('FAKE_PARCIAL_LOG'):  # a queda corta os logs no meio
    for p_ in (log, estado):
        with open(p_, 'a', encoding='ascii') as f:
            f.write('estado 9' if p_ == estado else 'quadro 9')
if '--gravar' in sys.argv and rc == 0 and not os.environ.get('FAKE_SEM_FIM'):  # fecha como o --gravar
    with open(opt('--gravar'), 'a', encoding='ascii') as f:
        f.write('# fim da sessao, ultima leitura %d\n' % (fim + adiante))
if '--gravar' in sys.argv and os.environ.get('FAKE_DORME'):  # a janela fica aberta (teste de segunda instancia/queda)
    import time
    open(os.path.join(os.path.dirname(opt('--gravar')), 'aberto.flag'), 'w').close()
    time.sleep(float(os.environ['FAKE_DORME']))
if '--gravar' in sys.argv and os.environ.get('FAKE_JANELA'):  # uma janela de verdade, fechada de fora (o gravador)
    import tkinter as tk
    j = tk.Tk()
    j.title('jogo falso')
    open(os.path.join(os.path.dirname(opt('--gravar')), 'aberto.flag'), 'w').close()
    j.mainloop()  # o WM_CLOSE fecha a janela, como o X
    with open(os.path.join(os.path.dirname(opt('--gravar')), 'janela_fechada.flag'), 'w') as f:
        f.write('ok')
sys.exit(rc)
