#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
"""HH15.1: portao da promocao do tempo logico (--tempo experimental) a padrao da janela.

Modelo: N2b2 do Blue Stinger (o trabalho nao consome tempo logico, D11: as cargas ficam instantaneas, entao a linha do
tempo muda e se compara o conteudo e a ordem, nao o instante). Rota newgame_area1_nativo (4 700 VBlanks), provedores
nativos e saves nativos, estavel (--tempo nativo) x logico (--tempo experimental):

  T1  leituras: a sequencia de blocos (arquivo, deslocamento, SHA-256; HH_FILE_LOG) de um modo e prefixo identico da do
      outro (o logico, adiantado, le mais do stream da musica no fim);
  T2  quadros: as trocas de quadro (hash a cada VBlank) sao iguais e na mesma ordem ate o pulo do ultimo filme da rota,
      fora dos trechos da lista fechada config/hh15_excecoes_quadros.toml (o boot e a tela de carga, so no estavel), cada
      um exatamente como listado; qualquer outra diferenca, ou trecho listado que nao aparece, reprova;
  T3  audio: a sequencia de comandos ao driver de som (HH_AUDIO_CMDS, 16 bytes por pacote) e identica do boot ate depois
      do pulo do ultimo filme nos dois modos;
  T4  vivacidade: as duas execucoes terminam os 4 700 VBlanks (sem falta de progresso nem parada);
  T6  desempenho: o logico nao e mais lento que o estavel por mais de 5% (tempo de parede das execucoes).
  (T5, determinismo, e o estavel byte a byte sao das referencias da HH15.0: tools/dev/hh15_referencias.py.)
Negativo: com HH_ARQUIVOS_MUTACAO=corromper_bloco no logico, T1 tem de reprovar.

uso: hh15_tempo.py <hh-run.exe> <raiz do projeto>
ambiente: HH_DATA_DIR (sem ele = SKIP, rc 77).
"""
import os
import re
import shutil
import subprocess
import sys
import tempfile
import time

VBLANKS = 4700
RTC = 1700000000


# ---- comparadores (testados sem o jogo em hh15_tempo_test.py) ----

def frame_changes(stdout):
    """[(vblank, hash, leituras do controle)] de cada troca de quadro."""
    out, prev = [], None
    for l in stdout.splitlines():
        m = re.match(r'quadro (\d+): \S+ hash (\S+) leituras (\d+)', l)
        if m and m.group(2) != prev:
            out.append((int(m.group(1)), m.group(2), int(m.group(3))))
            prev = m.group(2)
    return out


