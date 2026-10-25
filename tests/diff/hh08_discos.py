#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
"""HH08.5/HH08.6: os dois discos numa instalacao so (sem troca de disco) e as falhas seguras.

Positivo:
  - discos juntos (hh-extract --juntar-discos): o roteiro newgame_area1 roda com o provedor
    nativo, os 94 hashes de quadro sao os da referencia do HH06, nenhum setor de dado passa pelo
    GD-ROM HLE, o jogo identifica o disco 1 pelo DISC/DISK1.ID e nada pede troca de disco.
  (A passagem para o disco 2 pela variavel do jogo e coberta pelo unitario
  hh08_gdfs_merged_discs_follow_game_variable; um roteiro ate la depende de saves, HH11/HH16.)
Negativos (mensagem segura, rc 1, sem travar):
  - pasta de dados so com o disco 2; disco 2 junto de outra versao; versao falsificada no
    manifesto (o boot confere o hash do executavel); dados ausentes;
  - arquivo alterado (1 byte), truncado e ilegivel; mod que nao substitui nenhum recurso.
Mods (P5): recurso substituido por um arquivo maior que o original e aplicado.

uso: hh08_discos.py <hh-run.exe> <hh-extract.exe> <raiz do projeto>
ambiente: HH_DATA_DIR (disco 1) e HH_DATA_DIR2 (disco 2); HH_AREA1_HASHES (opcional). Sem os
dados = SKIP (rc 77). Os casos usam arvores temporarias de hardlinks (sem copiar os dados).
"""
import os
import shutil
import subprocess
import sys
import tempfile

RUN, EXTRACT, ROOT = (os.path.abspath(a) for a in sys.argv[1:4])
D1 = os.environ.get('HH_DATA_DIR', '')
D2 = os.environ.get('HH_DATA_DIR2', '')
REF = os.environ.get('HH_AREA1_HASHES', '')
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


def run(args, env=None, timeout=1500):
    p = subprocess.run([RUN] + args, capture_output=True, text=True, env=dict(os.environ, **(env or {})),
                       timeout=timeout)
    return p.returncode, p.stdout + p.stderr


def last(out):
    return out.strip().splitlines()[-1][:180] if out.strip() else ''


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


def merged(name):
    """Instalacao com os dois discos juntos (hardlinks), como o Builder monta."""
    dst = os.path.join(tmp, name)
    linktree(D1, dst)
    p = subprocess.run([EXTRACT, '--juntar-discos', dst, D2, '--links'], capture_output=True, text=True)
    if p.returncode != 0:
        raise RuntimeError('juncao falhou: ' + p.stdout + p.stderr)
    return dst


