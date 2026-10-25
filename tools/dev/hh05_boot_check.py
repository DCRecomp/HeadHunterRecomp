#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
"""HH05.5: portao do trace do boot contra o Flycast (dados privados em HH_SNAPSHOT_DIR).

Regras (docs/relatorios/HH05-RELATORIO.md, revisao Codex 2026-10-25):
  - cada linha dos dois traces tem exatamente 21 colunas hexadecimais de 8 digitos;
  - o trace recompilado tem exatamente --len-b linhas (padrao 3000);
  - a sequencia de funcoes e identica ate a divergencia DECLARADA em <aceitas.tsv>
    ("divergencia <chamada> <funcao flycast> <funcao recompilado> <motivo>"); qualquer outra
    diferenca de sequencia e erro; o trace do Flycast precisa alcancar essa chamada;
  - PR pode diferir so no espelho P1/P2 do mesmo endereco fisico;
  - r0, r2, r3, r10 na entrada de 8C1793E0 (leitura do contador de tempo) podem diferir;
  - o resto so com entrada exata em <aceitas.tsv>; toda entrada aceita precisa ocorrer.
uso: hh05_boot_check.py <flycast.tsv> <recompilado.tsv> <aceitas.tsv> [--len-b N]
rc: 0 = dentro das regras; 1 = divergencia; 2 = entrada invalida."""
import re
import sys

NAMES = ["func"] + ["r%d" % i for i in range(16)] + ["pr", "sr", "fpscr", "fpul"]
TIME_FN, TIME_REGS = "8C1793E0", {"r0", "r2", "r3", "r10"}
HEX8 = re.compile(r"^[0-9A-Fa-f]{8}$")


def load_trace(path):
    rows = []
    for n, line in enumerate(open(path), 1):
        f = line.rstrip("\n").split("\t")
        if len(f) != len(NAMES) or not all(HEX8.match(x) for x in f):
            raise SystemExit("%s:%d: linha invalida (esperado %d colunas hexadecimais)" % (path, n, len(NAMES)))
        rows.append([x.upper() for x in f])
    return rows


def load_accepted(path):
    acc, div = {}, None
    for n, line in enumerate(open(path, encoding="utf-8"), 1):
        line = line.split("#", 1)[0].strip()
        if not line:
            continue
        f = line.split("\t")
        if f[0] == "divergencia" and len(f) >= 5:
            div = (int(f[1]), f[2].upper(), f[3].upper())
        elif len(f) >= 6 and f[2] in NAMES:
            acc[(int(f[0]), f[1].upper(), f[2])] = (f[3].upper(), f[4].upper())
        else:
            raise SystemExit("%s:%d: entrada invalida" % (path, n))
    if not div:
        raise SystemExit("%s: falta a linha 'divergencia'" % path)
    return acc, div


def main():
    if len(sys.argv) < 4:
        print(__doc__)
        return 2
    A, B = load_trace(sys.argv[1]), load_trace(sys.argv[2])
    acc, (dvi, dva, dvb) = load_accepted(sys.argv[3])
    len_b = int(sys.argv[sys.argv.index("--len-b") + 1]) if "--len-b" in sys.argv else 3000
    bad, used = [], set()
    if len(B) != len_b:
        bad.append("trace recompilado com %d linhas (esperado %d)" % (len(B), len_b))
    if len(A) <= dvi or len(B) <= dvi:
        bad.append("traces nao alcancam a divergencia declarada (#%d): %d x %d linhas" % (dvi, len(A), len(B)))
    for i in range(min(dvi, len(A), len(B))):
        x, y = A[i], B[i]
        if x[0] != y[0]:
            bad.append("#%d sequencia: flycast %s recompilado %s" % (i, x[0], y[0]))
            break
        for k, (u, v) in enumerate(zip(x, y)):
            if u == v:
                continue
            col = NAMES[k]
            if col == "pr" and (int(u, 16) & 0x1FFFFFFF) == (int(v, 16) & 0x1FFFFFFF):
                continue
            if x[0] == TIME_FN and col in TIME_REGS:
                continue
            key = (i, x[0], col)
            if acc.get(key) == (u, v):
                used.add(key)
                continue
            bad.append("#%d %s %s flycast=%s recompilado=%s" % (i, x[0], col, u, v))
    if len(A) > dvi and len(B) > dvi and (A[dvi][0], B[dvi][0]) != (dva, dvb):
        bad.append("divergencia declarada nao ocorreu: #%d flycast %s recompilado %s (esperado %s x %s)"
                   % (dvi, A[dvi][0], B[dvi][0], dva, dvb))
    for k in acc:
        if k not in used:
            bad.append("aceita nao ocorreu: #%d %s %s" % k)
    print("chamadas iguais em sequencia: %d (divergencia declarada #%d); aceitas usadas: %d" % (dvi, dvi, len(used)))
    for b in bad[:20]:
        print("  ERRO", b)
    return 1 if bad else 0


if __name__ == "__main__":
    sys.exit(main())