def compare_frames(est, log, cut_est, cut_log, trechos, movie_est=None, movie_log=None):
    """T2. Diferencas ate os cortes so nos trechos da lista fechada (config/hh15_excecoes_quadros.toml), cada um
    exatamente como listado e todos presentes. Devolve (problemas, trocas iguais, trechos aplicados)."""
    import difflib
    a = [f for f in est if f[0] <= cut_est]
    b = [f for f in log if f[0] <= cut_log]
    sm = difflib.SequenceMatcher(None, [f[1] for f in a], [f[1] for f in b], autojunk=False)
    bad, applied, equal, seen = [], [], 0, set()
    ops = sm.get_opcodes()
    for n, (op, i1, i2, j1, j2) in enumerate(ops):
        if op == 'equal':
            equal += i2 - i1
            continue
        # fim: o lado que pulou o ultimo filme mais tarde tem mais quadros do filme (o indice do roteiro e a n-esima
        # leitura do controle, que cai noutro ponto do filme quando as cargas nao consomem tempo); so como ultimo
        # bloco, com o outro lado vazio, e depois de trocas iguais
        # (revisao Codex: so do lado cujo ultimo filme durou mais, dentro do intervalo do filme desse lado, e no maximo
        # metade da diferenca de duracao + 2 trocas; o filme troca de quadro a cada 2 VBlanks)
        if n == len(ops) - 1 and (i1 == i2 or j1 == j2) and n > 0 and ops[n - 1][0] == 'equal' and movie_est and movie_log:
            side, k0, k1, src, mv = ('estavel', i1, i2, a, movie_est) if j1 == j2 else ('logico', j1, j2, b, movie_log)
            dur_e, dur_l = movie_est[1] - movie_est[0], movie_log[1] - movie_log[0]
            longer = 'estavel' if dur_e > dur_l else 'logico'
            limit = abs(dur_e - dur_l) // 2 + 2
            inside = all(mv[0] <= f[0] <= mv[1] for f in src[k0:k1])
            if side == longer and inside and k1 - k0 <= limit:
                applied.append('fim do filme: %d trocas a mais no %s antes do pulo (V%d-V%d; limite %d)' %
                               (k1 - k0, side, src[k0][0], src[k1 - 1][0], limit))
                continue
        lo = a[i1][0] if i2 > i1 else None
        hi = a[i2 - 1][0] if i2 > i1 else None
        llo = b[j1][0] if j2 > j1 else None
        lhi = b[j2 - 1][0] if j2 > j1 else None
        hit = None
        for k, t in enumerate(trechos):  # os dois lados fixados (revisao Codex): trocas e VBlanks
            if (i2 - i1 == t['estavel_trocas'] and j2 - j1 == t['logico_trocas'] and lo == t['estavel_de'] and
                    hi == t['estavel_ate'] and llo == t.get('logico_de') and lhi == t.get('logico_ate')):
                hit = k
        if hit is None:
            where = 'estavel V%s-V%s (%d trocas)' % (lo, hi, i2 - i1) if i2 > i1 else 'estavel nada'
            there = 'logico V%d-V%d (%d trocas)' % (b[j1][0], b[j2 - 1][0], j2 - j1) if j2 > j1 else 'logico nada'
            bad.append('trecho fora da lista: %s, %s' % (where, there))
        else:
            seen.add(hit)
            applied.append('%s (estavel %d trocas, logico %d)' % (trechos[hit]['nome'], i2 - i1, j2 - j1))
    for k, t in enumerate(trechos):
        if k not in seen:
            bad.append('trecho listado nao apareceu: %s' % t['nome'])
    return bad, equal, applied


def read_blocks(path):
    rows = []
    for l in open(path, encoding='utf-8'):
        if l.startswith('seq') or not l.strip():
            continue
        p = l.rstrip('\n').split('\t')
        rows.append('\t'.join(p[2:]))  # sem a sequencia e o VBlank (a linha do tempo muda)
    return rows


def compare_prefix(a, b):
    """T1: a sequencia do estavel (a) e prefixo identico da do logico (b), que esta adiantado. Devolve problema ou ''."""
    if not a:
        return 'nenhuma leitura'
    if len(b) < len(a):
        return 'o logico leu menos blocos (%d) que o estavel (%d)' % (len(b), len(a))
    for k in range(len(a)):
        if a[k] != b[k]:
            return 'bloco %d diferente: %r != %r' % (k, a[k][:100], b[k][:100])
    return ''


def audio_cmds(path):
    out = []
    for l in open(path):
        v, pkt = l.rstrip('\n').split('\t')
        out.append((int(v), pkt))
    return out


def compare_audio(est, log, cut_est, cut_log):
    """T3: comandos iguais ate passar dos cortes nos dois modos. Devolve (problema, comandos iguais)."""
    n = 0
    while n < len(est) and n < len(log) and est[n][1] == log[n][1]:
        n += 1
    if n == 0:
        return 'nenhum comando igual', 0
    if est[n - 1][0] <= cut_est or log[n - 1][0] <= cut_log:  # iguais ate depois do corte (revisao Codex)
        k = min(n, len(est) - 1, len(log) - 1)
        return ('primeiro comando diferente antes do corte (estavel V%d %s, logico V%d %s)' %
                (est[k][0], est[k][1], log[k][0], log[k][1])), n
    return '', n


def last_skip(stderr):
    m = re.findall(r'\[video\] \S+: parado pelo jogo \(skip\) no VBlank (\d+)', stderr)
    return int(m[-1]) if m else None


def last_movie(stderr):
    """(inicio, pulo) do ultimo filme pulado pelo jogo."""
    name = re.findall(r'\[video\] (\S+): parado pelo jogo \(skip\) no VBlank (\d+)', stderr)
    if not name:
        return None
    start = re.findall(r'\[video\] %s: inicio no VBlank (\d+)' % re.escape(name[-1][0]), stderr)
    return (int(start[-1]), int(name[-1][1])) if start else None


# ---- portao ----

