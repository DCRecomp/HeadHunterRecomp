#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
"""HH11: portoes dos saves nativos (API bu 1.47 sobre arquivos) no roteiro newgame_area1 (4 700 VBlanks).

O oraculo e o jogo original no Flycast com VMU (tools/dev/flycast_oracle.py bu-trace; docs/planos/HH11-PLANO.md):
c1 = VMU vazia, c2 = VMU com o H_HUNTER.SYS que o c1 gravou. Os roteiros de entrada sao os mesmos menus.

S0  estatico: no gerado compilado, as entradas da bu chamadas pelo jogo passam pelo slot (HHG_SLOT), nenhuma direta;
S1  sessao 1 (cartao vazio): as chamadas do jogo (mount, exists, SAVE) iguais ao c1, SAVE ok, nada sem suporte; a
    imagem gravada igual a do VMU do oraculo no trecho coberto pelo CRC; a data do arquivo e a do relogio do jogo;
S2  sessao 2 (o cartao da sessao 1): chamadas iguais ao c2 (mount, exists, file_info, leitura do arquivo inteiro,
    SAVE), LOAD ok, nada sem suporte;
S3  determinismo: a sessao 2 repetida sobre outra copia do cartao da sessao 1 da os mesmos quadros e o mesmo arquivo;
S5  fechamento (HH_PAD_FECHAMENTO): no nativo, 0 entradas nas funcoes da bu geradas (API, servidor e as internas
    que elas alcancam); no legado (sem VMU) o init original roda (referencia);
S2  tambem: o que o LOAD deixou na RAM do jogo (relido do guest pelo adaptador) e o arquivo do cartao;
Negativos (terminam normalmente e o portao certo acusa): sem_data -> S1 (data), save_corrompe -> S1 (imagem),
load_falha -> S2 (chamadas), load_altera -> S2 (bytes entregues; as contagens ficam iguais).

uso: hh11_saves.py <hh-run.exe> <raiz do projeto> [<pasta do gerado>]
ambiente: HH_DATA_DIR (dados privados; sem ele = SKIP, rc 77); HH11_ORACULO (padrao analysis/hh11-oraculo; obrigatorio
com os dados).
"""
import os
import re
import shutil
import struct
import subprocess
import sys
import tempfile
from concurrent.futures import ThreadPoolExecutor

RUN, ROOT = (os.path.abspath(a) for a in sys.argv[1:3])
GEN = os.path.abspath(sys.argv[3]) if len(sys.argv) > 3 else ''
DATA = os.environ.get('HH_DATA_DIR', '')
ORA = os.environ.get('HH11_ORACULO', os.path.join(ROOT, 'analysis', 'hh11-oraculo'))
SCRIPT = os.path.join(ROOT, 'tools', 'dist', 'roteiros', 'newgame_area1.txt')
VBLANKS, RTC = 4700, 1700000000
if not os.path.isfile(os.path.join(DATA, 'disc.manifest')):
    print('SKIP: defina HH_DATA_DIR')
    sys.exit(77)

# entradas da bu chamadas pelo jogo (o servidor 8C15FC9C e chamado pela tabela da camada per, nao por chamada direta)
BU_CHAMADAS = ('8C15FB44', '8C15FC2A', '8C15FDCC', '8C15FDD2', '8C15FF54', '8C16037C', '8C1603AA', '8C1603DC',
               '8C160500', '8C160558', '8C160614', '8C160754', '8C160774')
# fechamento: API, servidor e internas alcancadas (verificacao, ultimo erro, disk_info, exists, file_info)
BU_FECHAMENTO = BU_CHAMADAS + ('8C15FC9C', '8C171448', '8C15FE66', '8C1721CA', '8C172EEE', '8C171CD8')

tmp = tempfile.mkdtemp(prefix='hh11_sv_')
fails = 0


def check(name, cond, info=''):
    global fails
    print('[%s] %s%s' % (' ok ' if cond else 'FAIL', name, (' (%s)' % info) if info else ''))
    fails += not cond


def oracle_counts(tsv):
    c = {}
    for line in open(tsv, encoding='utf-8'):
        f = line.rstrip('\n').split('\t')
        if len(f) > 4 and f[3] == 'entrada':
            c[f[4]] = c.get(f[4], 0) + 1
    # na leitura do arquivo inteiro (op 0x0B) o adaptador conta um LOAD
    return {'mount': c.get('mount', 0), 'exists': c.get('exists', 0), 'file_info': c.get('file_info', 0),
            'save': c.get('save', 0), 'load': c.get('op_0b', 0) + c.get('load', 0)}


