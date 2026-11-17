"""Monitor de campanha do Head Hunter (HH16.1, docs/planos/HH16-PLANO.md). RASCUNHO de 2026-11-14: escrito sem executar,
durante a bateria do Blue Stinger (nada pesado na maquina). Modelo: tools/dev/campanha.py e docs/CAMPANHA.md do Blue
Stinger (MIT), adaptado ao HH:

- o indice da gravacao e a LEITURA do controle (o HH nao tem passos de jogo; HeadHunter --gravar/--reproduzir, HH16.0);
- o estado comparado, enquanto nao houver o estado por passo: os hashes de quadro do --log (a cada K VBlanks), os saves de
  fim (bytes) e a ultima leitura; a reproducao e headless, no tempo logico (o --reproduzir forca), no software;
- tudo fica em <workspace>/campanhas/<id> (ignorado pelo git; regra: nada fora da pasta de trabalho); o programa e
  copiado na criacao (fixado); os dados ficam na instalacao, fixados pelo SHA-256 do data/arquivos.manifest;
- nada aqui apaga ou reescreve uma sessao gravada; selos SHA-256 de cada arquivo no manifesto.

Uso (planejado):
  python -I tools/dev/campanha.py nova [--instalacao <pasta>] [--teste] [--copia <pasta>]
  python -I tools/dev/campanha.py jogar [<id>]          (copia de seguranca automatica ao fim de cada sessao)
  python -I tools/dev/campanha.py copiar [<id>] [--para <pasta>]
  python -I tools/dev/campanha.py status [<id>]
  python -I tools/dev/campanha.py verificar [<id>] [--sessao N]
  python -I tools/dev/campanha.py reproduzir-em <id> --instalacao <pasta>
  python -I tools/dev/campanha.py lista

Feito: cadeia de saves entre sessoes, creditos (fim natural do SCENE84 e GAMEEND.AHF na reproducao), estado por leitura
(--estado), selos, copia de seguranca automatica, testes com programa falso (tests/campanha/test_campanha.py). Pendente
antes de valer (HH16.P): a sessao piloto na janela; depois, a janela simples e a recuperacao de queda do proprio monitor.
"""
import argparse
import datetime
import hashlib
import json
import os
import shutil
import subprocess
import sys
import time

RAIZ = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
# Ganchos de teste (tests/campanha/test_campanha.py): HH_CAMPANHAS troca a pasta das campanhas; HH_CAMPANHA_PROGRAMA e um
# script Python executado no lugar do HeadHunter.exe (mesmos argumentos), para testar o monitor sem janela nem jogo.
CAMPANHAS = os.environ.get('HH_CAMPANHAS') or os.path.join(RAIZ, 'campanhas')
PROGRAMA_FALSO = os.environ.get('HH_CAMPANHA_PROGRAMA')
# instalacao padrao de uma campanha nova: a Preview do Builder (HH16.P, P6), ou HH_CAMPANHA_INSTALACAO
DIST = os.environ.get('HH_CAMPANHA_INSTALACAO') or os.path.join(RAIZ, 'dist', 'HeadHunter-Preview')
HASH_A_CADA = 60          # VBlanks entre hashes no --log (1 s de jogo)
ESTADO_A_CADA = 60        # leituras do controle entre hashes do estado (--estado, HH16.0)
VERSAO_FORMATO = 1        # manifesto versionado: leitores futuros aceitam as versoes antigas
# Creditos (docs/planos/HH16-PREPARACAO.md, secao 2): o S92_MOVIE.ASB toca o SCENE84 e chama o comando 229 "GameEnd"
# (0x8C025F66 -> 0x8C095444, que carrega AHFDATA/GAMEEND.AHF; medido em 2026-11-14: fim natural do SCENE84 no V12002,
# GAMEEND.AHF no V12003, a musica dos creditos no V12006). Os dois sinais sao exigidos: o fim natural no stderr do
# provedor nativo de filmes e a abertura do pacote no log de arquivos (HH_FILE_LOG) da reproducao.
CREDITOS = '[video] MOVIES/SCENE84.4XM: fim natural'
GAMEEND = 'AHFDATA/GAMEEND.AHF'


VEREDITOS_BONS = ('REPRODUZIDA', 'REPRODUZIDA_ATE_A_QUEDA')


def ler_resultado(saida):
    """veredito gravado pela reproducao em <saida>/resultado.json (None sem ele)"""
    try:
        with open(os.path.join(saida, 'resultado.json'), encoding='utf-8') as f:
            return json.load(f).get('veredito')
    except (OSError, ValueError):
        return None


def _falso(args, kw):
    """no teste, o programa falso recebe os mesmos argumentos e o executavel que o monitor chamaria (HH_CAMPANHA_EXE)"""
    if PROGRAMA_FALSO:
        kw['env'] = dict(kw.get('env') or os.environ, HH_CAMPANHA_EXE=args[0])
        # os ganchos do proprio teste (FAKE_*, CAMPANHA_TESTE_*) passam mesmo pelo ambiente filtrado
        kw['env'].update({k: v for k, v in os.environ.items() if k.startswith(('FAKE_', 'CAMPANHA_TESTE_'))})
        args = [sys.executable, '-I', PROGRAMA_FALSO] + list(args[1:])
    return args, kw


def executar(args, **kw):
    args, kw = _falso(args, kw)
    return subprocess.run(args, **kw)


