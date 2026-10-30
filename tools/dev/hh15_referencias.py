#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
"""HH15.0: referencias pre-corte do caminho nativo (docs/planos/HH15-PLANO.md).

Cada caso roda o roteiro newgame_area1_nativo (4 700 VBlanks) com os provedores nativos e grava, por caso:
  quadros.txt  hashes de quadro a cada 50 VBlanks (stdout do hh-run);
  ordem.tsv    trace de ordem (HH_EVENT_TRACE) sem a coluna de ciclos: V, I (INTEVT, SPC), R, T, N, J em ordem;
  relogio.tsv  relogios consultaveis por VBlank (HH_CLOCK_LOG);
  ram.txt      SHA-256 da RAM principal nos VBlanks de RAM_VBLANKS, com a pilha morta de cada thread zerada (HH15.11:
               do R15 mais fundo visto na entrada de funcao, menos MARGEM_PILHA, ate o R15 atual, sem invadir a pilha
               viva de outra thread; pilhas_<v>.txt do hh-run; so a troca de pilha identificada, a do boot para a do
               jogo, e aceita, e a faixa e limitada a MAX_PILHA_MORTA). A pilha morta e lixo que nenhum codigo le, e uma
               funcao elevada nao reproduz o lixo da gerada (os despejos sao apagados);
  ram_completa.txt  o SHA-256 da RAM inteira (so evidencia: a recaptura das referencias prova que e o de antes);
  audio.txt    SHA-256 da saida do motor de audio (HH_AUDIO_WAV);
  resumo.txt   linhas de resumo do host ([tempo], [arquivos], [audio], [saves], parada).
Nada disso contem bytes do jogo: so hashes, enderecos e contagens (a pasta fica em analysis/, ignorada).

uso: hh15_referencias.py capturar <hh-run.exe> <pasta> [caso...]
     hh15_referencias.py conferir <hh-run.exe> <pasta> [--excecoes <toml>] [caso...]
       (roda de novo e compara artefato por artefato). Com --excecoes (HH15.1, cadencia B; regra do N8.1 do BS: o SPC
       nao e relaxado), o trace de ordem pode diferir so nas entregas listadas: mesma posicao, mesmo INTEVT, SPC da
       referencia e o novo exatamente como no arquivo, e o SPC novo entre os retornos justificados; todos os outros
       artefatos identicos; uma excecao listada que nao aparece tambem reprova.
ambiente: HH_DATA_DIR (obrigatorio). Variaveis de diagnostico como HH_CICLOS_RUIDO passam para o hh-run.
"""
import hashlib
import os
import re
import shutil
import subprocess
import sys
import tempfile

ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), '..', '..'))
SCRIPT = os.path.join(ROOT, 'tools', 'dist', 'roteiros', 'newgame_area1_nativo.txt')
VBLANKS = 4700
RTC = 1700000000
RAM_VBLANKS = (1000, 2000, 3000, 4000, 4700)
NATIVE = ['--arquivos', 'nativo', '--controles', 'nativo', '--video', 'nativo', '--audio', 'nativo', '--render', 'nativo']
# caso -> argumentos extras. 'saves' = saves nativos numa pasta nova (o padrao da janela).
CASES = {
    'estavel': ['--tempo', 'nativo', 'saves'],
    'experimental': ['--tempo', 'experimental'],
    # HH15.1: o caminho da janela no tempo logico (saves nativos); antes da espera 0x8C118346 parava no VBlank 2047
    'experimental_saves': ['--tempo', 'experimental', 'saves'],
}
ARTIFACTS = ('quadros.txt', 'ordem.tsv', 'relogio.tsv', 'ram.txt', 'audio.txt', 'resumo.txt')
ENV_DROP = ('HH_EVENT_TRACE', 'HH_CLOCK_LOG', 'HH_AUDIO_WAV', 'HH_RAM_DUMP', 'HH_HW_PROBE', 'HH_TEMPO_MUTACAO',
            'HH_TIME_TRACE', 'HH_PROGRESS_TRACE', 'HH_AUDIO_LOG', 'HH_ARQUIVOS_LOG', 'HH_BU_MUTACAO')


MARGEM_PILHA = 256  # o quadro de uma folha abaixo do R15 da entrada (o da conferencia das elevadas)
RAM_BASE, RAM_FIM = 0x8C000000, 0x8D000000
MAX_PILHA_MORTA = 64 << 10
PILHA_DO_BOOT = 0x8C010000  # a pilha do IP.BIN/boot fica abaixo do executavel (r15 de entrada 0x8C00F400)


