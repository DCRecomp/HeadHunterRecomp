#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
"""HH07.3/HH07.4: fechamento da fronteira declarada e catraca das metricas sobre o executavel
real, com a sonda do roteiro newgame_area1 (dados privados; sem eles = SKIP, rc 77).

Positivo: config/fronteiras.toml + sonda + config/metricas_base.toml -> rc 0.
Negativos (cada um precisa falhar, e pelo motivo esperado):
  - fronteira sem uma entrada direta chamada pelo jogo           -> rc 3, contorna_fronteira
  - fronteira sem cada [[dependencia_hw]] declarada              -> rc 3, acesso_hw da funcao
  - [[dependencia_hw]] com uma regiao a menos                    -> rc 3, acesso_hw da funcao
  - [[dependencia_hw]] extra numa funcao sem acesso a hardware   -> rc 3, OBSOLETA
  - sonda sem os acessos de uma regiao declarada (a referencia
    estatica continua: so a sonda confirma)                      -> rc 3, OBSOLETA da regiao
  - catraca com uma chave de dependencia abaixo do valor atual   -> rc 4, REGRESSAO da chave

uso: hh07_fechamento.py <hh-hwscan.exe> <raiz do projeto>
ambiente: HH07_BIN (1ST_READ.BIN), HH07_SONDA (TSV da sonda); padrao: as saidas locais ignoradas.
"""
import os
import re
import shutil
import subprocess
import sys
import tempfile

HWSCAN, ROOT = (os.path.abspath(a) for a in sys.argv[1:3])
BIN = os.environ.get('HH07_BIN', os.path.join(ROOT, 'game-data', 'hh01', 'disc1_ref', '1ST_READ.BIN'))
SONDA = os.environ.get('HH07_SONDA', os.path.join(ROOT, 'analysis', 'hh07', 'sonda_area1.tsv'))
CONFIG = os.path.join(ROOT, 'config', 'headhunter.toml')
FRONTEIRA = os.path.join(ROOT, 'config', 'fronteiras.toml')
METRICAS = os.path.join(ROOT, 'config', 'metricas_base.toml')
# funcoes do SDK nomeadas com duas evidencias no disco 1 (HH07-RELATORIO 3.1): sem um minimo
# fixo, o teste de nomes passaria com zero nomes (revisao Codex HH07)
NOMES_ESPERADOS = 13

if not (os.path.isfile(BIN) and os.path.isfile(SONDA)):
    print('SKIP: sem 1ST_READ.BIN ou sonda (dados privados)')
    sys.exit(77)

fails = 0
tmp = tempfile.mkdtemp(prefix='hh07_fech_')
OUT = os.path.join(tmp, 'saida')


def scan(fronteira, metricas=METRICAS, sonda=SONDA):
    p = subprocess.run([HWSCAN, CONFIG, BIN, OUT, '--sonda', sonda, '--fronteira', fronteira,
                        '--metricas-base', metricas], capture_output=True, text=True)
    return p.returncode, p.stdout + p.stderr


def closure_rows():
    with open(os.path.join(OUT, 'fechamento.tsv'), encoding='utf-8') as f:
        return [r.rstrip('\n').split('\t') for r in list(f)[1:]]


def expect(name, got, want, log, motivo=True):
    """rc igual ao esperado e o motivo (mensagem/linha do fechamento) presente."""
    global fails
    ok = got == want and motivo
    print('[%s] %s (rc=%d, esperado %d%s)' % (' ok ' if ok else 'FAIL', name, got, want,
                                              '' if motivo else ', motivo ausente'))
    if not ok:
        print(log)
    fails += not ok


def check(name, cond, log=''):
    global fails
    print('[%s] %s' % (' ok ' if cond else 'FAIL', name))
    if not cond and log:
        print(log)
    fails += not cond


def write(name, text):
    path = os.path.join(tmp, name)
    with open(path, 'w', encoding='utf-8', newline='\n') as f:
        f.write(text)
    return path


with open(FRONTEIRA, encoding='utf-8') as f:
    base = f.read()
blocks = re.split(r'\n(?=\[\[)', base)
head, entries = blocks[0], blocks[1:]
join = lambda es: '\n'.join([head] + es) + '\n'

deps = [i for i, b in enumerate(entries) if b.startswith('[[dependencia_hw]]')]
depFn = {i: re.search(r'funcao = (0x[0-9A-F]+)', entries[i]).group(1) for i in deps}
rc, log = scan(FRONTEIRA)
expect('fronteira declarada + sonda + catraca', rc, 0, log)
check('resumo com "acessos hw declarados %d"' % len(deps),
      bool(deps) and 'acessos hw declarados %d' % len(deps) in log, log)
declarados = {r[1] for r in closure_rows() if r[0] == 'acesso_hw_declarado'}
check('fechamento.tsv lista as %d dependencias declaradas' % len(deps), declarados == set(depFn.values()))