def _info_processo(pid):
    """(vivo, momento de criacao, executavel) do processo `pid`; no Windows por OpenProcess/GetProcessTimes/
    QueryFullProcessImageNameW. O momento de criacao distingue um PID reaproveitado depois de uma queda (revisao Codex
    HH16.P3)."""
    if not pid:
        return False, None, None
    if os.name != 'nt':
        try:
            os.kill(int(pid), 0)
            return True, None, None
        except OSError:
            return False, None, None
    import ctypes
    from ctypes import wintypes
    k = ctypes.windll.kernel32
    k.OpenProcess.restype = wintypes.HANDLE
    h = k.OpenProcess(0x1000, False, int(pid))  # PROCESS_QUERY_LIMITED_INFORMATION
    if not h:
        return False, None, None
    try:
        codigo = wintypes.DWORD()
        if not k.GetExitCodeProcess(h, ctypes.byref(codigo)) or codigo.value != 259:  # STILL_ACTIVE
            return False, None, None
        t = [wintypes.FILETIME() for _ in range(4)]
        criado = None
        if k.GetProcessTimes(h, *[ctypes.byref(x) for x in t]):
            criado = (t[0].dwHighDateTime << 32) | t[0].dwLowDateTime
        buf = ctypes.create_unicode_buffer(32768)
        n = wintypes.DWORD(len(buf))
        exe = buf.value if k.QueryFullProcessImageNameW(h, 0, buf, ctypes.byref(n)) else None
        return True, criado, exe
    finally:
        k.CloseHandle(h)


def identidade(pid):
    """'pid:criacao' do processo vivo `pid` (o que a trava e o manifesto guardam)"""
    vivo_, criado, _ = _info_processo(pid)
    return '%d:%s' % (pid, criado if criado is not None else '') if vivo_ else None


def ambiente_reproducao(**extra):
    """ambiente das reproducoes: sem as variaveis HH_ do terminal (um HH_INPUT_SCRIPT ou uma mutacao de desenvolvimento
    reprovaria uma sessao valida; revisao Codex HH16.P3), mais as do proprio monitor (HH_FILE_LOG)"""
    env = {k: v for k, v in os.environ.items() if not k.upper().startswith('HH_')}
    env.update(extra)
    return env


def vivo(ident):
    """o processo da identidade 'pid:criacao' (ou de um PID antigo, so o numero) ainda e o mesmo e esta vivo"""
    if not ident:
        return False
    pid, _, criado = str(ident).partition(':')
    try:
        vivo_, agora_criado, _ = _info_processo(int(pid))
    except ValueError:
        return False
    return vivo_ and (not criado or agora_criado is None or str(agora_criado) == criado)