def pilha_morta(pilhas):
    """faixas [lo, hi) de enderecos da pilha morta: de cada thread, do mais fundo (menos a margem) ao R15 atual, menos
    as pilhas vivas das outras ([sp, topo); a thread 0 ate o fim da RAM)."""
    ts = []
    for l in open(pilhas):
        t, topo, sp, mn, trocas, origem = (l.split() + ['0', '0'])[:6]  # o formato antigo nao tem as trocas
        t, trocas, origem = int(t), int(trocas), int(origem, 16)
        # so a troca identificada: a thread do jogo saindo da pilha do boot para a do jogo; qualquer outra descontinuidade
        # (salto de R15 acima de 4 KiB entre entradas de funcao) reprova a medicao em vez de mascarar
        if trocas and not (t == 0 and trocas == 1 and origem < PILHA_DO_BOOT):
            raise SystemExit('troca de pilha nao identificada na thread %d (%d trocas, ultima de %08X) em %s'
                             % (t, trocas, origem, pilhas))
        ts.append((t, int(topo, 16), int(sp, 16), int(mn, 16)))
    vivas = {t: (sp, topo if topo else RAM_FIM) for t, topo, sp, mn in ts}
    out = []
    for t, topo, sp, mn in ts:
        if sp - mn > MAX_PILHA_MORTA:  # a medicao nao delimitou a pilha: mascarar mais seria esconder estado
            raise SystemExit('pilha morta da thread %d grande demais (%X-%X) em %s' % (t, mn, sp, pilhas))
        faixas = [(max(mn - MARGEM_PILHA, RAM_BASE), sp)]
        for u, (lo, hi) in vivas.items():
            if u == t:
                continue
            novas = []
            for a, b in faixas:
                if b <= lo or a >= hi:
                    novas.append((a, b))
                    continue
                if a < lo:
                    novas.append((a, lo))
                if b > hi:
                    novas.append((hi, b))
            faixas = novas
        out += [(a, b) for a, b in faixas if a < b]
    return out


def sha_ram(path, pilhas):
    data = bytearray(open(path, 'rb').read())
    if os.path.isfile(pilhas):
        for a, b in pilha_morta(pilhas):
            data[a - RAM_BASE:b - RAM_BASE] = bytes(b - a)
    return hashlib.sha256(bytes(data)).hexdigest()


def sha(path):
    h = hashlib.sha256()
    with open(path, 'rb') as f:
        for b in iter(lambda: f.read(1 << 20), b''):
            h.update(b)
    return h.hexdigest()


def run_case(run, case, out):
    os.makedirs(out, exist_ok=True)
    tmp = tempfile.mkdtemp(prefix='hh15_ref_')
    try:
        args = [run, '--data', os.environ['HH_DATA_DIR'], '--vblanks', str(VBLANKS), '--rtc', str(RTC), '--every', '50']
        args += NATIVE
        for a in CASES[case]:
            if a == 'saves':
                saves = os.path.join(tmp, 'saves')
                os.makedirs(saves)
                args += ['--saves', 'nativo', '--saves-pasta', saves]
            else:
                args.append(a)
        ev, ck, wav = (os.path.join(tmp, n) for n in ('ev.tsv', 'ck.tsv', 'audio.wav'))
        env = {k: v for k, v in os.environ.items() if k not in ENV_DROP}
        env.update(HH_INPUT_SCRIPT=SCRIPT, HH_EVENT_TRACE=ev, HH_CLOCK_LOG=ck, HH_AUDIO_WAV=wav,
                   HH_RAM_DUMP=','.join(map(str, RAM_VBLANKS)) + ';' + tmp)
        p = subprocess.run(args, capture_output=True, text=True, env=env)
        text = (p.stdout + p.stderr).replace(tmp, '<tmp>')
        w = lambda name, s: open(os.path.join(out, name), 'w', newline='\n').write(s)
        w('quadros.txt', ''.join(l + '\n' for l in p.stdout.splitlines() if l.startswith('quadro ')))
        order = []
        if os.path.isfile(ev):
            for l in open(ev):
                f = l.rstrip('\n').split('\t')
                if len(f) == 5 and f[0].isdigit():
                    order.append('\t'.join((f[2], f[3], f[4])))
        w('ordem.tsv', ''.join(l + '\n' for l in order))
        w('relogio.tsv', open(ck).read() if os.path.isfile(ck) else '')
        ram = lambda v: os.path.join(tmp, 'ram_%d.bin' % v)
        w('ram.txt', ''.join('%d %s\n' % (v, sha_ram(ram(v), os.path.join(tmp, 'pilhas_%d.txt' % v))
                                           if os.path.isfile(ram(v)) else 'ausente') for v in RAM_VBLANKS))
        w('ram_completa.txt', ''.join('%d %s\n' % (v, sha(ram(v)) if os.path.isfile(ram(v)) else 'ausente')
                                      for v in RAM_VBLANKS))
        w('audio.txt', (sha(wav) if os.path.isfile(wav) else 'ausente') + '\n')
        keep = re.compile(r'^(\[(tempo|arquivos|audio|saves|video|render)\]|PARADA|ok:|vblanks:)')
        w('resumo.txt', 'rc %d\n' % p.returncode + ''.join(l + '\n' for l in text.splitlines() if keep.match(l)))
        return p.returncode
    finally:
        shutil.rmtree(tmp, ignore_errors=True)


