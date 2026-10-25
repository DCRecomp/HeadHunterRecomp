#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
"""HH05.5: regressao do boot contra o Flycast. Dados locais do usuario (fora do git) em
HH_SNAPSHOT_DIR:
    boot_entry.bss/.pvr     estado na entrada 0x8C010000 com o RTC da sessao do trace
    boot_flycast.tsv        flycast_oracle.py trace 0x8C010000 <entradas sem 8C1793EA/8C159820>
e os dados de execucao em HH_DATA_DIR. Sem eles, sai com 77 (SKIP no CTest).
O hh-diff precisa terminar com rc 0 e pelo limite de 3000 chamadas (nao por erro).
uso: hh_boot_regression.py <hh-diff.exe> <hh05_boot_check.py> <aceitas.tsv>"""
import os
import subprocess
import sys
import tempfile


def main():
    hh_diff, check, accepted = os.path.abspath(sys.argv[1]), sys.argv[2], sys.argv[3]
    snap, data = os.environ.get('HH_SNAPSHOT_DIR'), os.environ.get('HH_DATA_DIR')
    needed = [os.path.join(snap or '', f) for f in ('boot_entry.bss', 'boot_entry.pvr', 'boot_flycast.tsv')]
    if not snap or not data or not all(os.path.exists(p) for p in needed):
        print('HH_SNAPSHOT_DIR/HH_DATA_DIR ausentes: teste ignorado')
        return 77
    with tempfile.TemporaryDirectory() as tmp:
        out = os.path.join(tmp, 'recomp.tsv')
        env = dict(os.environ, HH_TRACE_SKIP='0x8C1793EA,0x8C159820')
        r = subprocess.run([hh_diff, 'trace', '0x8C010000', needed[0], out, '3000'],
                           env=env, capture_output=True, text=True, timeout=900)
        if r.returncode != 0 or 'limite de 3000 chamadas atingido' not in r.stdout or not os.path.exists(out):
            print('ERRO: hh-diff nao terminou pelo limite (rc=%d)' % r.returncode)
            print(r.stdout[-2000:], r.stderr[-2000:])
            return 1
        return subprocess.run([sys.executable, '-I', check, needed[2], out, accepted, '--len-b', '3000']).returncode


if __name__ == '__main__':
    sys.exit(main())
