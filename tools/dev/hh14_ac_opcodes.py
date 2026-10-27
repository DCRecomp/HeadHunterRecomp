#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
"""HH14: opcode do driver que cada funcao ac* do Audio64 envia (docs/planos/HH14-PLANO.md, fase A). Desmonta cada
funcao pelo C++ gerado (tools/dev/hh_dis.py) e procura o literal mov.w gravado no pacote da pilha (r15) que esta no
conjunto tratado pelo AUDIO64.DRV (0x30-0x3E, 0x80-0xAF). Imprime so enderecos, nomes e opcodes.

  python -I tools/dev/hh14_ac_opcodes.py <generated/disc1> <1ST_READ.BIN>
"""
import os
import re
import struct
import subprocess
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
GEN, BIN = sys.argv[1], sys.argv[2]
FUNCS = {
    0x8C120534: 'acSystemSetMasterVolume', 0x8C120610: 'acDigiGetCurrentReadPosition', 0x8C120AE8: 'acSystemSetStereoOrMono',
    0x8C120B38: 'acDigiSetLoopPoints', 0x8C120BCA: 'acDigiPlay', 0x8C120DC6: 'acDigiPlayWithParameters',
    0x8C121108: 'acSetMultiPlayMask', 0x8C121156: 'acDigiMultiPlay', 0x8C121270: 'acDigiMultiStop', 0x8C1212FC: 'acDigiOpen',
    0x8C121464: 'acDigiSetSampleRate', 0x8C12155A: 'acDigiSetQSoundAngle', 0x8C1215C4: 'acDspInstallProgram',
    0x8C121672: 'acDspInstallOutputMixer', 0x8C1216F8: 'acDspSetMixerChannel', 0x8C12177A: 'acDigiClose',
    0x8C1217D4: 'acDigiSetCurrentPitch', 0x8C121916: 'acDigiSetVolume', 0x8C1219A8: 'acDigiSetPan',
    0x8C121A12: 'acDigiRequestEvent', 0x8C121A82: 'acDigiStop', 0x8C122888: 'acIntSetMessagingMode', 0x8C122974: 'acIntInit',
    0x8C122E78: 'acSystemInit', 0x8C123088: 'acSystemInstallDriver', 0x8C123234: 'acSetTransferMode', 0x8C1232FE: 'acTransfer',
    0x8C120FBC: '(0x8C120FBC)',
}
raw = open(BIN, 'rb').read()


def w(a):
    return struct.unpack_from('<H', raw, a - 0x8C010000)[0]


for addr, name in sorted(FUNCS.items()):
    out = subprocess.run([sys.executable, '-I', ROOT + '/tools/dev/hh_dis.py', GEN, BIN, '%08X' % addr,
                          '--max', '260'], capture_output=True, text=True).stdout.splitlines()
    regs = {}
    ops = []
    calls = set()
    for l in out:
        m = re.search(r'mov\.w 0x([0-9a-f]+),r(\d+)', l)
        if m:
            regs[m.group(2)] = w(int(m.group(1), 16))
        m = re.search(r'mov\.w r(\d+),@(\(\d+,)?r15\)?', l)
        if m and m.group(1) in regs and (0x30 <= regs[m.group(1)] <= 0x3E or 0x80 <= regs[m.group(1)] <= 0xAF):
            ops.append(regs[m.group(1)])
        m = re.search(r'=0x(8C12[0-9A-F]{4})', l)
        if m:
            calls.add(m.group(1))
        m = re.search(r'bsr 0x(8c12[0-9a-f]{4})', l)
        if m:
            calls.add(m.group(1).upper())
    print('%08X %-30s opcodes %-16s chama %s' % (addr, name, ','.join('0x%02X' % o for o in ops) or '-',
                                                 ' '.join(sorted(calls))))
