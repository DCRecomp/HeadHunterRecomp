#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
"""HH05.7: laco de funcoes descobertas em execucao (metodo R2 do Blue Stinger).

Roda o hh-run; se a parada for "desvio indireto para endereco sem funcao recompilada",
confere o alvo no executavel (alinhado, fronteira de funcao: palavra anterior e nop,
desvio incondicional com slot, ou dado nao decodificavel), acrescenta [[functions]] ao
config com a evidencia (PC do desvio), regenera o C++ e recompila. Repete ate parar por
outro motivo ou atingir --max. Nenhum byte do jogo e gravado; so enderecos.

uso: python -I tools/dev/hh05_misses.py --data <dados> --exe <1ST_READ.BIN> [--vblanks N] [--max K]
"""
import argparse, os, re, struct, subprocess, sys

ap = argparse.ArgumentParser()
ap.add_argument('--data', required=True)
ap.add_argument('--exe', required=True)
ap.add_argument('--game-data', required=True, help='pasta com o 1ST_READ.BIN para o hh-recomp')
ap.add_argument('--config', default='config/headhunter.toml')
ap.add_argument('--vblanks', default='600')
ap.add_argument('--max', type=int, default=20)
ap.add_argument('--generated', default='generated/disc1')
a = ap.parse_args()

BASE = 0x8C010000
img = open(a.exe, 'rb').read()
w16 = lambda addr: struct.unpack_from('<H', img, addr - BASE)[0]
MISS = re.compile(r'desvio indireto para endereco sem funcao recompilada em PC=([0-9A-F]{8}) \(valor ([0-9A-F]{8})\)')

def boundary(t):
    if t & 1 or not (BASE + 4 <= t < BASE + len(img)): return False, 'fora da imagem ou impar'
    prev = w16(t - 2)
    if prev == 0x0009: return True, 'precedido de nop de alinhamento'
    p2 = w16(t - 4)
    if p2 in (0x000B, 0x002B) or (p2 & 0xF000) in (0xA000,) or (p2 & 0xF0FF) in (0x402B,): return True, 'logo apos rts/rte/bra/jmp + slot'
    return False, 'palavra anterior %04X nao indica fronteira' % prev

def run(cmd):
    if cmd[0].startswith('build/'): cmd = [os.path.abspath(cmd[0])] + cmd[1:]
    return subprocess.run(cmd, capture_output=True, text=True, encoding='utf-8', errors='replace')

for it in range(a.max):
    r = run(['build/app/Release/hh-run.exe', '--data', a.data, '--vblanks', a.vblanks, '--rtc', '1700000000'])
    out = r.stdout + r.stderr
    last = [l for l in out.splitlines() if l.startswith(('PARADA', 'ok:', 'vblanks:'))]
    print('\n'.join(last), flush=True)
    m = MISS.search(out)
    if not m:
        break
    pc, tgt = int(m.group(1), 16), int(m.group(2), 16)
    t = (tgt & 0x1FFFFFFF) | 0x80000000
    ok, why = boundary(t)
    if not ok:
        print('alvo %08X recusado: %s (revisar a mao)' % (t, why)); sys.exit(1)
    with open(a.config, 'a', encoding='utf-8', newline='\n') as f:
        f.write('\n[[functions]]\naddress = 0x%08X   # HH05 despacho em execucao: desvio indireto em %08X; %s\n' % (t, pc, why))
    print('+ funcao %08X (desvio em %08X; %s)' % (t, pc, why), flush=True)
    g = run(['build/translator/Release/hh-recomp.exe', a.config, '--game-data', a.game_data,
             '--analysis', 'analysis/hh04/disc1', '--out', a.generated])
    print(g.stdout.strip().splitlines()[-2] if g.stdout else g.stderr, flush=True)
    if g.returncode or ' fins mortos: 0 ' not in g.stdout or ' orfas: 0 ' not in g.stdout:
        print('invariantes quebradas; parar'); sys.exit(1)
    b = run(['cmake', '--build', '--preset', 'vs-release', '--target', 'hh-run', '--', '-m'])
    if b.returncode:
        print(b.stdout[-3000:]); sys.exit(1)
