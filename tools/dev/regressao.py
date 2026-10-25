#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
"""HH06.6: regressao retomavel inicial (modelo: tools/dev/regressao.ps1 do Blue Stinger, MIT).

Roda as etapas em ordem. Cada etapa aprovada fica registrada com uma chave; rodar de novo
continua da primeira etapa nao aprovada ou cuja chave mudou. A chave cobre (revisao Codex HH06):
fontes versionadas, toolchain (compilador e gerador do cache do CMake), executaveis gerados
(etapas depois do build) e as entradas privadas que a etapa consome. Executavel ausente ou
entrada alterada muda a chave e a etapa roda de novo. Nunca enfraquece uma etapa para aprovar.

  python -I tools/dev/regressao.py                 continua
  python -I tools/dev/regressao.py --listar        estado das etapas
  python -I tools/dev/regressao.py --novo          descarta o estado e recomeca
  python -I tools/dev/regressao.py --desde NOME    refaz a partir da etapa NOME
  python -I tools/dev/regressao.py --gravar-area1  grava a referencia do roteiro (HH_AREA1_HASHES)
Estado: %LOCALAPPDATA%\\HeadHunterRecomp\\regressao\\estado.json (fora do repositorio).
Dados privados (sem eles as etapas que dependem deles falham com mensagem clara):
  HH_DATA_DIR      dados de execucao do disco 1 (hh-extract --runtime)
  HH_SNAPSHOT_DIR  snapshots do oraculo (portoes do HH05)
  HH_DISC1         imagem CDI do disco 1 (etapa do Builder)
  HH_AREA1_HASHES  hashes de referencia do roteiro newgame_area1
  HH07_BIN         1ST_READ.BIN do disco 1 (fechamento do HH07)
  HH07_SONDA       TSV da sonda do roteiro newgame_area1 (HH_HW_PROBE)"""
import hashlib
import json
import os
import subprocess
import sys
import time

ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), '..', '..'))
STATE = os.path.join(os.environ.get('LOCALAPPDATA', os.path.expanduser('~')), 'HeadHunterRecomp', 'regressao', 'estado.json')
SRC_DIRS = ['app', 'builder', 'cmake', 'config', 'runtime', 'tests', 'tools/hh-extract', 'tools/hh-diff', 'tools/dev',
            'tools/sh4-tablegen', 'tools/dist', 'tools/dcmap', 'translator', 'native', 'transitional', 'legacy',
            'CMakeLists.txt', 'CMakePresets.json']
ARTIFACTS = [('app', 'Release', 'hh-run.exe'), ('app', 'Release', 'HeadHunter.exe'), ('app', 'Release', 'hh-diff.exe'),
             ('builder', 'Release', 'hh-builder.exe'), ('translator', 'Release', 'hh-hwscan.exe')]
SCRIPT = os.path.join(ROOT, 'tools', 'dist', 'roteiros', 'newgame_area1.txt')
FRAMES = 94  # 4700 VBlanks, amostra a cada 50


def EXE(*p):
    return os.path.join(ROOT, 'build', *p)


def tree_hash(paths):
    h = hashlib.sha256()
    for p in paths:
        full = os.path.join(ROOT, p)
        if not os.path.exists(full):
            continue
        files = [full] if os.path.isfile(full) else sorted(
            os.path.join(d, f) for d, _, fs in os.walk(full) for f in fs if '__pycache__' not in d)
        for f in files:
            h.update(os.path.relpath(f, ROOT).replace(os.sep, '/').encode())
            with open(f, 'rb') as fh:
                h.update(hashlib.sha256(fh.read()).digest())
    return h.hexdigest()


def file_hash(path, sample=False):
    """SHA-256 do arquivo; sample=True (imagem de disco): tamanho, data e 1 MB do inicio e do fim."""
    if not path or not os.path.exists(path):
        return 'ausente:' + str(path)
    h = hashlib.sha256()
    if sample:
        st = os.stat(path)
        h.update(('%d:%d' % (st.st_size, int(st.st_mtime))).encode())
        with open(path, 'rb') as f:
            h.update(f.read(1 << 20))
            f.seek(max(0, st.st_size - (1 << 20)))
            h.update(f.read())
    else:
        with open(path, 'rb') as f:
            for chunk in iter(lambda: f.read(1 << 22), b''):
                h.update(chunk)
    return h.hexdigest()


def dir_stat(path):
    """Caminho, tamanho e data de cada arquivo da arvore: um arquivo de dados trocado sem mudar o
    manifesto invalida as etapas que o leem (revisao Codex HH07)."""
    h = hashlib.sha256()
    for d, _, fs in sorted(os.walk(path)):
        for f in sorted(fs):
            st = os.stat(os.path.join(d, f))
            rel = os.path.relpath(os.path.join(d, f), path).replace(os.sep, '/')
            h.update(('%s:%d:%d|' % (rel, st.st_size, st.st_mtime_ns)).encode())
    return h.hexdigest()