# HH07.2: nome so com duas evidencias (unico na imagem e com evidencia alem da mensagem)
names = {}
with open(os.path.join(OUT, 'sdk_funcs.tsv'), encoding='utf-8') as f:
    for row in list(f)[1:]:
        c = row.rstrip('\n').split('\t')
        if c[5] != '-':
            names.setdefault(c[5], []).append(c[0])
            if not any(not e.startswith('nome:') for e in c[6].split(',')):
                print('[FAIL] %s nomeada %s so com mensagens' % (c[0], c[5]))
                fails += 1
dup = {n: f for n, f in names.items() if len(f) > 1}
check('nomes unicos (%d nomeadas)' % len(names), not dup)
check('ao menos %d funcoes nomeadas' % NOMES_ESPERADOS, len(names) >= NOMES_ESPERADOS)

# 1. sem uma entrada direta chamada pelo jogo (a primeira com chamadas >= 1)
idx = next(i for i, b in enumerate(entries)
           if b.startswith('[[fronteira]]') and 'tipo = "direta"' in b and re.search(r'chamadas = [1-9]', b))
removed = re.search(r'endereco = (0x[0-9A-F]+)', entries[idx]).group(1)
rc, log = scan(write('sem_entrada.toml', join(entries[:idx] + entries[idx + 1:])))
expect('fronteira sem a entrada %s' % removed, rc, 3, log,
       ['contorna_fronteira', removed] in [r[:2] for r in closure_rows()])

# 2. sem cada uma das dependencias de hardware declaradas (uma de cada vez)
for i in deps:
    rc, log = scan(write('sem_dep.toml', join(entries[:i] + entries[i + 1:])))
    expect('fronteira sem a [[dependencia_hw]] %s' % depFn[i], rc, 3, log,
           ['acesso_hw', depFn[i]] in [r[:2] for r in closure_rows()])

# 3. regiao a menos: 0x8C090974 declarada so com store_queue (os QACR em ccn ficam fora)
i = next(i for i in deps if depFn[i] == '0x8C090974')
menos = entries[i].replace('regioes = ["store_queue", "ccn"]', 'regioes = ["store_queue"]')
check('0x8C090974 declara store_queue e ccn', menos != entries[i])
rc, log = scan(write('regiao_menos.toml', join(entries[:i] + [menos] + entries[i + 1:])))
expect('[[dependencia_hw]] 0x8C090974 sem a regiao ccn', rc, 3, log,
       ['acesso_hw', '0x8C090974'] in [r[:2] for r in closure_rows()])

# 4. dependencia extra numa funcao do jogo sem acesso a hardware (0x8C0C3C54, chamadora de 0x8C0C4CE0)
extra = base + ('\n[[dependencia_hw]]\nfuncao = 0x8C0C3C54\nsubsistema = "render"\ndono = "HH13"\n'
                'regioes = ["store_queue"]\nevidencia = "caso negativo do teste"\n')
rc, log = scan(write('dep_extra.toml', extra))
expect('[[dependencia_hw]] obsoleta', rc, 3, log, 'DEPENDENCIA_HW OBSOLETA: 0x8C0C3C54' in log)

# 5. sonda sem as escritas de 0x8C090974 nos QACR: a referencia estatica nao confirma
with open(SONDA, encoding='utf-8') as f:
    linhas = f.readlines()
sem_ccn = [l for l in linhas if not (l.split('\t')[:2] == ['w', '0x8C090974'] and '\tccn\t' in l)]
check('sonda tem as escritas de 0x8C090974 em ccn', len(sem_ccn) < len(linhas))
rc, log = scan(FRONTEIRA, sonda=write('sonda_sem_ccn.tsv', ''.join(sem_ccn)))
expect('sonda sem os QACR de 0x8C090974', rc, 3, log, 'DEPENDENCIA_HW OBSOLETA: 0x8C090974 regiao ccn' in log)

# 6. catraca: base com uma dependencia (e uma regiao) a menos
with open(METRICAS, encoding='utf-8') as f:
    met = f.read()
for chave in ('fechamento.dependencias_hw_declaradas', 'fechamento.dependencias_hw_regioes'):
    m = re.search(r'"%s" = (\d+)' % re.escape(chave), met)
    check('metricas_base tem %s' % chave, bool(m))
    if not m:
        continue
    pior = met.replace(m.group(0), '"%s" = %d' % (chave, int(m.group(1)) - 1))
    rc, log = scan(FRONTEIRA, write('metricas_menor.toml', pior))
    expect('catraca de %s abaixo do valor atual' % chave, rc, 4, log, 'REGRESSAO: %s' % chave in log)

shutil.rmtree(tmp, ignore_errors=True)  # a saida tem strings do jogo: nao fica no disco
print('%d falhas' % fails)
sys.exit(1 if fails else 0)