def run(tag, card, saves='nativo', mutation=None):
    closure = os.path.join(tmp, tag + '_fech.tsv')
    env = dict(os.environ, HH_INPUT_SCRIPT=SCRIPT, HH_PAD_FECHAMENTO=closure)
    env.pop('HH_BU_MUTACAO', None)
    if mutation:
        env['HH_BU_MUTACAO'] = mutation
    cmd = [RUN, '--data', DATA, '--vblanks', str(VBLANKS), '--rtc', str(RTC), '--every', '50', '--saves', saves]
    if saves == 'nativo':
        cmd += ['--saves-pasta', card]
    p = subprocess.run(cmd, capture_output=True, text=True, env=env)
    rd = lambda f: open(f, encoding='utf-8').read().splitlines() if os.path.isfile(f) else []
    m = re.search(r'\[saves\] nativo: init (\d+), passadas \d+, mount (\d+), exists (\d+), file_info (\d+), SAVE (\d+) '
                  r'\(ok (\d+), falha (\d+)\), LOAD (\d+) \(ok (\d+), falha (\d+)\), status ocupado \d+, sem suporte (\d+)',
                  p.stderr)
    stats = dict(zip(('init', 'mount', 'exists', 'file_info', 'save', 'save_ok', 'save_falha', 'load', 'load_ok',
                      'load_falha', 'sem_suporte'), map(int, m.groups()))) if m else {}
    return {'rc': p.returncode, 'stats': stats, 'hashes': re.findall(r'quadro \d+: \S+ hash ([0-9a-f]+)', p.stdout),
            'fech': dict(l.split('\t', 1) for l in rd(closure) if '\t' in l), 'err': p.stderr}


def envelope(path):
    """Imagem e data do envelope .hhsv (native/platform/saves_format.cpp)."""
    e = open(path, 'rb').read()
    if e[:4] != b'HHSV':
        return None, None
    hdr, n = struct.unpack_from('<H', e, 6)[0], struct.unpack_from('<I', e, 8)[0]
    return e[hdr:hdr + n], struct.unpack_from('<q', e, 16)[0]


def fnv64(b):
    h = 1469598103934665603
    for x in b:
        h = ((h ^ x) * 1099511628211) & 0xFFFFFFFFFFFFFFFF
    return '%016x' % h


def calls(st):
    return {k: st.get(k) for k in ('mount', 'exists', 'file_info', 'save', 'load')}


