#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
"""HH08.5/HH08.6: os dois discos com o provedor nativo e as falhas seguras.

Positivos:
  - disco 2 (data/disc2): boot ate quadros nao pretos e 0 setores de dado pelo GD-ROM HLE (a
    checagem do DISC/DISKn.ID so roda depois do menu; no disco 1 ela e vista no roteiro);
  - troca: com a tampa logica aberta durante o jogo no disco 1, o jogo sai pelo proprio caminho
    (0x8C04E754 -> 0x8C04E470) logo depois, com o codigo do BIOS.
Negativos (mensagem segura, rc 1, sem travar):
  - pasta do disco 2 com o disco 1 (disco trocado);
  - disco 2 ausente;
  - arquivo do disco alterado (1 byte) e arquivo truncado: o provedor recusa na abertura.
Mods (P5): recurso substituido por um arquivo maior que o original e aplicado; mod que nao
substitui nenhum recurso e erro com o caminho.

uso: hh08_discos.py <hh-run.exe> <raiz do projeto>
ambiente: HH_DATA_DIR (disco 1) e HH_DATA_DIR2 (disco 2); sem eles = SKIP (rc 77).
Os casos negativos usam uma arvore temporaria de hardlinks (sem copiar os dados).
"""
import os
import shutil
import subprocess
import sys
import tempfile

RUN, ROOT = (os.path.abspath(a) for a in sys.argv[1:3])
D1 = os.environ.get('HH_DATA_DIR', '')
D2 = os.environ.get('HH_DATA_DIR2', '')
SCRIPT = os.path.join(ROOT, 'tools', 'dist', 'roteiros', 'newgame_area1.txt')
if not (os.path.isfile(os.path.join(D1, 'disc.manifest')) and os.path.isfile(os.path.join(D2, 'disc.manifest'))):
    print('SKIP: defina HH_DATA_DIR e HH_DATA_DIR2')
    sys.exit(77)

fails = 0
tmp = tempfile.mkdtemp(prefix='hh08_disc_', dir=os.path.dirname(os.path.abspath(D1)))


def check(name, cond, info=''):
    global fails
    print('[%s] %s%s' % (' ok ' if cond else 'FAIL', name, (' (%s)' % info) if info else ''))
    fails += not cond


def run(args, env=None, timeout=1200):
    p = subprocess.run([RUN] + args, capture_output=True, text=True, env=dict(os.environ, **(env or {})),
                       timeout=timeout)
    return p.returncode, p.stdout + p.stderr


def linktree(src, dst):
    for d, _, fs in os.walk(src):
        rel = os.path.relpath(d, src)
        os.makedirs(os.path.join(dst, rel), exist_ok=True)
        for f in fs:
            os.link(os.path.join(d, f), os.path.join(dst, rel, f))


def replace_copy(path, mutate):
    """Troca o hardlink por uma copia propria (o original fica intacto) e aplica `mutate`."""
    with open(path, 'rb') as f:
        data = bytearray(f.read())
    os.remove(path)
    with open(path, 'wb') as f:
        f.write(mutate(data))


