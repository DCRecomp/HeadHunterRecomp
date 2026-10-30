#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
"""HH15.9: inventario da elevacao (remocao do substrato AOT, D-HH15-1 = B).

Junta a classificacao das funcoes do hh-hwscan (camada jogo/sdk/desconhecida, familia, faixas e modulos do SDK pelos
banners), o grafo de chamadas diretas e os perfis de execucao (HH_FN_PERFIL) das rotas, e grava:

  funcoes.tsv        funcao, camada, familia, nome, chamadas por perfil, chamadores/chamados diretos, nivel (folhas = 0)
  superficie_sdk.tsv as funcoes do SDK chamadas diretamente pelo jogo (a API a substituir), com contagens
  resumo.txt         totais por camada e familia (total, executadas em cada rota) e a distribuicao por nivel

So enderecos, nomes do SDK ja identificados e contagens: nenhum byte do jogo. Saida privada (analysis/, ignorada).

uso: hh15_elevacao_inventario.py <saida do hh-hwscan> <saida> <perfil.tsv> [<perfil.tsv> ...]
"""
import collections
import csv
import os
import sys


def tsv(path):
    with open(path, encoding='utf-8', newline='') as f:
        return list(csv.DictReader(f, delimiter='\t'))


def addr(s):
    return int(s, 16)


def main():
    scan, out, perfis = sys.argv[1], sys.argv[2], sys.argv[3:]
    os.makedirs(out, exist_ok=True)
    funcs = {addr(r['funcao']): r for r in tsv(os.path.join(scan, 'sdk_funcs.tsv'))}
    callees, callers = collections.defaultdict(set), collections.defaultdict(set)
    indirect = collections.Counter()
    for r in tsv(os.path.join(scan, 'callgraph.tsv')):
        a = addr(r['chamador'])
        if r['chamado'] == '-':
            indirect[a] += 1
            continue
        b = addr(r['chamado'])
        callees[a].add(b)
        callers[b].add(a)
    runs = []
    for p in perfis:
        hits = collections.Counter()
        for r in tsv(p):
            hits[addr(r['funcao'])] += int(r['chamadas'])
        runs.append((os.path.splitext(os.path.basename(p))[0], hits))

    # nivel: folhas (sem chamadas diretas a outras funcoes) = 0; ciclos ficam no nivel do primeiro que fecha
    level = {}
    for a in sorted(funcs):
        stack = [(a, iter(sorted(callees[a])))]
        onstack = {a}
        while stack:
            f, it = stack[-1]
            nxt = next((c for c in it if c in funcs and c not in level and c not in onstack), None)
            if nxt is not None:
                stack.append((nxt, iter(sorted(callees[nxt]))))
                onstack.add(nxt)
                continue
            stack.pop()
            onstack.discard(f)
            level[f] = 1 + max((level[c] for c in callees[f] if c in level), default=-1)

    with open(os.path.join(out, 'funcoes.tsv'), 'w', encoding='utf-8', newline='\n') as o:
        o.write('funcao\tcamada\tfamilia\tnome\t%s\tchamadores\tchamados\tindiretos\tnivel\n'
                % '\t'.join(n for n, _ in runs))
        for a in sorted(funcs):
            r = funcs[a]
            o.write('0x%08X\t%s\t%s\t%s\t%s\t%d\t%d\t%d\t%d\n' % (
                a, r['camada'], r['familia'], r['nome'], '\t'.join(str(h[a]) for _, h in runs),
                len(callers[a]), len(callees[a]), indirect[a], level[a]))

    surface = collections.Counter()
    for a, r in funcs.items():
        if r['camada'] == 'jogo':
            for b in callees[a]:
                if b in funcs and funcs[b]['camada'] == 'sdk':
                    surface[b] += 1
    with open(os.path.join(out, 'superficie_sdk.tsv'), 'w', encoding='utf-8', newline='\n') as o:
        o.write('funcao\tfamilia\tnome\tchamadores_do_jogo\t%s\n' % '\t'.join(n for n, _ in runs))
        for b in sorted(surface, key=lambda b: (funcs[b]['familia'], b)):
            r = funcs[b]
            o.write('0x%08X\t%s\t%s\t%d\t%s\n' % (b, r['familia'], r['nome'], surface[b],
                                                  '\t'.join(str(h[b]) for _, h in runs)))

    lines = []
    by = collections.defaultdict(list)
    for a, r in funcs.items():
        by[(r['camada'], r['familia'])].append(a)
    lines.append('%-12s %-14s %6s %s' % ('camada', 'familia', 'total', '  '.join('exec:%s' % n for n, _ in runs)))
    for k in sorted(by):
        fs = by[k]
        lines.append('%-12s %-14s %6d %s' % (k[0], k[1], len(fs),
                                             '  '.join('%*d' % (5 + len(n), sum(1 for a in fs if h[a])) for n, h in runs)))
    lines.append('')
    lines.append('superficie do SDK chamada pelo jogo: %d funcoes (%s)' % (len(surface), ', '.join(
        '%s %d' % (f, n) for f, n in sorted(collections.Counter(funcs[b]['familia'] for b in surface).items()))))
    game = [a for a, r in funcs.items() if r['camada'] == 'jogo']
    dist = collections.Counter(min(level[a], 10) for a in game)
    lines.append('jogo por nivel (0 = folha; 10 = 10+): ' + ', '.join('%d:%d' % (k, dist[k]) for k in sorted(dist)))
    lines.append('jogo com chamada indireta: %d funcoes, %d pontos' % (
        sum(1 for a in game if indirect[a]), sum(indirect[a] for a in game)))
    text = '\n'.join(lines) + '\n'
    open(os.path.join(out, 'resumo.txt'), 'w', encoding='utf-8', newline='\n').write(text)
    sys.stdout.write(text)


if __name__ == '__main__':
    main()
