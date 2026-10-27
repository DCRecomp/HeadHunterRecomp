#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
"""HH14: inventario reproduzivel do audio (docs/planos/HH14-PLANO.md). So imprime nomes, contagens, formatos e duracoes;
nenhum byte de audio. Le cabecalhos e tabelas com leituras limitadas (sem carregar arquivos inteiros).

  hh14_inventario.py streams <STREAMS> [<STREAMS do outro disco>]   formatos/duracao; iguais por hash entre discos
  hh14_inventario.py ahf <AHFDATA> [filtro]                         entradas dos pacotes AHFF por diretorio e extensao
  hh14_inventario.py perfil <perfil.tsv> <functions.tsv> <sdk_funcs.tsv>
                                                                     resumo de HH_FN_PERFIL por funcao e camada do chamador
"""
import bisect
import collections
import csv
import hashlib
import os
import struct
import sys


def stream_info(path):
    n = os.path.getsize(path)
    with open(path, 'rb') as fh:
        h = fh.read(48)
        fh.seek(0)
        sha = hashlib.sha256()
        for b in iter(lambda: fh.read(1 << 20), b''):
            sha.update(b)
    if h[:4] == b'RIFF':  # WAVE; format tag 0 = ADPCM de 4 bits da AICA
        tag, ch, sr = struct.unpack_from('<HHI', h, 20)
        return 'wav tag%d ch%d %dHz' % (tag, ch, sr), (n - 44) * 2 / ch / sr, sha.hexdigest()
    if h[:2] == b'\x80\x00':  # CRI ADX (big-endian): canais em +7, taxa em +8, amostras em +12
        ch, sr = h[7], struct.unpack_from('>I', h, 8)[0]
        return 'adx ch%d %dHz' % (ch, sr), struct.unpack_from('>I', h, 12)[0] / sr, sha.hexdigest()
    return '?' + h[:4].hex(), 0.0, sha.hexdigest()


def cmd_streams(a, b=None):
    ia = {f: stream_info(os.path.join(a, f)) for f in sorted(os.listdir(a))}
    k, dur = collections.Counter(), collections.Counter()
    for t, d, _ in ia.values():
        k[t] += 1
        dur[t] += d
    for t in sorted(k):
        print('%-24s %3d %6.1f min' % (t, k[t], dur[t] / 60))
    if b:
        ib = {f: stream_info(os.path.join(b, f)) for f in sorted(os.listdir(b))}
        common = set(ia) & set(ib)
        print('so no primeiro %d, so no segundo %d, mesmo nome e hash %d, mesmo nome e hash diferente %d' %
              (len(set(ia) - set(ib)), len(set(ib) - set(ia)), sum(ia[f][2] == ib[f][2] for f in common),
               sum(ia[f][2] != ib[f][2] for f in common)))


def ahff_entries(path):
    """AHFF v2: +8 contagem, +12 fim dos registros, +16 fim da tabela (alinhado a 8); a tabela (n u32 relativos a 0x1C)
    comeca 16 bytes depois do fim dos registros; cada registro em 0x1C + desl: nome NUL alinhado a 4 e tres u32
    (deslocamento absoluto, tamanho, 0)."""
    with open(path, 'rb') as f:
        h = f.read(0x1C)
        if h[:4] != b'AHFF':
            raise ValueError('nao e AHFF')
        _, n, end_rec, end_tab, _ = struct.unpack_from('<5I', h, 4)
        f.seek(0x1C)
        rec = f.read(end_tab - 0x1C)
    tab = struct.unpack_from('<%dI' % n, rec, end_rec + 16 - 0x1C)
    out = []
    for p in tab:
        z = rec.index(b'\0', p)
        name = rec[p:z].decode('latin-1')
        q = (z + 1 + 3) & ~3
        off, size, _ = struct.unpack_from('<3I', rec, q)
        out.append((name, off, size))
    return out


def cmd_ahf(root, filt=''):
    dirs, ext, bad = collections.Counter(), collections.Counter(), []
    for f in sorted(os.listdir(root)):
        try:
            es = ahff_entries(os.path.join(root, f))
        except (ValueError, struct.error) as e:
            bad.append(f)
            continue
        for name, _, _ in es:
            if filt not in name:
                continue
            parts = name.split('\\')
            dirs[parts[1] if len(parts) > 2 else '-'] += 1
            ext[parts[-1].rsplit('.', 1)[-1].upper() if '.' in parts[-1] else '-'] += 1
    print('diretorios', dirs.most_common())
    print('extensoes', ext.most_common())
    print('pacotes com outra variante (nao lidos): %d %s' % (len(bad), bad[:8]))


def cmd_perfil(prof, fns, sdk):
    starts = []
    with open(fns, encoding='utf-8') as f:
        r = csv.reader(f, delimiter='\t')
        next(r)
        for row in r:
            try:
                starts.append(int(row[0], 16))
            except ValueError:
                pass
    starts.sort()
    layer = {}
    with open(sdk, encoding='utf-8') as f:
        for row in csv.DictReader(f, delimiter='\t'):
            layer[int(row['funcao'], 16)] = row['camada']
    owner = lambda pc: starts[max(bisect.bisect_right(starts, pc) - 1, 0)]
    tot, game, span, callers = collections.Counter(), collections.Counter(), {}, collections.defaultdict(set)
    with open(prof, encoding='utf-8') as f:
        for row in csv.DictReader(f, delimiter='\t'):
            fn, n = int(row['funcao'], 16), int(row['chamadas'])
            c = owner(int(row['pr'], 16) - 4)
            tot[fn] += n
            if layer.get(c, '?') in ('jogo', 'desconhecida'):
                game[fn] += n
            callers[fn].add('%08X(%s)' % (c, layer.get(c, '?')[:4]))
            a, b = int(row['primeiro_vblank']), int(row['ultimo_vblank'])
            s = span.get(fn, (a, b))
            span[fn] = (min(s[0], a), max(s[1], b))
    print('funcao\tcamada\ttotal\tdo_jogo\tvblanks\tchamadores')
    for fn in sorted(tot):
        print('%08X\t%s\t%d\t%d\t%d-%d\t%s' % (fn, layer.get(fn, '?'), tot[fn], game[fn], span[fn][0], span[fn][1],
                                              ' '.join(sorted(callers[fn])[:4])))


if __name__ == '__main__':
    if len(sys.argv) < 3 or sys.argv[1] not in ('streams', 'ahf', 'perfil'):
        print(__doc__)
        sys.exit(2)
    {'streams': cmd_streams, 'ahf': cmd_ahf, 'perfil': cmd_perfil}[sys.argv[1]](*sys.argv[2:])