def main():
    run, root = (os.path.abspath(a) for a in sys.argv[1:3])
    data = os.environ.get('HH_DATA_DIR', '')
    if not os.path.isfile(os.path.join(data, 'disc.manifest')):
        print('SKIP: defina HH_DATA_DIR')
        return 77
    script = os.path.join(root, 'tools', 'dist', 'roteiros', 'newgame_area1_nativo.txt')
    tmp = tempfile.mkdtemp(prefix='hh15_tempo_')
    fails = 0

    def check(name, cond, info=''):
        nonlocal fails
        print('[%s] %s%s' % (' ok ' if cond else 'FAIL', name, (' (%s)' % info) if info else ''))
        fails += not cond

    def go(tag, mode, extra=None):
        saves = os.path.join(tmp, tag + '_saves')
        os.makedirs(saves)
        env = {k: v for k, v in os.environ.items() if not k.startswith(('HH_TEMPO_MUTACAO', 'HH_ARQUIVOS_MUTACAO',
                                                                         'HH_CICLOS_RUIDO', 'HH_CADENCIA'))}
        env.update(HH_INPUT_SCRIPT=script, HH_FILE_LOG=os.path.join(tmp, tag + '_leituras.tsv'),
                   HH_AUDIO_CMDS=os.path.join(tmp, tag + '_audio.tsv'), **(extra or {}))
        t0 = time.perf_counter()
        p = subprocess.run([run, '--data', data, '--vblanks', str(VBLANKS), '--rtc', str(RTC), '--every', '1',
                            '--arquivos', 'nativo', '--tempo', mode, '--controles', 'nativo', '--saves', 'nativo',
                            '--saves-pasta', saves, '--video', 'nativo', '--audio', 'nativo', '--render', 'nativo'],
                           capture_output=True, text=True, env=env)
        return p, time.perf_counter() - t0

    try:
        pe, te = go('estavel', 'nativo')
        pl, tl = go('logico', 'experimental')
        for tag, p in (('estavel', pe), ('logico', pl)):
            check('T4 %s: %d VBlanks sem parada' % (tag, VBLANKS), p.returncode == 0 and 'limite de VBlanks atingido' in p.stdout,
                  'rc %d' % p.returncode)
        cut_e, cut_l = last_skip(pe.stderr), last_skip(pl.stderr)
        check('corte: pulo do ultimo filme nos dois modos', cut_e is not None and cut_l is not None,
              'estavel V%s, logico V%s' % (cut_e, cut_l))
        if cut_e is None or cut_l is None:
            return 1
        bad = compare_prefix(read_blocks(os.path.join(tmp, 'estavel_leituras.tsv')),
                             read_blocks(os.path.join(tmp, 'logico_leituras.tsv')))
        check('T1 leituras: prefixo identico', not bad, bad)
        import tomllib
        with open(os.path.join(root, 'config', 'hh15_excecoes_quadros.toml'), 'rb') as f:
            trechos = tomllib.load(f)['trecho']
        bad, equal, applied = compare_frames(frame_changes(pe.stdout), frame_changes(pl.stdout), cut_e, cut_l, trechos,
                                             last_movie(pe.stderr), last_movie(pl.stderr))
        check('T2 quadros ate o pulo do ultimo filme', not bad, '%d trocas iguais; %s%s' %
              (equal, '; '.join(applied), ('; ' + bad[0]) if bad else ''))
        bad, n = compare_audio(audio_cmds(os.path.join(tmp, 'estavel_audio.tsv')),
                               audio_cmds(os.path.join(tmp, 'logico_audio.tsv')), cut_e, cut_l)
        check('T3 audio: comandos iguais ate depois do corte', not bad, '%d comandos%s' % (n, ('; ' + bad) if bad else ''))
        check('T6 desempenho: logico <= estavel x 1,05', tl <= te * 1.05, 'estavel %.1f s, logico %.1f s' % (te, tl))
        pn, _ = go('negativo', 'experimental', {'HH_ARQUIVOS_MUTACAO': 'corromper_bloco'})
        bad = compare_prefix(read_blocks(os.path.join(tmp, 'estavel_leituras.tsv')),
                             read_blocks(os.path.join(tmp, 'negativo_leituras.tsv')))
        check('negativo: bloco corrompido no logico reprova T1', bool(bad), bad[:80] if bad else 'nao acusou')
    finally:
        shutil.rmtree(tmp, ignore_errors=True)
    return 1 if fails else 0


if __name__ == '__main__':
    sys.exit(main())