def toolchain():
    """Compilador e gerador do cache do CMake: mudar de toolchain invalida as etapas."""
    cache = EXE('CMakeCache.txt')
    if not os.path.exists(cache):
        return 'sem-cache'
    keep = [l for l in open(cache, encoding='utf-8', errors='replace')
            if l.startswith(('CMAKE_GENERATOR:', 'CMAKE_CXX_COMPILER:', 'CMAKE_GENERATOR_INSTANCE:', 'HH_GENERATED_DIR:'))]
    return hashlib.sha256(''.join(sorted(keep)).encode()).hexdigest()


def private_inputs(names):
    parts = []
    for n in names:
        v = os.environ.get(n, '')
        if n == 'HH_DATA_DIR':
            parts += [file_hash(os.path.join(v, f)) for f in ('disc.manifest', 'disc_meta.idx', 'disc_meta.bin')]
            parts.append(dir_stat(v))
        elif n == 'HH_SNAPSHOT_DIR':
            parts += [file_hash(os.path.join(v, f))
                      for f in ('boot_entry.bss', 'boot_entry.pvr', 'boot_flycast.tsv', 'fb_sega.vram', 'fb_sega.pvr')]
        elif n == 'HH_DISC1':
            parts.append(file_hash(v, sample=True))
        else:
            parts.append(file_hash(v))
    return parts


def sh(cmd, env=None, timeout=3600):
    print('  >', ' '.join(cmd), flush=True)
    r = subprocess.run(cmd, cwd=ROOT, env=dict(os.environ, **(env or {})), timeout=timeout,
                       capture_output=True, text=True, encoding='utf-8', errors='replace')
    for line in (r.stdout + r.stderr).strip().splitlines()[-12:]:
        print('    ' + line)
    return r


def need(var):
    v = os.environ.get(var)
    if not v:
        raise RuntimeError('defina %s (dado privado necessario para esta etapa)' % var)
    return v


def reference():
    ref = need('HH_AREA1_HASHES')
    if not os.path.exists(ref):
        raise RuntimeError('referencia ausente: gere com --gravar-area1')
    want = [l for l in open(ref).read().splitlines() if l]
    if len(want) != FRAMES:
        raise RuntimeError('referencia com %d quadros (esperado %d)' % (len(want), FRAMES))
    return want


def area1_hashes(exe, data):
    """Hashes do roteiro newgame_area1 ("<vblank>: <hash>"). hh-run escreve no stdout; o
    HeadHunter.exe (--headless) escreve no --log."""
    if not os.path.exists(exe):
        raise RuntimeError('executavel ausente: ' + exe)
    args = [exe, '--data', data, '--vblanks', '4700', '--rtc', '1700000000']
    log = None
    if os.path.basename(exe).lower() == 'headhunter.exe':
        log = EXE('regressao_headhunter.log')
        if os.path.exists(log):
            os.remove(log)
        args += ['--headless', '--log', log, '--hash-every', '50']
    else:
        args += ['--every', '50']
    r = sh(args, env={'HH_INPUT_SCRIPT': SCRIPT})
    if r.returncode != 0 or 'ok: limite de VBlanks atingido' not in r.stdout:
        raise RuntimeError('%s nao terminou pelo limite (rc=%d)' % (os.path.basename(exe), r.returncode))
    lines = open(log).read().splitlines() if log else r.stdout.splitlines()
    got = [' '.join(l.split()[1:5:3]) for l in lines if l.startswith('quadro ')]
    if len(got) != FRAMES:
        raise RuntimeError('esperados %d quadros amostrados, obtidos %d' % (FRAMES, len(got)))
    return got


def step_build():
    for args in (['cmake', '--preset', 'vs'], ['cmake', '--build', '--preset', 'vs-release', '--', '-m']):
        if sh(args).returncode != 0:
            raise RuntimeError('falhou: ' + ' '.join(args))
    for a in ARTIFACTS:
        if not os.path.exists(EXE(*a)):
            raise RuntimeError('o build nao produziu ' + EXE(*a))


def step_ctest():
    r = sh(['ctest', '--preset', 'vs-release', '--no-tests=error'], env={'HH_SNAPSHOT_DIR': '', 'HH_DATA_DIR': ''})
    if r.returncode != 0 or '100% tests passed' not in r.stdout:
        raise RuntimeError('ctest falhou')


def step_gates():
    need('HH_SNAPSHOT_DIR'), need('HH_DATA_DIR')
    r = sh(['ctest', '--preset', 'vs-release', '-R', 'hh_boot_regression|hh_first_frame', '--no-tests=error',
            '--output-on-failure'])
    passed = [n for n in ('hh_boot_regression', 'hh_first_frame')
              if any(n in l and 'Passed' in l for l in r.stdout.splitlines())]
    if r.returncode != 0 or 'Skipped' in r.stdout or len(passed) != 2:
        raise RuntimeError('portoes do HH05 falharam, foram ignorados ou nao rodaram')


