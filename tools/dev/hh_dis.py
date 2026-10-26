"""Desmontagem de leitura de uma funcao do C++ gerado (comentarios "// ENDERECO: instrucao" do codegen), com os
literais mov.l @(disp,pc) resolvidos no 1ST_READ.BIN (base 0x8C010000). Ferramenta de analise (HH12): nao imprime
bytes do executavel alem dos valores de 32 bits dos literais usados pelas instrucoes listadas.

  python -I tools/dev/hh_dis.py <generated/discN> <1ST_READ.BIN> <endereco> [--max N]
"""
import glob
import os
import re
import struct
import sys

BASE = 0x8C010000


def find_function(gen, addr):
    name = 'void f_%08X(' % addr
    for p in sorted(glob.glob(os.path.join(gen, 'funcs_*.cpp'))):
        with open(p, encoding='utf-8', errors='replace') as f:
            lines = f.readlines()
        for i, l in enumerate(lines):
            if l.startswith(name):
                out = []
                for l2 in lines[i + 1:]:
                    if l2.startswith('void f_') or l2.startswith('}\n') and out and False:
                        break
                    if l2.startswith('void f_'):
                        break
                    out.append(l2)
                return out
    return None


def main():
    if len(sys.argv) < 4:
        print(__doc__)
        return 2
    gen, binp, addr = sys.argv[1], sys.argv[2], int(sys.argv[3], 16)
    limit = int(sys.argv[sys.argv.index('--max') + 1]) if '--max' in sys.argv else 400
    body = find_function(gen, addr)
    if body is None:
        print('funcao %08X nao encontrada em %s' % (addr, gen))
        return 1
    size = os.path.getsize(binp)
    f = open(binp, 'rb')

    def lit(a):
        off = a - BASE
        if off < 0 or off + 4 > size:
            return None
        f.seek(off)
        return struct.unpack('<I', f.read(4))[0]

    rx = re.compile(r'^\s*// ([0-9A-F]{8}): (.*)$')
    n = 0
    for l in body:
        m = rx.match(l)
        if not m:
            continue
        a, ins = m.group(1), m.group(2)
        note = ''
        lm = re.search(r'mov\.l 0x([0-9a-f]{8}),', ins)
        if lm:
            v = lit(int(lm.group(1), 16))
            if v is not None:
                note = '    ; =0x%08X' % v
        print('%s: %s%s' % (a, ins, note))
        n += 1
        if n >= limit:
            break
    return 0


if __name__ == '__main__':
    sys.exit(main())