def processos_com_exe(exe):
    """PIDs vivos que executam `exe` (o programa fixado de uma campanha e unico): cobre o intervalo entre abrir o jogo e
    gravar o PID dele no manifesto, se o monitor cair ali (revisao Codex HH16.P3)"""
    if os.name != 'nt':
        return []
    import ctypes
    from ctypes import wintypes
    pids = (wintypes.DWORD * 65536)()
    usado = wintypes.DWORD()
    if not ctypes.windll.psapi.EnumProcesses(pids, ctypes.sizeof(pids), ctypes.byref(usado)):
        return []
    alvo = os.path.normcase(os.path.abspath(exe))
    out = []
    for pid in pids[:usado.value // ctypes.sizeof(wintypes.DWORD)]:
        v, _, e = _info_processo(pid)
        if v and e and os.path.normcase(e) == alvo:
            out.append(pid)
    return out


class Trava:
    """uma campanha aberta por um monitor de cada vez; a trava de um monitor que caiu e reconhecida e substituida"""

    def __init__(self, cid):
        self.p = os.path.join(CAMPANHAS, cid, '.trava')
        self.cid = cid

    def __enter__(self):
        for _ in range(3):
            try:
                fd = os.open(self.p, os.O_CREAT | os.O_EXCL | os.O_WRONLY)
                os.write(fd, (identidade(os.getpid()) or str(os.getpid())).encode('ascii'))
                os.close(fd)
                return self
            except FileExistsError:
                try:
                    with open(self.p, encoding='ascii') as f:
                        dono = f.read().strip()
                except OSError:
                    dono = ''
                if vivo(dono):
                    sys.exit('a campanha %s ja esta aberta em outro monitor (processo %s); feche-o antes'
                             % (self.cid, dono.partition(':')[0]))
                os.replace(self.p, self.p + '.antiga')  # o monitor anterior caiu: a trava dele fica guardada
        sys.exit('nao foi possivel travar a campanha %s' % self.cid)

    def __exit__(self, *exc):
        try:
            os.remove(self.p)  # so a propria trava (um arquivo), nunca uma pasta
        except OSError:
            pass


def sha256(p):
    h = hashlib.sha256()
    with open(p, 'rb') as f:
        for b in iter(lambda: f.read(1 << 20), b''):
            h.update(b)
    return h.hexdigest()


def agora():
    return datetime.datetime.now().isoformat(timespec='seconds')


def carregar(cid):
    with open(os.path.join(CAMPANHAS, cid, 'campanha.json'), encoding='utf-8') as f:
        m = json.load(f)
    if m.get('versao', 0) > VERSAO_FORMATO:
        sys.exit('campanha %s em formato %d, mais novo que este monitor (%d)' % (cid, m['versao'], VERSAO_FORMATO))
    return m


def gravar_manifesto(cid, m):
    p = os.path.join(CAMPANHAS, cid, 'campanha.json')
    tmp = p + '.novo'
    with open(tmp, 'w', encoding='utf-8') as f:
        json.dump(m, f, indent=1, ensure_ascii=False)
        f.flush()
        os.fsync(f.fileno())
    os.replace(tmp, p)  # escrita atomica


def selar(cid, m, rel):
    m.setdefault('selos', {})[rel] = sha256(os.path.join(CAMPANHAS, cid, rel))


def conferir_selos(cid, m):
    ruins = [r for r, h in m.get('selos', {}).items()
             if not os.path.exists(os.path.join(CAMPANHAS, cid, r)) or sha256(os.path.join(CAMPANHAS, cid, r)) != h]
    return ruins


def ultima(cid=None):
    if cid:
        return cid
    ids = sorted(d for d in os.listdir(CAMPANHAS) if os.path.isfile(os.path.join(CAMPANHAS, d, 'campanha.json')))
    if not ids:
        sys.exit('nenhuma campanha; use "nova"')
    return ids[-1]


def cmd_nova(a):
    inst = os.path.abspath(a.instalacao or DIST)
    exe = os.path.join(inst, 'HeadHunter.exe')
    manifesto_dados = os.path.join(inst, 'data', 'arquivos.manifest')
    for p in (exe, manifesto_dados):
        if not os.path.exists(p):
            sys.exit('instalacao incompleta: falta %s' % p)
    # o programa fixado precisa do contrato de fim da gravacao (HH16.1, 2026-11-15): sem ele a reproducao nao para onde
    # a sessao parou e toda sessao sairia REPROVADA. Um programa anterior e recusado na criacao, nao depois de jogar.
    # A prova e por comportamento (revisao Codex HH16.P3: procurar textos no executavel nao prova nada): uma gravacao
    # minima fechada na leitura 5, reproduzida sem janela, tem de parar ali, com o estado e a ultima leitura informados.
    falha = provar_contrato(exe, os.path.join(inst, 'data'))
    if falha:
        sys.exit('o programa %s nao cumpre o contrato da gravacao (%s); refaca a instalacao pelo Builder' % (exe, falha))
    cid = time.strftime('%Y%m%d-%H%M%S') + ('-teste' if a.teste else '')
    base = os.path.join(CAMPANHAS, cid)
    os.makedirs(os.path.join(base, 'programa'))
    for n in os.listdir(inst):  # o programa fixado: executavel e DLLs (os dados ficam na instalacao)
        if n.lower().endswith(('.exe', '.dll')):
            shutil.copy2(os.path.join(inst, n), os.path.join(base, 'programa', n))
    m = {'versao': VERSAO_FORMATO, 'id': cid, 'teste': bool(a.teste), 'criada': agora(), 'instalacao': inst,
         'dados_sha256': sha256(manifesto_dados), 'sessoes': [], 'selos': {}}
    if a.copia:  # pasta da copia de seguranca depois de cada sessao (padrao: campanhas/_copias)
        m['copia'] = os.path.abspath(a.copia)
    for n in sorted(os.listdir(os.path.join(base, 'programa'))):
        selar(cid, m, os.path.join('programa', n))
    gravar_manifesto(cid, m)
    print('campanha %s criada (programa fixado de %s)' % (cid, inst))


def pasta_sessao(cid, n):
    return os.path.join(CAMPANHAS, cid, 'sessao_%03d' % n)


def fechar_sessao(cid, m, s, rc, recuperada=False):
    """sela o que a sessao deixou e marca gravada (rc 0) ou interrompida; usada no fim normal e na recuperacao"""
    ps = pasta_sessao(cid, s['n'])
    s['fim'], s['rc'] = agora(), rc
    s['situacao'] = 'gravada' if rc == 0 else 'interrompida'
    if recuperada:
        s['recuperada'] = True  # o monitor caiu durante a sessao; o que foi gravado ate ali vale
    s.pop('pid', None)
    for rel in ('entrada.txt', 'quadros.txt', 'estado.txt'):
        if os.path.exists(os.path.join(ps, rel)):
            selar(cid, m, os.path.relpath(os.path.join(ps, rel), os.path.join(CAMPANHAS, cid)))
    for f in arvore(os.path.join(ps, 'saves')):
        selar(cid, m, os.path.relpath(os.path.join(ps, 'saves', f), os.path.join(CAMPANHAS, cid)))


def recuperar(cid, m):
    """uma sessao que ficou 'gravando' (o monitor caiu): espera o jogo dela fechar e a marca interrompida"""
    s = m['sessoes'][-1] if m['sessoes'] else None
    if not s or s['situacao'] != 'gravando':
        return m
    aberto = vivo(s.get('pid'))
    if not aberto and not PROGRAMA_FALSO:  # o monitor pode ter caido antes de gravar o PID do jogo
        aberto = bool(processos_com_exe(os.path.join(CAMPANHAS, cid, 'programa', 'HeadHunter.exe')))
    if aberto:
        sys.exit('o jogo da sessao %d ainda esta aberto: feche a janela dele e tente de novo' % s['n'])
    # o jogo fechou sozinho depois da queda do monitor: se a gravacao tem o comentario de fim, a sessao terminou
    # normalmente e vale inteira (revisao Codex HH16.P3); senao, ate a queda
    entrada = os.path.join(pasta_sessao(cid, s['n']), 'entrada.txt')
    fechada = os.path.exists(entrada) and ultima_leitura(entrada) is not None
    fechar_sessao(cid, m, s, 0 if fechada else None, recuperada=True)
    gravar_manifesto(cid, m)
    print('sessao %d recuperada: o monitor caiu durante ela; %s' % (
        s['n'], 'o jogo fechou normalmente e a sessao vale inteira' if fechada else 'vale o que foi gravado ate ali'))
    return m


# variaveis de verificacao que nunca entram numa campanha real (o produto ja recusa as de mutacao; o roteiro e a maior)
RECUSADAS = ('HH_INPUT_SCRIPT',)


def existe_campanha():
    return os.path.isdir(CAMPANHAS) and any(os.path.isfile(os.path.join(CAMPANHAS, d, 'campanha.json'))
                                            for d in os.listdir(CAMPANHAS))


def campanhas_reais(teste=False):
    """campanhas reais (ou, com teste=True, so as de teste), da mais antiga a mais nova"""
    return sorted(d for d in (os.listdir(CAMPANHAS) if os.path.isdir(CAMPANHAS) else [])
                  if os.path.isfile(os.path.join(CAMPANHAS, d, 'campanha.json')) and carregar(d)['teste'] == bool(teste))


def cmd_jogar(a):
    cid = a.id
    if a.criar and not cid:
        # atalho e janela: continuam a campanha REAL (uma campanha de teste, como a sessao piloto, nunca e continuada por
        # engano); sem campanha real, a primeira sessao cria uma. Com --teste (a sessao piloto pelo gravador), o mesmo
        # com as campanhas de teste: nunca continua a real.
        reais = campanhas_reais(a.teste)
        if not reais:
            cmd_nova(argparse.Namespace(instalacao=None, teste=a.teste, copia=os.environ.get('HH_CAMPANHA_COPIA')))
            reais = campanhas_reais(a.teste)
        cid = reais[-1]
    elif a.teste:
        sys.exit('jogar --teste so vale com --criar (sem id)')
    cid = ultima(cid)
    with Trava(cid):
        return jogar(cid)


def jogar(cid):
    m = recuperar(cid, carregar(cid))
    ruins = [v for v in RECUSADAS if os.environ.get(v)]
    if ruins and not m['teste']:
        sys.exit('campanha real recusa roteiro de teste (%s definida); use uma campanha "nova --teste"' % ', '.join(ruins))
    if conferir_selos(cid, m):
        sys.exit('campanha %s alterada depois da gravacao: %s' % (cid, conferir_selos(cid, m)))
    if sha256(os.path.join(m['instalacao'], 'data', 'arquivos.manifest')) != m['dados_sha256']:
        sys.exit('os dados da instalacao mudaram desde a criacao: reinstale os mesmos dados ou comece outra campanha')
    n = len(m['sessoes']) + 1
    ps = pasta_sessao(cid, n)
    os.makedirs(os.path.join(ps, 'saves'))
    os.makedirs(os.path.join(ps, 'saves_inicio'))
    if n > 1:  # a sessao continua dos saves REPRODUZIDOS da anterior; sem reproducao aprovada, dos gravados
        ant = pasta_sessao(cid, n - 1)
        orig = os.path.join(ant, 'reproducao', 'saves')
        # revisao Codex HH16.P2: uma reproducao reprovada nunca alimenta a sessao seguinte (a cadeia acusa depois)
        if not os.path.isdir(orig) or ler_resultado(os.path.join(ant, 'reproducao')) not in VEREDITOS_BONS:
            orig = os.path.join(ant, 'saves')
        shutil.copytree(orig, os.path.join(ps, 'saves_inicio'), dirs_exist_ok=True)  # com as subpastas (card0)
        shutil.copytree(orig, os.path.join(ps, 'saves'), dirs_exist_ok=True)
    rtc = int(time.time())
    # render da janela: o padrao (Vulkan); os quadros dela nao se comparam bit a bit com a reproducao em software
    s = {'n': n, 'inicio': agora(), 'rtc': rtc, 'situacao': 'gravando', 'render': 'nativo'}
    m['sessoes'].append(s)
    gravar_manifesto(cid, m)
    exe = os.path.join(CAMPANHAS, cid, 'programa', 'HeadHunter.exe')
    args = [exe, '--data', os.path.join(m['instalacao'], 'data'), '--rtc', str(rtc), '--saves-pasta',
            os.path.join(ps, 'saves'), '--gravar', os.path.join(ps, 'entrada.txt'), '--log', os.path.join(ps, 'quadros.txt'),
            '--hash-every', str(HASH_A_CADA), '--estado', os.path.join(ps, 'estado.txt'), '--estado-cada', str(ESTADO_A_CADA)]
    print('sessao %d: jogue normalmente e SALVE no jogo antes de fechar a janela' % n)
    # numa campanha real, nenhuma variavel HH_ do ambiente chega ao jogo (o atalho nao deve herdar as de desenvolvimento)
    env = dict(os.environ) if m['teste'] else {k: v for k, v in os.environ.items() if not k.upper().startswith('HH_')}
    args, kw = _falso(args, {'cwd': os.path.join(CAMPANHAS, cid, 'programa'), 'env': env})
    proc = subprocess.Popen(args, **kw)
    m = carregar(cid)
    m['sessoes'][-1]['pid'] = identidade(proc.pid) or str(proc.pid)  # a recuperacao espera este processo
    gravar_manifesto(cid, m)
    rc = proc.wait()
    m = carregar(cid)
    s = m['sessoes'][-1]
    fechar_sessao(cid, m, s, rc)
    gravar_manifesto(cid, m)
    print('sessao %d %s (rc %d)' % (n, s['situacao'], rc))
    return copiar(cid, m)


def pasta_copia(cid, m, para=None):
    return os.path.join(os.path.abspath(para or m.get('copia') or os.path.join(CAMPANHAS, '_copias')), cid)


def copiar(cid, m, para=None):
    """copia de seguranca incremental da campanha (docs/CAMPANHA.md): copia o que falta ou difere, nunca apaga nada no
    destino, e confere os selos na copia. Uma falha avisa (rc 2) mas nao desfaz a sessao gravada."""
    orig = os.path.join(CAMPANHAS, cid)
    dest = pasta_copia(cid, m, para)
    if os.path.commonpath([os.path.abspath(dest), orig]) == orig:
        sys.exit('a copia nao pode ficar dentro da propria campanha: %s' % dest)
    novos = 0
    try:
        for raiz, dirs, arqs in os.walk(orig):
            # pastas vazias contam (saves_inicio da sessao 1 = cartao vazio): sem elas a copia nao reproduz
            os.makedirs(os.path.join(dest, os.path.relpath(raiz, orig)), exist_ok=True)
            for f in arqs:
                o = os.path.join(raiz, f)
                d = os.path.join(dest, os.path.relpath(o, orig))
                if os.path.exists(d) and sha256(d) == sha256(o):
                    continue
                os.makedirs(os.path.dirname(d), exist_ok=True)
                shutil.copy2(o, d + '.novo')
                os.replace(d + '.novo', d)  # nunca deixa um arquivo pela metade com o nome final
                novos += 1
        ruins = [r for r, h in m.get('selos', {}).items()
                 if not os.path.exists(os.path.join(dest, r)) or sha256(os.path.join(dest, r)) != h]
    except OSError as e:
        print('AVISO: copia de seguranca falhou (%s); a sessao continua gravada em %s' % (e, orig))
        return 2
    if ruins:
        print('AVISO: copia de seguranca em %s nao confere: %s' % (dest, ruins))
        return 2
    print('copia de seguranca: %s (%d arquivos novos ou atualizados, %d selos conferidos)' % (
        dest, novos, len(m.get('selos', {}))))
    return 0


def cmd_copiar(a):
    cid = ultima(a.id)
    return copiar(cid, carregar(cid), a.para)


def linhas_inteiras(p):
    """as linhas de `p` sem uma ultima pela metade (uma queda no meio da escrita; revisao Codex HH16.P3)"""
    with open(p, 'rb') as f:
        bruto = f.read()
    if bruto and not bruto.endswith(b'\n'):
        bruto = bruto[:bruto.rfind(b'\n') + 1]
    return bruto.decode('utf-8', errors='replace').splitlines()


def ler_quadros(p):
    out = {}
    for l in linhas_inteiras(p):
        if l.startswith('quadro ') and ':' in l:
            q, resto = l[7:].split(':', 1)
            if q.strip().isdigit():
                out[int(q)] = resto.strip()
    return out


def ler_estado(p):
    """'estado <leitura> <hash>' do --estado; {} se o arquivo nao existe (sessoes anteriores ao --estado)"""
    out = {}
    if os.path.exists(p):
        for l in linhas_inteiras(p):
            c = l.split()
            if len(c) == 3 and c[0] == 'estado' and c[1].isdigit():
                out[int(c[1])] = c[2]
    return out


REFERENCIA_QUADROS = 'quadros_referencia.txt'


def reproduzir(cid, m, n, exe, data, saida, fixar_referencia=False):
    """headless, no tempo logico (o --reproduzir forca), no software, ate a leitura do fim da gravacao"""
    ps = pasta_sessao(cid, n)
    grav = ler_quadros(os.path.join(ps, 'quadros.txt'))
    if not grav:
        return {'veredito': 'SEM_QUADROS'}
    ate = max(grav)
    if os.path.isdir(saida):  # a reproducao anterior e guardada com outro nome; nada e apagado
        os.replace(saida, '%s.%s' % (saida, time.strftime('%Y%m%d-%H%M%S')))
    os.makedirs(saida)
    saves = os.path.join(saida, 'saves')
    shutil.copytree(os.path.join(ps, 'saves_inicio'), saves)
    s = m['sessoes'][n - 1]
    entrada = os.path.join(ps, 'entrada.txt')
    if s['situacao'] != 'gravada':
        # uma queda no meio da escrita pode deixar a ultima linha pela metade; a reproducao usa uma copia sem ela (a
        # gravacao selada nao e tocada)
        with open(entrada, 'rb') as f:
            bruto = f.read()
        if bruto and not bruto.endswith(b'\n'):
            entrada = os.path.join(saida, 'entrada_ate_a_queda.txt')
            with open(entrada, 'wb') as f:
                f.write(bruto[:bruto.rfind(b'\n') + 1])
    # a gravacao fechada diz a ultima leitura; a reproducao para nela (HeadHunter --reproduzir) e o --vblanks e so um teto
    # (a sessao acaba num VBlank qualquer, fora da grade dos hashes; num carregamento a proxima leitura demora)
    fim_leitura = ultima_leitura(os.path.join(ps, 'entrada.txt'))
    teto = ate + FOLGA_VBLANKS if fim_leitura is not None else ate
    args = [exe, '--headless', '--vblanks', str(teto), '--data', data, '--rtc', str(s['rtc']), '--saves-pasta', saves,
            '--reproduzir', entrada, '--log', os.path.join(saida, 'quadros.txt'),
            '--hash-every', str(HASH_A_CADA), '--estado', os.path.join(saida, 'estado.txt'), '--estado-cada',
            str(ESTADO_A_CADA)]
    arquivos = os.path.join(saida, 'arquivos.tsv')
    p = executar(args, capture_output=True, text=True, errors='replace', env=ambiente_reproducao(HH_FILE_LOG=arquivos))
    rc = p.returncode
    with open(os.path.join(saida, 'stderr.txt'), 'w', encoding='utf-8') as f:
        f.write(p.stderr or '')
    rep = ler_quadros(os.path.join(saida, 'quadros.txt')) if os.path.exists(os.path.join(saida, 'quadros.txt')) else {}
    # Quadros: cada renderizador comparado consigo mesmo. A janela grava no Vulkan, e o Vulkan nao e identico bit a bit
    # ao software da reproducao (o HH13 compara os dois por PSNR): os quadros da janela entao so informam. A primeira
    # reproducao aprovada fixa a referencia em software da sessao (quadros_referencia.txt, selada), e dali em diante
    # toda reproducao tem de ser igual a ela. O estado por leitura e os saves continuam obrigatorios contra a gravacao.
    janela_dif = next((q for q in sorted(grav) if rep.get(q) != grav[q]), None)
    ref_p = os.path.join(ps, REFERENCIA_QUADROS)
    if s.get('render') != 'nativo':
        quadros, primeira, vinculante = 'gravacao', janela_dif, True
    elif os.path.exists(ref_p):
        ref = ler_quadros(ref_p)
        quadros, primeira, vinculante = 'referencia', next((q for q in sorted(ref) if rep.get(q) != ref[q]), None), True
    else:
        quadros, primeira, vinculante = 'janela (Vulkan, informativo)', janela_dif, False
    # estado por leitura: a primeira leitura do controle em que a RAM do jogo diverge (antes de a imagem divergir)
    est_g, est_r = ler_estado(os.path.join(ps, 'estado.txt')), ler_estado(os.path.join(saida, 'estado.txt'))
    leitura = next((n for n in sorted(est_g) if est_r.get(n) != est_g[n]), None)
    chegou_ao_fim = fim_leitura is None or FIM_REPRODUZIDO in (p.stdout or '')
    saves_iguais = mesmos_saves(saves, os.path.join(ps, 'saves'))
    # revisao Codex HH16.P2: sem estado gravado ou reproduzido a sessao nao vale (nada aprova por ausencia)
    com_estado = bool(est_g) and bool(est_r)
    # uma sessao interrompida (queda) nao termina com rc 0 nem com os saves de fim: vale o que foi gravado ate a queda
    ok = (primeira is None or not vinculante) and leitura is None and com_estado and chegou_ao_fim and (
        s['situacao'] != 'gravada' or (rc == 0 and saves_iguais))
    criou_referencia = False
    if ok and fixar_referencia and not vinculante and rep:
        shutil.copyfile(os.path.join(saida, 'quadros.txt'), ref_p + '.novo')
        os.replace(ref_p + '.novo', ref_p)  # o chamador sela (manifesto) depois das reproducoes
        criou_referencia = True
    r = {'rc': rc, 'ate_vblank': ate, 'ultima_leitura': fim_leitura, 'chegou_ao_fim': chegou_ao_fim,
         'quadros_comparados_com': quadros, 'quadro_da_janela_diferente': janela_dif,
         'referencia_criada': criou_referencia,
         'primeiro_quadro_diferente': primeira, 'primeira_leitura_diferente': leitura,
         'estados_conferidos': len(est_g), 'com_estado': com_estado, 'saves_iguais': saves_iguais,
         'creditos': CREDITOS in (p.stderr or '') and abriu(arquivos, GAMEEND),
         'cobertura': cobertura(arquivos, p.stderr or ''),
         'veredito': ('REPRODUZIDA' if s['situacao'] == 'gravada' else 'REPRODUZIDA_ATE_A_QUEDA') if ok else 'REPROVADA'}
    with open(os.path.join(saida, 'resultado.json'), 'w', encoding='utf-8') as f:
        json.dump(r, f, indent=1)
    return r


FIM_GRAVACAO = '# fim da sessao, ultima leitura '   # ultima linha de uma gravacao fechada (HeadHunter --gravar)
FIM_REPRODUZIDO = 'fim da gravacao reproduzido'     # o --reproduzir parou na leitura seguinte a ultima gravada
FOLGA_VBLANKS = 18000                               # teto alem do ultimo hash (5 min de jogo) ate a leitura do fim
PROVA_LEITURA = 5         # a gravacao minima da prova do contrato termina nesta leitura
PROVA_VBLANKS = 3000      # teto da prova: as leituras do controle comecam logo depois do boot


def provar_contrato(exe, data):
    """None se o programa reproduz uma gravacao minima parando na leitura seguinte ao fim gravado; senao o motivo"""
    pasta = os.path.join(CAMPANHAS, '_prova_%d' % os.getpid())
    os.makedirs(os.path.join(pasta, 'saves'), exist_ok=True)
    entrada = os.path.join(pasta, 'entrada.txt')
    with open(entrada, 'w', encoding='ascii') as f:
        f.write('# prova do contrato da gravacao (campanha.py nova)\n0 -\n%s%d\n' % (FIM_GRAVACAO, PROVA_LEITURA))
    estado = os.path.join(pasta, 'estado.txt')
    args = [exe, '--headless', '--vblanks', str(PROVA_VBLANKS), '--data', data, '--rtc', '1700000000', '--saves-pasta',
            os.path.join(pasta, 'saves'), '--reproduzir', entrada, '--log', os.path.join(pasta, 'quadros.txt'),
            '--hash-every', '60', '--estado', estado, '--estado-cada', '1']
    try:
        p = executar(args, capture_output=True, text=True, errors='replace', timeout=600, env=ambiente_reproducao())
    except (OSError, subprocess.TimeoutExpired) as e:
        return 'nao executou: %s' % e
    if p.returncode != 0 or FIM_REPRODUZIDO not in (p.stdout or ''):
        return 'nao parou no fim da gravacao (rc %d)' % p.returncode
    est = ler_estado(estado)
    if not est or max(est) > PROVA_LEITURA + 1:
        return 'estado por leitura ausente ou alem do fim'
    return None


def ultima_leitura(entrada):
    """N de '# fim da sessao, ultima leitura N'; None sem o comentario (a sessao caiu) ou sem leitura nenhuma"""
    n = None
    with open(entrada, encoding='ascii', errors='replace') as f:
        for l in f:
            if l.startswith(FIM_GRAVACAO):
                try:
                    n = int(l[len(FIM_GRAVACAO):].split()[0])
                except (ValueError, IndexError):
                    n = None
    return None if n is None or n >= 2 ** 64 - 1 else n


def cobertura(tsv, stderr=''):
    """o que a reproducao abriu (HH_FILE_LOG): areas (AHFDATA), musicas e falas (STREAMS) e o total de arquivos distintos;
    os filmes vem do provedor nativo ("[video] MOVIES/X.4XM: inicio ...", que nao passa pelo log de arquivos). E a parte
    de arquivos da matriz de cobertura da HH16.3; as funcoes executadas por sessao ficam para la (perfil no build de
    verificacao, que o produto nao aceita)"""
    filmes = sorted({l.split()[1].rstrip(':') for l in stderr.splitlines()
                     if l.startswith('[video] MOVIES/') and ': inicio' in l})
    nomes = set()
    if os.path.exists(tsv):
        with open(tsv, encoding='utf-8', errors='replace') as f:
            for l in f:
                c = l.rstrip('\n').split('\t')
                if len(c) > 4 and c[4]:
                    nomes.add(c[4])
    grupo = lambda pre: sorted(x for x in nomes if x.upper().startswith(pre))
    return {'arquivos': len(nomes), 'areas': grupo('AHFDATA/'), 'filmes': sorted(set(filmes) | set(grupo('MOVIES/'))),
            'streams': len(grupo('STREAMS/'))}


def abriu(tsv, nome):
    """o log de arquivos (HH_FILE_LOG: seq, vblank, origem, tipo, arquivo, ...) tem uma leitura de `nome`"""
    if not os.path.exists(tsv):
        return False
    with open(tsv, encoding='utf-8', errors='replace') as f:
        return any(l.split('\t')[4:5] == [nome] for l in f)


def arvore(pasta):
    """caminhos relativos de todos os arquivos sob `pasta` (o jogo grava em subpastas: saves/card0/H_HUNTER.SYS.hhsv)"""
    out = []
    for raiz, _, arqs in os.walk(pasta):
        out += [os.path.relpath(os.path.join(raiz, f), pasta) for f in arqs]
    return sorted(out)


def mesmos_saves(a, b):
    la = arvore(a) if os.path.isdir(a) else []
    lb = arvore(b) if os.path.isdir(b) else []
    return la == lb and all(sha256(os.path.join(a, f)) == sha256(os.path.join(b, f)) for f in la)


def reproduzir_todas(cid, m, exe, data, saida_de, so=None, paralelo=1, fixar_referencia=False):
    """reproduz as sessoes (cada uma parte dos proprios saves de inicio, entao podem rodar ao mesmo tempo; a cadeia e
    conferida depois); resultados na ordem das sessoes"""
    import concurrent.futures
    ns = [s['n'] for s in m['sessoes'] if not so or s['n'] == so]
    with concurrent.futures.ThreadPoolExecutor(max_workers=max(1, paralelo)) as ex:
        fut = {n: ex.submit(reproduzir, cid, m, n, exe, data, saida_de(n), fixar_referencia) for n in ns}
    res = []
    for n in ns:
        r = fut[n].result()
        r['sessao'] = n
        res.append(r)
        print('sessao %d: %s' % (n, r['veredito']))
    return res


def cmd_verificar(a):
    cid = ultima(a.id)
    with Trava(cid):
        return verificar(cid, a)


def verificar(cid, a):
    m = recuperar(cid, carregar(cid))
    ruins = conferir_selos(cid, m)
    if ruins:
        print('REPROVADA: arquivos alterados depois da gravacao: %s' % ruins)
        return 1
    exe = os.path.join(CAMPANHAS, cid, 'programa', 'HeadHunter.exe')
    data = os.path.join(m['instalacao'], 'data')
    res = reproduzir_todas(cid, m, exe, data, lambda n: os.path.join(pasta_sessao(cid, n), 'reproducao'), a.sessao,
                           a.paralelo, fixar_referencia=True)
    novas = [r['sessao'] for r in res if r.get('referencia_criada')]
    for n in novas:  # a referencia de quadros em software da sessao, fixada pela primeira reproducao aprovada
        selar(cid, m, os.path.relpath(os.path.join(pasta_sessao(cid, n), REFERENCIA_QUADROS), os.path.join(CAMPANHAS, cid)))
        print('sessao %d: referencia de quadros em software fixada (os quadros da janela, no Vulkan, so informam)' % n)
    if novas:
        gravar_manifesto(cid, m)
    # cadeia: cada sessao comeca dos saves que a reproducao da anterior produziu (a primeira, do cartao vazio)
    cadeia_ok = True
    for s in m['sessoes'][1:]:
        if a.sessao:
            break
        rep_ant = os.path.join(pasta_sessao(cid, s['n'] - 1), 'reproducao', 'saves')
        if not mesmos_saves(os.path.join(pasta_sessao(cid, s['n']), 'saves_inicio'), rep_ant):
            cadeia_ok = False
            print('sessao %d: os saves de inicio nao sao os reproduzidos da sessao %d (cadeia quebrada)' % (s['n'], s['n'] - 1))
    creditos = bool(res) and res[-1].get('creditos') and res[-1]['veredito'] == 'REPRODUZIDA'
    # qualquer veredito que nao seja de aprovacao (REPROVADA, SEM_QUADROS...) reprova (revisao Codex HH16.P3)
    if any(r['veredito'] not in VEREDITOS_BONS for r in res) or not cadeia_ok:
        geral = 'REPROVADA'
    elif m['teste']:
        geral = 'TESTE REPRODUZIDO'
    elif creditos and not a.sessao:
        geral = 'CAMPANHA REPRODUZIDA ATE OS CREDITOS'
    else:
        geral = 'REPRODUZIDA, MAS INCOMPLETA (sem os creditos)'
    cob = {'areas': sorted({x for r in res for x in r.get('cobertura', {}).get('areas', [])}),
           'filmes': sorted({x for r in res for x in r.get('cobertura', {}).get('filmes', [])})}
    with open(os.path.join(CAMPANHAS, cid, 'relatorio.json'), 'w', encoding='utf-8') as f:
        json.dump({'campanha': cid, 'quando': agora(), 'veredito': geral, 'cobertura': cob, 'sessoes': res}, f, indent=1)
    print('cobertura: %d areas, %d filmes' % (len(cob['areas']), len(cob['filmes'])))
    print(geral)
    return 0 if geral != 'REPROVADA' else 1


def cmd_reproduzir_em(a):
    with Trava(a.id):  # duas reproducoes ao mesmo tempo misturariam as pastas de saida (revisao Codex HH16.P3)
        return reproduzir_em(a)


def reproduzir_em(a):
    m = carregar(a.id)
    inst = os.path.abspath(a.instalacao)
    if sha256(os.path.join(inst, 'data', 'arquivos.manifest')) != m['dados_sha256']:
        sys.exit('so vale para uma instalacao feita do mesmo disco (dados diferentes)')
    rot = a.rotulo or time.strftime('%Y%m%d-%H%M%S')
    res = reproduzir_todas(a.id, m, os.path.join(inst, 'HeadHunter.exe'), os.path.join(inst, 'data'),
                           lambda n: os.path.join(CAMPANHAS, a.id, 'outros', rot, 'sessao_%03d' % n), None, a.paralelo)
    ok = bool(res) and all(r['veredito'] in VEREDITOS_BONS for r in res)
    print('REPRODUZIDA EM OUTRO PROGRAMA' if ok else 'DIVERGENTE')  # nunca aprova nem reprova a campanha
    return 0 if ok else 1


def cmd_status(a):
    cid = ultima(a.id)
    m = carregar(cid)
    print('campanha %s%s, criada %s, %d sessoes' % (cid, ' (teste)' if m['teste'] else '', m['criada'], len(m['sessoes'])))
    for s in m['sessoes']:
        rep = ler_resultado(os.path.join(pasta_sessao(cid, s['n']), 'reproducao')) or 'nao verificada'
        print('  sessao %d: %s%s, inicio %s, fim %s, reproducao: %s' % (
            s['n'], s['situacao'], ' (recuperada)' if s.get('recuperada') else '', s['inicio'], s.get('fim', '-'), rep))
    print('  copia de seguranca: %s' % pasta_copia(cid, m))
    try:
        with open(os.path.join(CAMPANHAS, cid, 'relatorio.json'), encoding='utf-8') as f:
            rel = json.load(f)
        print('  ultima verificacao (%s): %s' % (rel['quando'], rel['veredito']))
    except (OSError, ValueError, KeyError):
        print('  ainda nao verificada')


def cmd_janela(a):
    """janela simples (HH16.1): Jogar e gravar, Verificar, Status, Copiar; cada botao roda este mesmo monitor num
    processo filho e mostra a saida. --so-montar monta a janela escondida e fecha (teste)."""
    import queue
    import threading
    import tkinter as tk
    from tkinter import scrolledtext
    raiz = tk.Tk()
    if a.so_montar:
        raiz.withdraw()
    raiz.title('Head Hunter Recompiled: campanha gravada')
    texto = scrolledtext.ScrolledText(raiz, width=100, height=28, font=('Consolas', 10))
    fila = queue.Queue()
    botoes = []

    def rodar(*cmd):
        for b in botoes:
            b.config(state='disabled')
        texto.insert('end', '\n> %s\n' % ' '.join(cmd))

        def fio():
            p = subprocess.Popen([sys.executable, '-I', os.path.abspath(__file__)] + list(cmd), stdout=subprocess.PIPE,
                                 stderr=subprocess.STDOUT, text=True, errors='replace')
            for l in p.stdout:
                fila.put(l)
            fila.put(None)
        threading.Thread(target=fio, daemon=True).start()

    def bombear():
        try:
            while True:
                l = fila.get_nowait()
                if l is None:
                    for b in botoes:
                        b.config(state='normal')
                else:
                    texto.insert('end', l)
                    texto.see('end')
        except queue.Empty:
            pass
        raiz.after(100, bombear)

    barra = tk.Frame(raiz)
    for rot, cmd in (('Jogar e gravar (continuar)', ('jogar', '--criar')), ('Verificar', ('verificar',)), ('Status', ('status',)),
                     ('Copiar agora', ('copiar',))):
        b = tk.Button(barra, text=rot, command=lambda c=cmd: rodar(*c))
        b.pack(side='left', padx=4, pady=4)
        botoes.append(b)
    barra.pack(fill='x')
    texto.pack(fill='both', expand=True)
    texto.insert('end', 'Jogue, SALVE no jogo e feche a janela do jogo para terminar a sessao. Guia: docs/CAMPANHA.md\n')
    if not existe_campanha():
        texto.insert('end', 'Nenhuma campanha ainda: o primeiro "Jogar e gravar" cria a campanha.\n')
    bombear()
    if a.so_montar:
        raiz.update()
        print('janela montada: %d botoes' % len(botoes))
        raiz.destroy()
        return 0
    raiz.mainloop()
    return 0


def cmd_lista(a):
    for d in sorted(os.listdir(CAMPANHAS)) if os.path.isdir(CAMPANHAS) else []:
        if os.path.isfile(os.path.join(CAMPANHAS, d, 'campanha.json')):
            print(d)


def main():
    ap = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    sub = ap.add_subparsers(dest='cmd', required=True)
    p = sub.add_parser('nova')
    p.add_argument('--instalacao')
    p.add_argument('--teste', action='store_true')
    p.add_argument('--copia', help='pasta da copia de seguranca automatica (de preferencia outro disco)')
    p = sub.add_parser('copiar')
    p.add_argument('id', nargs='?')
    p.add_argument('--para')
    p = sub.add_parser('jogar')
    p.add_argument('id', nargs='?')
    p.add_argument('--criar', action='store_true', help='sem campanha nenhuma, cria uma (atalho e janela)')
    p.add_argument('--teste', action='store_true', help='com --criar: a ultima campanha de TESTE (sessao piloto)')
    p = sub.add_parser('status')
    p.add_argument('id', nargs='?')
    p = sub.add_parser('verificar')
    p.add_argument('id', nargs='?')
    p.add_argument('--sessao', type=int)
    p.add_argument('--paralelo', type=int, default=1, help='sessoes reproduzidas ao mesmo tempo')
    p = sub.add_parser('reproduzir-em')
    p.add_argument('id')
    p.add_argument('--instalacao', required=True)
    p.add_argument('--rotulo')
    p.add_argument('--paralelo', type=int, default=1)
    sub.add_parser('lista')
    p = sub.add_parser('janela')
    p.add_argument('--so-montar', action='store_true', help=argparse.SUPPRESS)
    a = ap.parse_args()
    os.makedirs(CAMPANHAS, exist_ok=True)
    return {'nova': cmd_nova, 'jogar': cmd_jogar, 'status': cmd_status, 'verificar': cmd_verificar,
            'reproduzir-em': cmd_reproduzir_em, 'lista': cmd_lista, 'copiar': cmd_copiar,
            'janela': cmd_janela}[a.cmd](a) or 0


if __name__ == '__main__':
    sys.exit(main())