try:
    # disco 2: boot nativo, sem setores de dado
    log = os.path.join(tmp, 'd2.tsv')
    rc, out = run(['--data', D2, '--disco', '2', '--vblanks', '1500', '--rtc', '1700000000', '--every', '500'], {'HH_FILE_LOG': log})
    check('disco 2: termina pelo limite', rc == 0, 'rc=%d' % rc)
    check('disco 2: quadros nao pretos', 'quadros nao pretos: 0 ' not in out)
    rows = [l.rstrip('\n').split('\t') for l in open(log, encoding='utf-8')][1:] if os.path.isfile(log) else []
    gd = sum(1 for r in rows if r[2] == 'gdrom' and r[3] == 'dado')
    nat = sum(1 for r in rows if r[2] == 'nativo' and r[3] == 'dado')
    check('disco 2: 0 setores de dado pelo GD-ROM HLE, leituras pelo provedor nativo', gd == 0 and nat > 100,
          'gdrom %d, nativo %d' % (gd, nat))

    # troca: tampa aberta no VBlank 3000 (area 1 do disco 1, roteiro newgame_area1)
    rc, out = run(['--data', D1, '--vblanks', '4700', '--rtc', '1700000000', '--every', '500', '--tampa-vblank', '3000'],
                  {'HH_INPUT_SCRIPT': SCRIPT})
    vb = next((l for l in out.splitlines() if l.startswith('vblanks: ')), '')
    check('troca: tampa aberta registrada', '[disco] tampa aberta no VBlank 3000' in out)
    check('troca: o jogo sai pelo proprio caminho (codigo do BIOS)', rc == 3 and 'pediu para sair (codigo 00000001)' in out,
          'rc=%d' % rc)
    check('troca: logo depois da tampa', vb.startswith('vblanks: 30') and int(vb.split()[1]) <= 3060, vb)

    # instalacao com dois discos: data/disc1, data/disc2 (hardlinks)
    inst = os.path.join(tmp, 'data')
    linktree(D1, os.path.join(inst, 'disc1'))
    linktree(D1, os.path.join(inst, 'disc2'))  # disco trocado: a pasta do 2 tem o 1
    rc, out = run(['--data', inst, '--disco', '2', '--vblanks', '10'])
    check('negativo: disco trocado', rc == 1 and 'nao tem o disco 2' in out and '(e o disco 1)' in out,
          out.strip().splitlines()[-1] if out.strip() else '')
    # so a linha "version" do manifesto falsificada: o boot confere pelo hash do executavel
    man = os.path.join(inst, 'disc2', 'disc.manifest')
    replace_copy(man, lambda d: d.replace(b'version hedhunter_v1006_a', b'version hedhunter_v1006_b'))
    rc, out = run(['--data', inst, '--disco', '2', '--vblanks', '10'])
    check('negativo: versao falsificada no manifesto', rc == 1 and 'disco trocado' in out and 'hedhunter_v1006_a' in out,
          out.strip().splitlines()[-1][:160] if out.strip() else '')
    shutil.rmtree(os.path.join(inst, 'disc2'))
    rc, out = run(['--data', inst, '--disco', '2', '--vblanks', '10'])
    check('negativo: disco 2 ausente', rc == 1 and 'dados do disco 2 ausentes' in out,
          out.strip().splitlines()[-1] if out.strip() else '')
    linktree(D2, os.path.join(inst, 'disc2'))
    rc, out = run(['--data', inst, '--disco', '2', '--vblanks', '10'])
    check('instalacao com os dois discos: disco 2 resolvido', rc == 0, 'rc=%d' % rc)

    # mods de recursos (P5): maior que o original e aplicado sem conferencia de hash; mod que nao
    # substitui nada e erro com o caminho
    mods = os.path.join(tmp, 'mods')
    csv = os.path.join(D1, 'disc', 'HEADHUNTER', 'LOCALE', 'STRING_COLLECTION.CSV')
    os.makedirs(os.path.join(mods, 'HEADHUNTER', 'LOCALE'))
    with open(csv, 'rb') as f, open(os.path.join(mods, 'HEADHUNTER', 'LOCALE', 'string_collection.csv'), 'wb') as g:
        g.write(f.read() + b'\r\n' * 4096)  # mesmo conteudo + linhas vazias: maior que o original
    rc, out = run(['--data', inst, '--disco', '1', '--mods', mods, '--vblanks', '300', '--rtc', '1700000000'])
    check('mod maior que o original aplicado', rc == 0 and '[mods] recurso substituido: HEADHUNTER/LOCALE/STRING_COLLECTION.CSV' in out,
          'rc=%d' % rc)
    with open(os.path.join(mods, 'NADA.BIN'), 'wb') as g:
        g.write(b'x')
    rc, out = run(['--data', inst, '--disco', '1', '--mods', mods, '--vblanks', '10'])
    check('negativo: mod que nao substitui nada', rc == 1 and 'NADA.BIN: nao substitui nenhum recurso' in out,
          out.strip().splitlines()[-1][:160] if out.strip() else '')

    # arquivo ilegivel (aberto com acesso exclusivo por outro handle): parada com mensagem, nao
    # "arquivo inexistente" (revisao Codex HH08)
    import ctypes
    from ctypes import wintypes
    k32 = ctypes.WinDLL('kernel32', use_last_error=True)
    k32.CreateFileW.restype = wintypes.HANDLE
    k32.CreateFileW.argtypes = [wintypes.LPCWSTR, wintypes.DWORD, wintypes.DWORD, wintypes.LPVOID, wintypes.DWORD,
                                wintypes.DWORD, wintypes.HANDLE]
    travado = os.path.join(inst, 'disc1', 'disc', 'AUDIO64.DRV')
    replace_copy(travado, lambda d: d)  # copia propria: a trava nao alcanca os dados originais
    lock = k32.CreateFileW(travado, 0x80000000, 0, None, 3, 0x80, None)
    check('trava exclusiva do arquivo', lock not in (None, wintypes.HANDLE(-1).value))
    try:
        rc, out = run(['--data', inst, '--disco', '1', '--vblanks', '300', '--rtc', '1700000000'])
    finally:
        k32.CloseHandle(lock)
    check('negativo: arquivo ilegivel', rc == 1 and 'arquivo do jogo ilegivel' in out and 'AUDIO64.DRV' in out,
          out.strip().splitlines()[-1][:160] if out.strip() else '')

    # arquivo alterado (1 byte) e truncado: recusados na abertura, com o nome
    alvo = os.path.join(inst, 'disc1', 'disc', 'AUDIO64.DRV')

    def flip(d):
        d[len(d) // 2] ^= 0x5A
        return d

    replace_copy(alvo, flip)
    rc, out = run(['--data', inst, '--disco', '1', '--vblanks', '300', '--rtc', '1700000000'])
    check('negativo: arquivo alterado', rc == 1 and 'alterado ou corrompido: AUDIO64.DRV' in out and
          'conteudo diferente do disco' in out, out.strip().splitlines()[-1][:160] if out.strip() else '')
    replace_copy(alvo, lambda d: d[:-100])
    rc, out = run(['--data', inst, '--disco', '1', '--vblanks', '300', '--rtc', '1700000000'])
    # o tamanho ja e conferido no boot (dados de execucao, runtime_data.cpp): falha antes, com o nome
    check('negativo: arquivo truncado', rc == 1 and 'AUDIO64.DRV' in out and 'Builder' in out,
          out.strip().splitlines()[-1][:160] if out.strip() else '')
finally:
    shutil.rmtree(tmp, ignore_errors=True)  # so hardlinks e copias de arquivos do jogo

print('%d falhas' % fails)
sys.exit(1 if fails else 0)
