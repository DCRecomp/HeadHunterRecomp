#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
"""HH13.6: superficie da API grafica que o jogo usa, a partir do perfil dinamico (HH_FN_PERFIL) e da identificacao do
SDK do HH07 (sdk_funcs: camada e familia por funcao).

Para cada funcao do SDK chamada por codigo do jogo (o chamador, achado pelo PR no mapa das funcoes do gerado, nao e do
SDK): chamadas, chamadores distintos, primeiro/ultimo VBlank, familia; e, separado, o que essas entradas chamam dentro
do SDK (o fechamento que o provedor nativo substitui).

  hh13_kamui_api.py <perfil.tsv> <sdk_funcs.tsv> <pasta do gerado>
"""
import bisect
import collections
import glob
import os
import re
import sys


def main():
    if len(sys.argv) < 4:
        print(__doc__)
        return 2
    perfil, sdkp, gen = sys.argv[1:4]
    sdk = {}
    for l in open(sdkp, encoding='utf-8'):
        p = l.rstrip('\n').split('\t')
        if p[0].startswith('0x'):
            sdk[int(p[0], 16)] = (p[1], p[2], p[5])
    starts = []
    for f in glob.glob(os.path.join(gen, 'funcs_*.cpp')):
        for l in open(f, encoding='utf-8', errors='replace'):
            m = re.match(r'void f_([0-9A-F]{8})\(', l)
            if m:
                starts.append(int(m.group(1), 16))
    starts.sort()
    dono = lambda pc: starts[bisect.bisect_right(starts, pc - 2) - 1] if pc >= starts[0] else 0
    is_sdk = lambda f: f in sdk and sdk[f][0] not in ('desconhecida', 'jogo')
    api = collections.defaultdict(lambda: [0, set(), 1 << 62, 0])
    interno = collections.Counter()
    for l in open(perfil, encoding='utf-8'):
        p = l.rstrip('\n').split('\t')
        if not p[0] or p[0] == 'funcao':
            continue
        fn, pr, n, v0, v1 = int(p[0], 16), int(p[1], 16), int(p[2]), int(p[3]), int(p[4])
        caller = dono(pr)
        if is_sdk(caller):
            interno[fn] += n
            continue
        a = api[fn]
        a[0] += n
        a[1].add(caller)
        a[2], a[3] = min(a[2], v0), max(a[3], v1)
    print('API do SDK chamada pelo jogo (%d funcoes):' % len(api))
    print('funcao     camada        familia     nome                 chamadas  chamadores  vblanks')
    for fn, a in sorted(api.items(), key=lambda kv: (-kv[1][0], kv[0])):
        cam, fam, nome = sdk.get(fn, ('?', '?', '-'))
        print('%08X   %-12s  %-10s  %-20s %8d  %10d  %d-%d' % (fn, cam, fam, nome[:20], a[0], len(a[1]), a[2], a[3]))
    print('\nfuncoes do SDK chamadas so de dentro do SDK: %d (chamadas %d)' % (len(interno), sum(interno.values())))
    return 0


if __name__ == '__main__':
    sys.exit(main())