def step_gates_hh07():
    # HH07: fechamento da fronteira + catraca (executavel e sonda privados) e ordem de eventos
    need('HH_DATA_DIR'), need('HH07_BIN'), need('HH07_SONDA')
    r = sh(['ctest', '--preset', 'vs-release', '-R', 'hh07_fechamento|hh07_ordem', '--no-tests=error',
            '--output-on-failure'])
    passed = [n for n in ('hh07_fechamento', 'hh07_ordem')
              if any(n in l and 'Passed' in l for l in r.stdout.splitlines())]
    if r.returncode != 0 or 'Skipped' in r.stdout or len(passed) != 2:
        raise RuntimeError('portoes do HH07 falharam, foram ignorados ou nao rodaram')


def step_area1():
    if area1_hashes(EXE('app', 'Release', 'hh-run.exe'), need('HH_DATA_DIR')) != reference():
        raise RuntimeError('hashes do roteiro newgame_area1 diferentes da referencia')


def step_builder():
    disc, want = need('HH_DISC1'), reference()
    out = os.path.join(ROOT, 'dist', 'regressao')
    if sh([EXE('builder', 'Release', 'hh-builder.exe'), '--disc', disc, '--out', out], timeout=7200).returncode != 0:
        raise RuntimeError('Builder falhou')
    # roteiro no executavel INSTALADO (HeadHunter.exe --headless), com os dados que vieram do CDI
    inst = os.path.join(out, 'HeadHunter')
    if area1_hashes(os.path.join(inst, 'HeadHunter.exe'), os.path.join(inst, 'data')) != want:
        raise RuntimeError('roteiro no HeadHunter.exe instalado difere da referencia')


# (nome, funcao, entradas privadas consumidas)
STEPS = [('build', step_build, []), ('ctest', step_ctest, []),
         ('portoes_hh05', step_gates, ['HH_SNAPSHOT_DIR', 'HH_DATA_DIR']),
         ('portoes_hh07', step_gates_hh07, ['HH_DATA_DIR', 'HH07_BIN', 'HH07_SONDA']),
         ('roteiro_area1', step_area1, ['HH_DATA_DIR', 'HH_AREA1_HASHES']),
         ('builder', step_builder, ['HH_DISC1', 'HH_AREA1_HASHES'])]


def step_key(name, private, src):
    parts = [name, src, toolchain()]
    if name != 'build':
        parts += [file_hash(EXE(*a)) for a in ARTIFACTS]
    parts += private_inputs(private)
    return hashlib.sha256('|'.join(parts).encode()).hexdigest()


def load():
    try:
        return json.load(open(STATE))
    except (OSError, ValueError):
        return {}


def save(st):
    os.makedirs(os.path.dirname(STATE), exist_ok=True)
    json.dump(st, open(STATE, 'w'), indent=1)


def main():
    st = load()
    if '--novo' in sys.argv:
        st = {}
        save(st)
    if '--gravar-area1' in sys.argv:
        ref = need('HH_AREA1_HASHES')
        open(ref, 'w').write('\n'.join(area1_hashes(EXE('app', 'Release', 'hh-run.exe'), need('HH_DATA_DIR'))) + '\n')
        print('referencia gravada em', ref)
        return 0
    if '--desde' in sys.argv:
        start = sys.argv[sys.argv.index('--desde') + 1]
        names = [n for n, _, _ in STEPS]
        for n in names[names.index(start):]:
            st.pop(n, None)
        save(st)
    src = tree_hash(SRC_DIRS)
    if '--listar' in sys.argv:
        for name, _, private in STEPS:
            s = st.get(name)
            ok = s and s.get('chave') == step_key(name, private, src)
            print('%-14s %s' % (name, ('aprovada em %s (%.0f s)' % (s['quando'], s['segundos'])) if ok else 'pendente'))
        return 0
    for name, fn, private in STEPS:
        s = st.get(name)
        # a chave e calculada na hora: executaveis refeitos pelo build mudam as etapas seguintes
        if s and s.get('chave') == step_key(name, private, src):
            print('[aprovada] %s' % name)
            continue
        print('[etapa] %s' % name, flush=True)
        t0 = time.time()
        try:
            fn()
        except (RuntimeError, subprocess.TimeoutExpired, OSError) as e:
            print('[FALHOU] %s: %s' % (name, e))
            print('rode de novo para continuar desta etapa')
            return 1
        st[name] = {'chave': step_key(name, private, src), 'quando': time.strftime('%Y-%m-%d %H:%M'),
                    'segundos': time.time() - t0}
        save(st)
        print('[ok] %s (%.0f s)' % (name, time.time() - t0))
    print('regressao completa')
    return 0


if __name__ == '__main__':
    sys.exit(main())
