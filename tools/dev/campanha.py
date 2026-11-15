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
DIST = os.path.join(RAIZ, 'dist', 'HeadHunter-Preview')
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


def executar(args, **kw):
    if PROGRAMA_FALSO:
        args = [sys.executable, '-I', PROGRAMA_FALSO] + list(args[1:])
    return subprocess.run(args, **kw)


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


def cmd_jogar(a):
    cid = ultima(a.id)
    m = carregar(cid)
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
        for f in os.listdir(orig):
            shutil.copy2(os.path.join(orig, f), os.path.join(ps, 'saves_inicio', f))
            shutil.copy2(os.path.join(orig, f), os.path.join(ps, 'saves', f))
    rtc = int(time.time())
    s = {'n': n, 'inicio': agora(), 'rtc': rtc, 'situacao': 'gravando'}
    m['sessoes'].append(s)
    gravar_manifesto(cid, m)
    exe = os.path.join(CAMPANHAS, cid, 'programa', 'HeadHunter.exe')
    args = [exe, '--data', os.path.join(m['instalacao'], 'data'), '--rtc', str(rtc), '--saves-pasta',
            os.path.join(ps, 'saves'), '--gravar', os.path.join(ps, 'entrada.txt'), '--log', os.path.join(ps, 'quadros.txt'),
            '--hash-every', str(HASH_A_CADA), '--estado', os.path.join(ps, 'estado.txt'), '--estado-cada', str(ESTADO_A_CADA)]
    print('sessao %d: jogue normalmente e SALVE no jogo antes de fechar a janela' % n)
    rc = executar(args, cwd=os.path.join(CAMPANHAS, cid, 'programa')).returncode
    m = carregar(cid)
    s = m['sessoes'][-1]
    s['fim'], s['rc'] = agora(), rc
    s['situacao'] = 'gravada' if rc == 0 else 'interrompida'
    for rel in ('entrada.txt', 'quadros.txt', 'estado.txt'):
        if os.path.exists(os.path.join(ps, rel)):
            selar(cid, m, os.path.relpath(os.path.join(ps, rel), os.path.join(CAMPANHAS, cid)))
    for f in sorted(os.listdir(os.path.join(ps, 'saves'))):
        selar(cid, m, os.path.relpath(os.path.join(ps, 'saves', f), os.path.join(CAMPANHAS, cid)))
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


def ler_quadros(p):
    out = {}
    with open(p, encoding='utf-8') as f:
        for l in f:
            if l.startswith('quadro '):
                q, resto = l[7:].split(':', 1)
                out[int(q)] = resto.strip()
    return out


def ler_estado(p):
    """'estado <leitura> <hash>' do --estado; {} se o arquivo nao existe (sessoes anteriores ao --estado)"""
    out = {}
    if os.path.exists(p):
        with open(p, encoding='utf-8') as f:
            for l in f:
                c = l.split()
                if len(c) == 3 and c[0] == 'estado':
                    out[int(c[1])] = c[2]
    return out


def reproduzir(cid, m, n, exe, data, saida):
    """headless, no tempo logico (o --reproduzir forca), no software, ate o ultimo VBlank do --log gravado"""
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
    args = [exe, '--headless', '--vblanks', str(ate), '--data', data, '--rtc', str(s['rtc']), '--saves-pasta', saves,
            '--reproduzir', os.path.join(ps, 'entrada.txt'), '--log', os.path.join(saida, 'quadros.txt'),
            '--hash-every', str(HASH_A_CADA), '--estado', os.path.join(saida, 'estado.txt'), '--estado-cada',
            str(ESTADO_A_CADA)]
    arquivos = os.path.join(saida, 'arquivos.tsv')
    p = executar(args, capture_output=True, text=True, errors='replace', env=dict(os.environ, HH_FILE_LOG=arquivos))
    rc = p.returncode
    with open(os.path.join(saida, 'stderr.txt'), 'w', encoding='utf-8') as f:
        f.write(p.stderr or '')
    rep = ler_quadros(os.path.join(saida, 'quadros.txt')) if os.path.exists(os.path.join(saida, 'quadros.txt')) else {}
    primeira = next((q for q in sorted(grav) if rep.get(q) != grav[q]), None)
    # estado por leitura: a primeira leitura do controle em que a RAM do jogo diverge (antes de a imagem divergir)
    est_g, est_r = ler_estado(os.path.join(ps, 'estado.txt')), ler_estado(os.path.join(saida, 'estado.txt'))
    leitura = next((n for n in sorted(est_g) if est_r.get(n) != est_g[n]), None)
    saves_iguais = mesmos_saves(saves, os.path.join(ps, 'saves'))
    # revisao Codex HH16.P2: sem estado gravado ou reproduzido a sessao nao vale (nada aprova por ausencia)
    com_estado = bool(est_g) and bool(est_r)
    # uma sessao interrompida (queda) nao termina com rc 0 nem com os saves de fim: vale o que foi gravado ate a queda
    ok = primeira is None and leitura is None and com_estado and (
        s['situacao'] != 'gravada' or (rc == 0 and saves_iguais))
    r = {'rc': rc, 'ate_vblank': ate, 'primeiro_quadro_diferente': primeira, 'primeira_leitura_diferente': leitura,
         'estados_conferidos': len(est_g), 'com_estado': com_estado, 'saves_iguais': saves_iguais,
         'creditos': CREDITOS in (p.stderr or '') and abriu(arquivos, GAMEEND),
         'veredito': ('REPRODUZIDA' if s['situacao'] == 'gravada' else 'REPRODUZIDA_ATE_A_QUEDA') if ok else 'REPROVADA'}
    with open(os.path.join(saida, 'resultado.json'), 'w', encoding='utf-8') as f:
        json.dump(r, f, indent=1)
    return r