try:
    # S0: estatico
    direct, slots = 0, {a: 0 for a in BU_CHAMADAS}
    for fname in (sorted(os.listdir(GEN)) if GEN and os.path.isdir(GEN) else []):
        if fname.startswith('funcs_') and fname.endswith('.cpp'):
            text = open(os.path.join(GEN, fname), encoding='utf-8', errors='replace').read()
            for a in BU_CHAMADAS:
                slots[a] += text.count('HHG_SLOT(%s)' % a)
                direct += len(re.findall(r'HHG_FN\(%s\)|\bf_%s\s*\(\s*ctx' % (a, a), text))
    check('S0 estatico: chamadas do jogo a bu so pelo slot no gerado compilado',
          all(slots.values()) and direct == 0, '%s: %d por slot, %d diretas, sem slot: %s' %
          (GEN or 'sem pasta do gerado', sum(slots.values()), direct, [a for a, n in slots.items() if not n]))
    oc1, oc2 = (os.path.join(ORA, c + '.tsv') for c in ('c1', 'c2'))
    ovms = os.path.join(ORA, 'c1', 'vmu', 'H_HUNTER.SYS')
    have = all(os.path.isfile(f) for f in (oc1, oc2, ovms))
    check('oraculo do Flycast presente (c1, c2, VMS do c1)', have, ORA)
    if not have:
        raise SystemExit
    want1, want2 = oracle_counts(oc1), oracle_counts(oc2)
    ref_vms = open(ovms, 'rb').read()
    covered = struct.unpack_from('<I', ref_vms, 0x48)[0]

    card = lambda t: os.path.join(tmp, 'card_' + t)
    with ThreadPoolExecutor(4) as ex:
        j = {'s1': ex.submit(run, 's1', card('s1')),
             'semdata': ex.submit(run, 'semdata', card('semdata'), 'nativo', 'sem_data'),
             'corrompe': ex.submit(run, 'corrompe', card('corrompe'), 'nativo', 'save_corrompe'),
             'leg': ex.submit(run, 'leg', '', 'legado')}
        r = {k: v.result() for k, v in j.items()}
    S1 = r['s1']
    st1 = S1['stats']
    check('S1: sessao 1 termina pelo limite com o provedor nativo', S1['rc'] == 0 and st1.get('init') == 1,
          'rc %d' % S1['rc'])
    check('S1: chamadas do jogo iguais ao oraculo c1 (cartao vazio)', calls(st1) == want1,
          'nativo %s, oraculo %s' % (calls(st1), want1))
    check('S1: SAVE ok e nada sem suporte', st1.get('save_ok') == want1['save'] and st1.get('save_falha') == 0 and
          st1.get('sem_suporte') == 0, str(st1))
    f1 = os.path.join(card('s1'), 'card0', 'H_HUNTER.SYS.hhsv')
    img1, t1 = envelope(f1) if os.path.isfile(f1) else (None, None)
    check('S1: imagem gravada = VMU do oraculo no trecho coberto pelo CRC',
          img1 is not None and len(img1) == len(ref_vms) and img1[:covered] == ref_vms[:covered],
          '%s bytes, coberto %d' % (len(img1) if img1 else '-', covered))
    limit = RTC + VBLANKS // 59 + 60
    check('S1: data do arquivo = relogio do jogo (argumento de tempo do SAVE)', t1 is not None and RTC <= t1 <= limit,
          't %s, faixa [%d, %d]' % (t1, RTC, limit))
    # sessao 2 (e a repeticao e o negativo) sobre copias do cartao da sessao 1
    for t in ('s2', 's2b', 'loadfalha', 'loadaltera'):
        if os.path.isdir(card('s1')):
            shutil.copytree(card('s1'), card(t))
    with ThreadPoolExecutor(4) as ex:
        j = {'s2': ex.submit(run, 's2', card('s2')), 's2b': ex.submit(run, 's2b', card('s2b')),
             'loadfalha': ex.submit(run, 'loadfalha', card('loadfalha'), 'nativo', 'load_falha'),
             'loadaltera': ex.submit(run, 'loadaltera', card('loadaltera'), 'nativo', 'load_altera')}
        r.update({k: v.result() for k, v in j.items()})
    S2, S2b = r['s2'], r['s2b']
    st2 = S2['stats']
    check('S2: chamadas do jogo iguais ao oraculo c2 (cartao com o H_HUNTER.SYS)', S2['rc'] == 0 and calls(st2) == want2,
          'nativo %s, oraculo %s' % (calls(st2), want2))
    check('S2: LOAD do arquivo inteiro ok, SAVE ok e nada sem suporte',
          st2.get('load_ok') == want2['load'] and st2.get('save_ok') == want2['save'] and st2.get('sem_suporte') == 0,
          str(st2))
    want_fnv = fnv64(img1) if img1 else None
    check('S2: o LOAD entregou ao jogo exatamente o arquivo do cartao (relido da RAM do guest)',
          want_fnv is not None and S2['fech'].get('bu_load_fnv') == want_fnv and S2['fech'].get('bu_load_bytes') == str(len(img1)),
          'RAM %s (%s bytes), arquivo %s' % (S2['fech'].get('bu_load_fnv'), S2['fech'].get('bu_load_bytes'), want_fnv))
    i2, _ = envelope(os.path.join(card('s2'), 'card0', 'H_HUNTER.SYS.hhsv'))
    i2b, _ = envelope(os.path.join(card('s2b'), 'card0', 'H_HUNTER.SYS.hhsv'))
    check('S3: sessao 2 deterministica (quadros e arquivo)', S2b['rc'] == 0 and len(S2['hashes']) == VBLANKS // 50 and
          S2['hashes'] == S2b['hashes'] and i2 is not None and i2 == i2b)
    fn, fl = S2['fech'], r['leg']['fech']
    nat = {a: fn.get('f_' + a) for a in BU_FECHAMENTO}
    check('S5: fechamento no nativo (funcoes da bu geradas nao rodam)', all(v == '0' for v in nat.values()),
          ', '.join('%s=%s' % (a, v) for a, v in nat.items() if v != '0') or 'todas 0')
    check('S5: o legado roda o init original (referencia do fechamento)', int(fl.get('f_8C15FB44', '0')) >= 1,
          'f_8C15FB44 %s' % fl.get('f_8C15FB44'))
    # negativos
    D = r['semdata']
    fd = os.path.join(card('semdata'), 'card0', 'H_HUNTER.SYS.hhsv')
    _, td = envelope(fd) if os.path.isfile(fd) else (None, None)
    check('negativo sem_data: termina e o S1 (data) acusa', D['rc'] == 0 and td is not None and not (RTC <= td <= limit),
          't %s' % td)
    C = r['corrompe']
    fc = os.path.join(card('corrompe'), 'card0', 'H_HUNTER.SYS.hhsv')
    ic, _ = envelope(fc) if os.path.isfile(fc) else (None, None)
    check('negativo save_corrompe: termina e o S1 (imagem) acusa',
          C['rc'] == 0 and (ic is None or ic[:covered] != ref_vms[:covered]))
    A = r['loadaltera']
    check('negativo load_altera: termina e o S2 (bytes entregues) acusa',
          A['rc'] == 0 and A['fech'].get('bu_load_fnv') not in (None, want_fnv), A['fech'].get('bu_load_fnv'))
    L = r['loadfalha']
    check('negativo load_falha: termina e o S2 (chamadas) acusa', L['rc'] == 0 and calls(L['stats']) != want2,
          str(calls(L['stats'])))
except SystemExit:
    pass
finally:
    shutil.rmtree(tmp, ignore_errors=True)

print('%d falhas' % fails)
sys.exit(1 if fails else 0)