try:
    inst = merged('data')
    check('juncao: arquivos.manifest e variante do disco 2',
          os.path.isfile(os.path.join(inst, 'arquivos.manifest')) and
          os.path.isfile(os.path.join(inst, 'disc', '_DISCO2', 'MOVIES', 'SCENE0.4XM')))

    # roteiro completo nos discos juntos
    log = os.path.join(tmp, 'arquivos.tsv')
    trace = os.path.join(tmp, 'open.tsv')  # gdFsOpen: o DISKn.ID so e aberto e fechado (sem leitura)
    rc, out = run(['--data', inst, '--vblanks', '4700', '--rtc', '1700000000', '--every', '50'],
                  {'HH_INPUT_SCRIPT': SCRIPT, 'HH_FILE_LOG': log, 'HH_CALL_TRACE': trace + ';8C15D09A'})
    hashes = [l.split()[4] for l in out.splitlines() if l.startswith('quadro ')]
    check('discos juntos: roteiro termina pelo limite', rc == 0, 'rc=%d' % rc)
    check('discos juntos: 94 amostras de quadro', len(hashes) == 94, '%d' % len(hashes))
    if os.path.isfile(REF):
        with open(REF) as f:
            ref = [l.split()[1] for l in f if l.strip()]
        check('discos juntos: hashes iguais a referencia do HH06', hashes == ref)
    rows = [l.rstrip('\n').split('\t') for l in open(log, encoding='utf-8')][1:] if os.path.isfile(log) else []
    gd = sum(1 for r in rows if r[2] == 'gdrom' and r[3] == 'dado')
    check('discos juntos: 0 setores de dado pelo GD-ROM HLE', gd == 0 and len(rows) > 1000, 'gdrom %d' % gd)
    opens = [l.rstrip('\n').split('\t') for l in open(trace, encoding='utf-8')][1:] if os.path.isfile(trace) else []
    ids = [(o[10], o[7]) for o in opens if '.id' in o[10].lower()]  # (strings, r0 = GDFS ou 0)
    check('discos juntos: o jogo identifica o disco 1 (DISC/DISK1.ID aberto, sem pedir o 2)',
          len(ids) == 1 and 'disk1.id' in ids[0][0].lower() and ids[0][1] != '00000000' and 'disco 1' in out,
          str(ids))
    check('discos juntos: nada de troca de disco', 'tampa' not in out and 'disco 2 ausente' not in out)

    # negativos de identidade
    rc, out = run(['--data', D2, '--vblanks', '10'])
    check('negativo: pasta so com o disco 2', rc == 1 and "'hedhunter_v1006_b', esperado o disco 1" in out, last(out))
    other = merged('outra')
    replace_copy(os.path.join(other, 'arquivos.manifest'),
                 lambda d: d.replace(b'disco2 hedhunter_v1006_b', b'disco2 hedhunter_v9999_x'))
    rc, out = run(['--data', other, '--vblanks', '10'])
    check('negativo: disco 2 junto de outra versao', rc == 1 and 'disco 2 junto' in out, last(out))
    spoof = os.path.join(tmp, 'falso')
    linktree(D2, spoof)
    replace_copy(os.path.join(spoof, 'disc.manifest'),
                 lambda d: d.replace(b'version hedhunter_v1006_b', b'version hedhunter_v1006_a'))
    rc, out = run(['--data', spoof, '--vblanks', '10'])
    check('negativo: versao falsificada no manifesto', rc == 1 and 'dados de outro disco' in out and
          'hedhunter_v1006_b' in out, last(out))
    rc, out = run(['--data', os.path.join(tmp, 'nada'), '--vblanks', '10'])
    check('negativo: dados ausentes', rc == 1 and 'dados do jogo ausentes' in out, last(out))
    # instalacao junta incompleta (revisao Codex HH08): a variante ou o DISK2.ID ausentes param no
    # boot, nunca servem o arquivo do disco 1 em silencio
    os.remove(os.path.join(other, 'arquivos.manifest'))
    shutil.copy(os.path.join(inst, 'arquivos.manifest'), os.path.join(other, 'arquivos.manifest'))
    os.remove(os.path.join(other, 'disc', '_DISCO2', 'MOVIES', 'SCENE0.4XM'))
    rc, out = run(['--data', other, '--vblanks', '10'])
    check('negativo: variante do disco 2 ausente', rc == 1 and 'ausente ou ilegivel: _DISCO2/MOVIES/SCENE0.4XM' in out,
          last(out))
    incomp = merged('incompleta')
    os.remove(os.path.join(incomp, 'disc', 'DISC', 'DISK2.ID'))
    rc, out = run(['--data', incomp, '--vblanks', '10'])
    check('negativo: DISK2.ID ausente', rc == 1 and 'ausente ou ilegivel: DISC/DISK2.ID' in out, last(out))

    # mods (P5)
    mods = os.path.join(tmp, 'mods')
    csv = os.path.join(D1, 'disc', 'HEADHUNTER', 'LOCALE', 'STRING_COLLECTION.CSV')
    os.makedirs(os.path.join(mods, 'HEADHUNTER', 'LOCALE'))
    with open(csv, 'rb') as f, open(os.path.join(mods, 'HEADHUNTER', 'LOCALE', 'string_collection.csv'), 'wb') as g:
        g.write(f.read() + b'\r\n' * 4096)  # mesmo conteudo + linhas vazias: maior que o original
    rc, out = run(['--data', inst, '--mods', mods, '--vblanks', '300', '--rtc', '1700000000'])
    check('mod maior que o original aplicado',
          rc == 0 and '[mods] recurso substituido: HEADHUNTER/LOCALE/STRING_COLLECTION.CSV' in out, 'rc=%d' % rc)
    with open(os.path.join(mods, 'NADA.BIN'), 'wb') as g:
        g.write(b'x')
    rc, out = run(['--data', inst, '--mods', mods, '--vblanks', '10'])
    check('negativo: mod que nao substitui nada', rc == 1 and 'NADA.BIN: nao substitui nenhum recurso' in out, last(out))

    # arquivo ilegivel (acesso exclusivo por outro handle): parada com mensagem
    import ctypes
    from ctypes import wintypes
    k32 = ctypes.WinDLL('kernel32', use_last_error=True)
    k32.CreateFileW.restype = wintypes.HANDLE
    k32.CreateFileW.argtypes = [wintypes.LPCWSTR, wintypes.DWORD, wintypes.DWORD, wintypes.LPVOID, wintypes.DWORD,
                                wintypes.DWORD, wintypes.HANDLE]
    alvo = os.path.join(inst, 'disc', 'AUDIO64.DRV')
    replace_copy(alvo, lambda d: d)  # copia propria: a trava nao alcanca os dados originais
    lock = k32.CreateFileW(alvo, 0x80000000, 0, None, 3, 0x80, None)
    check('trava exclusiva do arquivo', lock not in (None, wintypes.HANDLE(-1).value))
    try:
        rc, out = run(['--data', inst, '--vblanks', '300', '--rtc', '1700000000'])
    finally:
        k32.CloseHandle(lock)
    # pego ja na conferencia de presenca do boot ("ausente ou ilegivel"); depois do boot, o
    # adaptador para com "arquivo do jogo ilegivel" (unitario do adaptador nao cobre: trava real)
    check('negativo: arquivo ilegivel', rc == 1 and 'ilegivel' in out and 'AUDIO64.DRV' in out, last(out))

    # arquivo alterado (1 byte) e truncado
    def flip(d):
        d[len(d) // 2] ^= 0x5A
        return d

    replace_copy(alvo, flip)
    rc, out = run(['--data', inst, '--vblanks', '300', '--rtc', '1700000000'])
    check('negativo: arquivo alterado', rc == 1 and 'alterado ou corrompido: AUDIO64.DRV' in out and
          'conteudo diferente do disco' in out, last(out))
    replace_copy(alvo, lambda d: d[:-100])
    rc, out = run(['--data', inst, '--vblanks', '300', '--rtc', '1700000000'])
    # o tamanho ja e conferido no boot (dados de execucao, runtime_data.cpp): falha antes, com o nome
    check('negativo: arquivo truncado', rc == 1 and 'AUDIO64.DRV' in out and 'Builder' in out, last(out))
finally:
    shutil.rmtree(tmp, ignore_errors=True)  # so hardlinks e copias de arquivos do jogo

print('%d falhas' % fails)
sys.exit(1 if fails else 0)