def abriu(tsv, nome):
    """o log de arquivos (HH_FILE_LOG: seq, vblank, origem, tipo, arquivo, ...) tem uma leitura de `nome`"""
    if not os.path.exists(tsv):
        return False
    with open(tsv, encoding='utf-8', errors='replace') as f:
        return any(l.split('\t')[4:5] == [nome] for l in f)


def mesmos_saves(a, b):
    la = sorted(os.listdir(a)) if os.path.isdir(a) else []
    lb = sorted(os.listdir(b)) if os.path.isdir(b) else []
    return la == lb and all(sha256(os.path.join(a, f)) == sha256(os.path.join(b, f)) for f in la)


def cmd_verificar(a):
    cid = ultima(a.id)
    m = carregar(cid)
    ruins = conferir_selos(cid, m)
    if ruins:
        print('REPROVADA: arquivos alterados depois da gravacao: %s' % ruins)
        return 1
    exe = os.path.join(CAMPANHAS, cid, 'programa', 'HeadHunter.exe')
    data = os.path.join(m['instalacao'], 'data')
    res = []
    for s in m['sessoes']:
        if a.sessao and s['n'] != a.sessao:
            continue
        r = reproduzir(cid, m, s['n'], exe, data, os.path.join(pasta_sessao(cid, s['n']), 'reproducao'))
        r['sessao'] = s['n']
        res.append(r)
        print('sessao %d: %s' % (s['n'], r['veredito']))
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
    if any(r['veredito'] == 'REPROVADA' for r in res) or not cadeia_ok:
        geral = 'REPROVADA'
    elif m['teste']:
        geral = 'TESTE REPRODUZIDO'
    elif creditos and not a.sessao:
        geral = 'CAMPANHA REPRODUZIDA ATE OS CREDITOS'
    else:
        geral = 'REPRODUZIDA, MAS INCOMPLETA (sem os creditos)'
    with open(os.path.join(CAMPANHAS, cid, 'relatorio.json'), 'w', encoding='utf-8') as f:
        json.dump({'campanha': cid, 'quando': agora(), 'veredito': geral, 'sessoes': res}, f, indent=1)
    print(geral)
    return 0 if geral != 'REPROVADA' else 1


def cmd_reproduzir_em(a):
    m = carregar(a.id)
    inst = os.path.abspath(a.instalacao)
    if sha256(os.path.join(inst, 'data', 'arquivos.manifest')) != m['dados_sha256']:
        sys.exit('so vale para uma instalacao feita do mesmo disco (dados diferentes)')
    rot = a.rotulo or time.strftime('%Y%m%d-%H%M%S')
    res = []
    for s in m['sessoes']:
        r = reproduzir(a.id, m, s['n'], os.path.join(inst, 'HeadHunter.exe'), os.path.join(inst, 'data'),
                       os.path.join(CAMPANHAS, a.id, 'outros', rot, 'sessao_%03d' % s['n']))
        res.append(r)
        print('sessao %d: %s' % (s['n'], r['veredito']))
    ok = all(r['veredito'] != 'REPROVADA' for r in res)
    print('REPRODUZIDA EM OUTRO PROGRAMA' if ok else 'DIVERGENTE')  # nunca aprova nem reprova a campanha
    return 0 if ok else 1


def cmd_status(a):
    cid = ultima(a.id)
    m = carregar(cid)
    print('campanha %s%s, criada %s, %d sessoes' % (cid, ' (teste)' if m['teste'] else '', m['criada'], len(m['sessoes'])))
    for s in m['sessoes']:
        print('  sessao %d: %s, inicio %s, fim %s' % (s['n'], s['situacao'], s['inicio'], s.get('fim', '-')))


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
    p = sub.add_parser('status')
    p.add_argument('id', nargs='?')
    p = sub.add_parser('verificar')
    p.add_argument('id', nargs='?')
    p.add_argument('--sessao', type=int)
    p = sub.add_parser('reproduzir-em')
    p.add_argument('id')
    p.add_argument('--instalacao', required=True)
    p.add_argument('--rotulo')
    sub.add_parser('lista')
    a = ap.parse_args()
    os.makedirs(CAMPANHAS, exist_ok=True)
    return {'nova': cmd_nova, 'jogar': cmd_jogar, 'status': cmd_status, 'verificar': cmd_verificar,
            'reproduzir-em': cmd_reproduzir_em, 'lista': cmd_lista, 'copiar': cmd_copiar}[a.cmd](a) or 0


if __name__ == '__main__':
    sys.exit(main())
