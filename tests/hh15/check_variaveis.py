#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
"""HH15.2 (revisao Codex): toda variavel HH_* lida pelo codigo do produto (app/, runtime/, transitional/, native/) esta
classificada em runtime/debug/validacao_vars.h: kTestVars (o produto recusa) ou kObservationalVars (so observa). Reprova
leitura sem classificacao, nome nas duas listas e nome listado que ninguem le.
uso: check_variaveis.py <raiz do projeto>
"""
import os
import re
import sys

root = os.path.abspath(sys.argv[1] if len(sys.argv) > 1 else '.')
hdr = open(os.path.join(root, 'runtime', 'debug', 'validacao_vars.h'), encoding='utf-8').read()


def listed(name):
    m = re.search(r'%s\[\] = \{(.*?)\};' % name, hdr, re.S)
    return set(re.findall(r'"(HH_[A-Z0-9_]+)"', m.group(1))) if m else set()


test, obs = listed('kTestVars'), listed('kObservationalVars')
read = {}
for sub in ('app', 'runtime', 'transitional', 'native'):
    for d, _, files in os.walk(os.path.join(root, sub)):
        for f in files:
            if f.endswith(('.cpp', '.h')) and f != 'validacao_vars.h':
                p = os.path.join(d, f)
                text = open(p, encoding='utf-8', errors='replace').read()
                for n in re.findall(r'(?:getenv|envIs)\("(HH_[A-Z0-9_]+)"', text):  # envIs: debug/validacao.cpp
                    read.setdefault(n, os.path.relpath(p, root))
bad = []
bad += ['sem classificacao: %s (%s)' % (n, read[n]) for n in sorted(set(read) - test - obs)]
bad += ['nas duas listas: %s' % n for n in sorted(test & obs)]
bad += ['listada e ninguem le: %s' % n for n in sorted((test | obs) - set(read))]
if not test or not obs:
    bad.append('listas vazias (cabecalho mudou de formato?)')
for b in bad:
    print('FALHA:', b)
print('%d variaveis lidas: %d de teste (recusadas pelo produto), %d observacionais' % (len(read), len(test), len(obs)))
sys.exit(1 if bad else 0)
