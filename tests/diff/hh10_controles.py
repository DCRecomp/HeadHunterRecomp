#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
"""HH10: portoes dos controles nativos no roteiro newgame_area1 (4 700 VBlanks) e nos roteiros de conexao.

C1  HH_PAD_TRACE (unidade 0 do PDS_PERIPHERAL por VBlank, com as leituras) legado x nativo igual,
    fora os ponteiros info/name (o nativo aponta para a area nativa do BIOS HLE, por desenho);
C2  94 hashes de quadro iguais nos dois modos e a referencia do HH06 (HH_AREA1_HASHES);
C3  fechamento (HH_PAD_FECHAMENTO): no nativo, 0 entradas no servidor 8C15DB3A e no decodificador
    8C15D900 gerados e 0 GET_CONDITION na porta A do Maple;
C4  ponto de amostragem: a coluna de leituras (indice da amostra) igual por VBlank (dentro do C1) e
    o negativo amostra_adiantada (1 leitura adiantada) acusado. (Amostra nos disparos com retorno < 0
    nao e acusavel nesta rota: pd_disparos = pd_disparos_efetivos, nenhum disparo devolve < 0.)
C5  conexao: 4 roteiros "indice transicao" (desconectar com botao apertado, ausente no inicio,
    reconectar neutro, reconectar com botao): traco, conteudo de info/name e quadros = legado (o
    Maple com a presenca roteirizada e o oraculo), com a porta vazia depois do boot e o controle de
    volta depois dela; negativo presenca_imediata acusado;
C7  conteudo apontado por info (120 bytes) e name (texto) igual ao legado em todo VBlank; negativo
    info_sem_conectado acusado;
C8  tempo experimental: C1 e quadros iguais legado x nativo; o nativo e deterministico.
Negativos (terminam normalmente e o portao certo acusa): press_on -> C1; maple (o controle
continua no Maple) -> C3; os de C4, C5 e C7 acima. (old_host nao e acusavel nesta rota: o jogo
nunca reescreve o on do PDS; o old lido da RAM e coberto pelo unitario
hh10_pad_adapter_presence_model_and_server. C6, remapeamento, e o unitario hh10_pad_adapter_remap_c6.)

uso: hh10_controles.py <hh-run.exe> <raiz do projeto>
ambiente: HH_DATA_DIR (dados privados; sem ele = SKIP, rc 77), HH_AREA1_HASHES (obrigatoria com os dados).
"""
import os
import re
import shutil
import subprocess
import sys
import tempfile
from concurrent.futures import ThreadPoolExecutor

RUN, ROOT = (os.path.abspath(a) for a in sys.argv[1:3])
GEN = os.path.abspath(sys.argv[3]) if len(sys.argv) > 3 else ''
DATA = os.environ.get('HH_DATA_DIR', '')
REF = os.environ.get('HH_AREA1_HASHES', '')
SCRIPT = os.path.join(ROOT, 'tools', 'dist', 'roteiros', 'newgame_area1.txt')
VBLANKS = 4700
CONN_VBLANKS = 1600
if not os.path.isfile(os.path.join(DATA, 'disc.manifest')):
    print('SKIP: defina HH_DATA_DIR')
    sys.exit(77)

# C5: indice = transicao injetada (uma por VBlank-In, no primeiro acesso ao MDST; o Maple segue
# ativo no nativo pela camada per), entao a porta vazia nao trava o roteiro
CONN = {
    'desconecta_apertado': '0 -\n1000 START\n1010 DESCONECTADO\n1200 -\n1300 START\n1330 -\n',
    'ausente_inicio': '0 DESCONECTADO\n800 -\n1100 START\n1130 -\n',
    'reconecta_neutro': '0 -\n900 DESCONECTADO\n1000 -\n1100 START\n1130 -\n',
    'reconecta_botao': '0 -\n900 DESCONECTADO\n1000 START\n1030 -\n',
}

tmp = tempfile.mkdtemp(prefix='hh10_ctl_')
fails = 0


def check(name, cond, info=''):
    global fails
    print('[%s] %s%s' % (' ok ' if cond else 'FAIL', name, (' (%s)' % info) if info else ''))
    fails += not cond


def run(tag, controls, tempo='nativo', mutation=None, script=SCRIPT, vblanks=VBLANKS):
    trace, closure = os.path.join(tmp, tag + '_pad.tsv'), os.path.join(tmp, tag + '_fech.tsv')
    env = dict(os.environ, HH_INPUT_SCRIPT=script, HH_PAD_TRACE=trace, HH_PAD_FECHAMENTO=closure)
    env.pop('HH_PAD_NATIVO_MUTACAO', None)
    if mutation:
        env['HH_PAD_NATIVO_MUTACAO'] = mutation
    p = subprocess.run([RUN, '--data', DATA, '--vblanks', str(vblanks), '--rtc', '1700000000', '--every', '50',
                        '--tempo', tempo, '--controles', controls], capture_output=True, text=True, env=env)
    rd = lambda f: open(f, encoding='utf-8').read().splitlines() if os.path.isfile(f) else []
    fech = dict(l.split('\t', 1) for l in rd(closure) if '\t' in l)
    return {'rc': p.returncode, 'hashes': re.findall(r'quadro \d+: \S+ hash ([0-9a-f]+)', p.stdout),
            'trace': rd(trace), 'fech': fech, 'err': p.stderr}