def first_diff(a, b):
    la, lb = a.splitlines(), b.splitlines()
    for i, (x, y) in enumerate(zip(la, lb)):
        if x != y:
            return 'linha %d: %r != %r' % (i + 1, x[:120], y[:120])
    return 'tamanho %d != %d linhas' % (len(la), len(lb))


def order_with_exceptions(case, ref, new, exc):
    """Diferencas do trace de ordem fora da lista fechada (lista vazia = aceito)."""
    a, b = ref.splitlines(), new.splitlines()
    if len(a) != len(b):
        return ['tamanho %d != %d linhas (deslocamento de posicao nao tem excecao)' % (len(a), len(b))]
    allowed = {int(r['spc'], 16) for r in exc.get('retornos', [])}
    from collections import Counter  # cada entrada vale para uma unica diferenca (multiplicidade, revisao Codex)
    listed = Counter((e['vblank'], int(e['intevt'], 16), int(e['spc_ref'], 16), int(e['spc_novo'], 16))
                     for e in exc.get('excecao', []) if e['caso'] == case)
    seen, bad, vblank = Counter(), [], 0
    for i, (x, y) in enumerate(zip(a, b)):
        if x.startswith('V\t'):
            vblank = int(x.split('\t')[1], 16)
        if x == y:
            continue
        fx, fy = x.split('\t'), y.split('\t')
        if fx[0] != 'I' or fy[0] != 'I' or fx[1] != fy[1]:
            bad.append('linha %d: %r != %r (so o SPC de uma entrega pode mudar)' % (i + 1, x, y))
            continue
        key = (vblank, int(fx[1], 16), int(fx[2], 16), int(fy[2], 16))
        if seen[key] >= listed[key]:
            bad.append('linha %d: entrega %03X no VBlank %d, SPC %s -> %s fora da lista' % (i + 1, key[1], vblank, fx[2], fy[2]))
        elif key[3] not in allowed:
            bad.append('linha %d: SPC novo %s sem justificativa em [[retornos]]' % (i + 1, fy[2]))
        seen[key] += 1
    for k in sorted((listed - seen).elements()):
        bad.append('excecao listada nao apareceu: VBlank %d, %03X, %08X -> %08X' % k)
    return bad


def main():
    if len(sys.argv) < 4 or sys.argv[1] not in ('capturar', 'conferir'):
        print(__doc__)
        return 2
    if not os.path.isfile(os.path.join(os.environ.get('HH_DATA_DIR', ''), 'disc.manifest')):
        print('ERRO: defina HH_DATA_DIR')
        return 2
    mode, run, base = sys.argv[1], os.path.abspath(sys.argv[2]), os.path.abspath(sys.argv[3])
    rest, exc = sys.argv[4:], None
    if '--excecoes' in rest:
        k = rest.index('--excecoes')
        import tomllib
        with open(rest[k + 1], 'rb') as f:
            exc = tomllib.load(f)
        rest = rest[:k] + rest[k + 2:]
    cases = rest or list(CASES)
    fails = 0
    for case in cases:
        if mode == 'capturar':
            rc = run_case(run, case, os.path.join(base, case))
            print('[%s] %s capturado (rc %d)' % ('ok' if rc == 0 else 'FALHA', case, rc))
            fails += rc != 0
            continue
        new = tempfile.mkdtemp(prefix='hh15_conf_')
        try:
            run_case(run, case, new)
            for a in ARTIFACTS:
                ra = open(os.path.join(base, case, a)).read()
                rb = open(os.path.join(new, a)).read()
                if exc is not None and a == 'ordem.tsv':  # tambem com o trace igual: excecao listada e nao vista reprova
                    bad = order_with_exceptions(case, ra, rb, exc)
                    fails += bool(bad)
                    n = sum(1 for x, y in zip(ra.splitlines(), rb.splitlines()) if x != y)
                    print('[%s] %s %s: %d entregas com SPC diferente%s' % ('ok' if not bad else 'DIFERENTE', case, a, n,
                          ', todas na lista fechada' if not bad else ': ' + '; '.join(bad[:5])))
                    continue
                same = ra == rb
                fails += not same
                print('[%s] %s %s%s' % ('ok' if same else 'DIFERENTE', case, a, '' if same else ': ' + first_diff(ra, rb)))
        finally:
            shutil.rmtree(new, ignore_errors=True)
    return 1 if fails else 0


if __name__ == '__main__':
    sys.exit(main())