def masked(lines):
    """VBlank, leituras e o PDS sem os ponteiros name (+0x24) e info (+0x30)."""
    return [(c[0], c[1], c[2][:72] + c[2][80:96]) for c in (l.split('\t') for l in lines)]


def contents(lines):
    """Conteudo apontado por info e name (C7)."""
    return [tuple(l.split('\t')[3:5]) for l in lines]


def reads(lines):
    return [l.split('\t')[1] for l in lines]


conn_scripts = {}
for name, text in CONN.items():
    conn_scripts[name] = os.path.join(tmp, name + '.txt')
    with open(conn_scripts[name], 'w', encoding='utf-8', newline='\n') as f:
        f.write('# HH10 C5: %s\nindice transicao\n%s' % (name, text))

try:
    # C3 estatico (revisao Codex HH10): no gerado compilado, toda chamada ao servidor, ao getter e ao
    # disparo passa pelo slot (HHG_SLOT); uma chamada direta (HHG_FN/f_) contornaria o provedor nativo
    direct, slots = 0, 0
    pd = ('8C15DB3A', '8C15DC40', '8C17F0AA')
    for fname in (sorted(os.listdir(GEN)) if GEN and os.path.isdir(GEN) else []):
        if fname.startswith('funcs_') and fname.endswith('.cpp'):
            with open(os.path.join(GEN, fname), encoding='utf-8', errors='replace') as f:
                text = f.read()
            slots += sum(text.count('HHG_SLOT(%s)' % a) for a in pd)
            direct += sum(len(re.findall(r'HHG_FN\(%s\)|\bf_%s\s*\(\s*ctx' % (a, a), text)) for a in pd)
    check('C3 estatico: chamadas do jogo ao pd so pelo slot no gerado compilado', slots > 0 and direct == 0,
          '%s: %d por slot, %d diretas' % (GEN or 'sem HH_GENERATED_DIR', slots, direct))
    with ThreadPoolExecutor(6) as ex:
        jobs = {
            'leg': ex.submit(run, 'leg', 'legado'),
            'nat': ex.submit(run, 'nat', 'nativo'),
            'xleg': ex.submit(run, 'xleg', 'legado', 'experimental'),
            'xnat': ex.submit(run, 'xnat', 'nativo', 'experimental'),
            'xnat2': ex.submit(run, 'xnat2', 'nativo', 'experimental'),
            'press': ex.submit(run, 'press', 'nativo', 'nativo', 'press_on'),
            'adiant': ex.submit(run, 'adiant', 'nativo', 'nativo', 'amostra_adiantada'),
            'infonc': ex.submit(run, 'infonc', 'nativo', 'nativo', 'info_sem_conectado'),
        }
        for name, path in conn_scripts.items():
            for mode in ('legado', 'nativo'):
                jobs['c5_%s_%s' % (name, mode)] = ex.submit(run, 'c5_%s_%s' % (name, mode), mode, 'nativo', None,
                                                            path, CONN_VBLANKS)
        jobs['c5_presenca'] = ex.submit(run, 'c5_presenca', 'nativo', 'nativo', 'presenca_imediata',
                                        conn_scripts['reconecta_neutro'], CONN_VBLANKS)
        r = {k: v.result() for k, v in jobs.items()}
    with ThreadPoolExecutor(1) as ex:
        r['maple'] = ex.submit(run, 'maple', 'nativo', 'nativo', 'maple').result()
    L, N = r['leg'], r['nat']
    check('legado e nativo terminam pelo limite', L['rc'] == 0 and N['rc'] == 0, 'rc %d/%d' % (L['rc'], N['rc']))
    check('C1: traco do PDS igual (fora info/name)', len(L['trace']) == VBLANKS and masked(L['trace']) == masked(N['trace']),
          '%d linhas' % len(L['trace']))
    check('C2: 94 hashes iguais legado x nativo', len(N['hashes']) == VBLANKS // 50 and L['hashes'] == N['hashes'])
    ref = []
    if os.path.isfile(REF):
        with open(REF) as f:
            ref = [l.split()[1] for l in f if l.strip()]
    # obrigatoria (revisao Codex HH10): sem a referencia o portao nao aprova
    check('C2: nativo igual a referencia do HH06 (HH_AREA1_HASHES)', len(ref) == VBLANKS // 50 and N['hashes'] == ref,
          REF or 'HH_AREA1_HASHES ausente')
    fn = N['fech']
    check('C3: fechamento no nativo (servidor e decodificador gerados nao rodam; 0 GET_CONDITION)',
          fn.get('f_8C15DB3A') == '0' and fn.get('f_8C15D900') == '0' and fn.get('maple_portaA_getcond') == '0',
          'servidor %s, decodificador %s, getcond %s' % (fn.get('f_8C15DB3A'), fn.get('f_8C15D900'), fn.get('maple_portaA_getcond')))
    fl = L['fech']
    check('C3: o legado usa o caminho do SDK (referencia do fechamento)',
          int(fl.get('f_8C15DB3A', '0')) > 1000 and int(fl.get('maple_portaA_getcond', '0')) > 1000)
    # area nativa do BIOS HLE (0x8C006000, bios.h): o legado nunca a altera, nem com a porta vazia
    areas = [R['fech'].get('area_nativa_alterada') for R in [L] + [r['c5_%s_legado' % n] for n in CONN]]
    check('area nativa intocada pelo jogo em todo VBlank (legado, roteiro e conexao)', areas == ['0'] * len(areas),
          str(areas))
    # os zeros acima tambem sairiam com o slot nativo nunca alcancado (revisao Codex HH10): o adaptador
    # tem de ter atendido cada passada do servidor do legado, os disparos e o getter, sem falta da guarda
    # (pd_servidores_extras e so diagnostico no HH: a coleta e no disparo; ver pad_adapter.cpp)
    num = lambda d, k: int(d.get(k, '-1'))
    check('C3: o adaptador nativo atendeu o servidor, os disparos e o getter',
          num(fn, 'pd_servidor') == num(fl, 'f_8C15DB3A') and num(fn, 'pd_disparos_efetivos') > 4000 and
          num(fn, 'pd_getter') > 0 and num(fn, 'pd_guarda') == 0,
          'servidor %s/%s, disparos %s, getter %s' % (fn.get('pd_servidor'), fl.get('f_8C15DB3A'),
                                                      fn.get('pd_disparos_efetivos'), fn.get('pd_getter')))
    lr = reads(L['trace'])
    check('C4: leituras por VBlank iguais e andando (indice da amostra)',
          lr == reads(N['trace']) and lr and int(lr[-1]) > 4000, 'ultima %s' % (lr[-1] if lr else '-'))
    lc = contents(L['trace'])
    named = sum(1 for _, n in lc if n.startswith('Dreamcast Controller'))
    check('C7: conteudo de info/name igual ao legado em todo VBlank', lc == contents(N['trace']) and named > 4000,
          '%d VBlanks com o controle identificado' % named)
    X, Y, Y2 = r['xleg'], r['xnat'], r['xnat2']
    full = lambda R: R['rc'] == 0 and len(R['trace']) == VBLANKS and len(R['hashes']) == VBLANKS // 50
    check('C8: experimental, traco e quadros iguais legado x nativo',
          full(X) and full(Y) and full(Y2) and masked(X['trace']) == masked(Y['trace']) and
          contents(X['trace']) == contents(Y['trace']) and X['hashes'] == Y['hashes'],
          'rc %d/%d/%d' % (X['rc'], Y['rc'], Y2['rc']))
    check('C8: experimental nativo deterministico', Y['hashes'] == Y2['hashes'] and Y['trace'] == Y2['trace'])
    for name in CONN:
        A, B = r['c5_%s_legado' % name], r['c5_%s_nativo' % name]
        # id 0 = sem dispositivo; o boot (~107 VBlanks) nao conta: a porta tem de ficar vazia depois dele e
        # o controle tem de voltar depois da ultima linha vazia (antes o indice de transicao ficava em 0 e
        # estes roteiros so cobriam o boot)
        ids = [l.split('\t')[2][:8] for l in A['trace']]
        gone = [i for i, v in enumerate(ids) if v == '00000000' and i >= 150]
        back = bool(gone) and any(v != '00000000' for v in ids[gone[-1] + 1:])
        check('C5 %s: traco, info/name e quadros = legado' % name,
              A['rc'] == 0 and B['rc'] == 0 and len(A['trace']) == CONN_VBLANKS and back and
              masked(A['trace']) == masked(B['trace']) and contents(A['trace']) == contents(B['trace']) and
              A['hashes'] == B['hashes'], 'rc %d/%d, %d VBlanks sem dispositivo depois do boot, volta %s' %
              (A['rc'], B['rc'], len(gone), back))
    for tag, gate, cmp in (('press', 'C1', masked), ('adiant', 'C1/C4', masked), ('infonc', 'C7', contents)):
        M = r[tag]
        check('negativo %s: termina e o %s acusa' % (tag, gate),
              M['rc'] == 0 and M['trace'] and cmp(M['trace']) != cmp(L['trace']), 'rc %d' % M['rc'])
    P, base = r['c5_presenca'], r['c5_reconecta_neutro_legado']
    check('negativo presenca_imediata: termina e o C5 acusa',
          P['rc'] == 0 and P['trace'] and (masked(P['trace']) != masked(base['trace']) or
                                           contents(P['trace']) != contents(base['trace'])), 'rc %d' % P['rc'])
    Mp = r['maple']
    check('negativo maple: termina e o C3 acusa (GET_CONDITION no Maple)',
          Mp['rc'] == 0 and int(Mp['fech'].get('maple_portaA_getcond', '0')) > 0, 'rc %d' % Mp['rc'])
finally:
    shutil.rmtree(tmp, ignore_errors=True)

print('%d falhas' % fails)
sys.exit(1 if fails else 0)
