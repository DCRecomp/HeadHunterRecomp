#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
"""HH15.8: ausencia operacional do legado no executavel do produto, por duas provas independentes do mapa do linker
(a catraca hh15_produto_papeis):

  1. imports: as DLLs das tabelas de imports e de delay-load do PE (lidas aqui, sem dumpbin) estao na lista fechada do
     produto (sistema, Win32, audio e janela), e as obrigatorias estao la (um parser que nao le nada reprova);
  2. strings: nenhum literal exclusivo das fontes que nao podem estar no produto (os papeis so da verificacao e das
     ferramentas do runtime, a captura da IR e o tradutor com o interpretador) aparece nos bytes do executavel.
     Exclusivo: literal estreito (16+ caracteres ASCII; escapes simples decodificados, os outros descartam o literal)
     que nao esta contido em nenhum literal das outras
     fontes de app/, runtime/, transitional/ e native/. Os literais saem de um lexer de C++ (comentarios, caracteres,
     prefixos e raw strings), e cada grupo de fontes tem um minimo de literais conferidos (sem lista vazia por grupo).

Negativos por mutacao (HH15_FECHAMENTO_MUTAR): dll_extra (uma DLL fora da lista entra nos imports lidos), sem_imports
(o parser nao devolve nada), string_extra:<grupo> (um literal exclusivo do grupo e acrescentado aos bytes lidos).

uso: check_fechamento.py <HeadHunter.exe> <raiz do projeto> <runtime_papeis.tsv>
"""
import os
import re
import struct
import sys

EXE, ROOT, MANIFEST = (os.path.abspath(a) for a in sys.argv[1:4])
MUTAR = os.environ.get('HH15_FECHAMENTO_MUTAR', '')

# DLLs que o produto pode importar (medido no HH15.8, c559859): o runtime do MSVC (CRT universal e C++), o Win32 da
# janela e da entrada, Media Foundation (filmes nativos), Direct2D/DirectWrite (texto da interface) e o mixer do
# Windows; o Vulkan e o XAudio2 sao carregados em tempo de execucao, nao importados
DLLS = {
    'kernel32.dll', 'user32.dll', 'gdi32.dll', 'shell32.dll', 'ole32.dll', 'oleaut32.dll', 'advapi32.dll',
    'imm32.dll', 'setupapi.dll', 'version.dll', 'winmm.dll', 'xinput1_4.dll', 'mfplat.dll', 'mfreadwrite.dll',
    'd2d1.dll', 'dwrite.dll', 'msvcp140.dll', 'msvcp140_atomic_wait.dll', 'vcruntime140.dll', 'vcruntime140_1.dll',
}
DLLS_CRT = re.compile(r'api-ms-win-crt-[a-z]+-l1-1-0\.dll')  # o CRT universal, por contrato de API do Windows
OBRIGATORIAS = {'kernel32.dll', 'user32.dll', 'vcruntime140.dll', 'xinput1_4.dll', 'mfplat.dll'}
FORA_DO_PRODUTO = ('validacao', 'hle', 'perifericos', 'aica', 'sb', 'intc', 'grafico')
MIN_POR_GRUPO = 3


def pe_imports(data):
    """nomes das DLLs das tabelas de imports (diretorio 1) e de delay-load (diretorio 13), PE32+."""
    pe = struct.unpack_from('<I', data, 0x3C)[0]
    assert data[pe:pe + 4] == b'PE\0\0', 'nao e PE'
    nsec, optsize = struct.unpack_from('<H', data, pe + 6)[0], struct.unpack_from('<H', data, pe + 20)[0]
    opt = pe + 24
    assert struct.unpack_from('<H', data, opt)[0] == 0x20B, 'nao e PE32+'
    ndirs = struct.unpack_from('<I', data, opt + 108)[0]
    secs = []
    for i in range(nsec):
        vsize, va, rsize, raw = struct.unpack_from('<IIII', data, opt + optsize + 40 * i + 8)
        secs.append((va, min(vsize, rsize) if vsize else rsize, raw))

    def off(rva, n=1):
        for va, size, raw in secs:  # so os bytes que estao no arquivo
            if va <= rva and rva + n <= va + size:
                return raw + rva - va
        raise ValueError('RVA fora dos dados das secoes: %X' % rva)

    def cstr(rva):
        o = off(rva)
        return data[o:data.index(b'\0', o)].decode('ascii').lower()

    def directory(i):
        return struct.unpack_from('<II', data, opt + 112 + 8 * i) if i < ndirs else (0, 0)

    names = []
    for idx, desc, name_at in ((1, 20, 12), (13, 32, 4)):
        rva, size = directory(idx)
        for k in range(size // desc if rva else 0):
            o = off(rva + k * desc, desc)
            if not any(data[o:o + desc]):
                break  # descritor nulo: fim da tabela
            names.append(cstr(struct.unpack_from('<I', data, o + name_at)[0]))
    return names


def literals(path):
    """literais de string estreitos de uma fonte C++ (lexer: comentarios, caracteres, prefixos, raw strings)."""
    with open(path, encoding='utf-8', errors='replace') as f:
        t = f.read()
    out, i, n = set(), 0, len(t)
    while i < n:
        c = t[i]
        if t.startswith('//', i):
            i = t.find('\n', i)
            i = n if i < 0 else i
        elif t.startswith('/*', i):
            i = t.find('*/', i + 2)
            i = n if i < 0 else i + 2
        elif c == "'":
            i += 1
            while i < n and t[i] != "'":
                i += 2 if t[i] == '\\' else 1
            i += 1
        elif c == '"' or (c in 'uUL' and re.match(r'(u8|u|U|L)?R?"', t[i:i + 4])) or (c == 'R' and t.startswith('R"', i)):
            m = re.match(r'(u8|u|U|L)?(R?)"', t[i:])
            if i > 0 and (t[i - 1].isalnum() or t[i - 1] == '_') and m.group(0) != '"':
                i += 1  # identificador terminado em u/U/L/R, nao prefixo
                continue
            wide = m.group(1) in ('u', 'U', 'L')
            i += len(m.group(0))
            if m.group(2):  # raw string: R"delim( ... )delim"
                p = t.index('(', i)
                end = ')' + t[i:p] + '"'
                q = t.index(end, p + 1)
                s, i = t[p + 1:q], q + len(end)
            else:
                j = i
                while j < n and t[j] != '"':
                    j += 2 if t[j] == '\\' else 1
                s, i = t[i:j], j + 1
            if not wide:
                s = s if m.group(2) else unescape(s)  # raw string: os bytes sao o texto
                if s is not None:
                    out.add(s)
        else:
            i += 1
    return out


SIMPLES = {'n': '\n', 't': '\t', 'r': '\r', '"': '"', "'": "'", '\\': '\\'}


def unescape(s):
    """o literal como fica nos bytes, ou None com escape que nao e simples (hex, octal, universal)."""
    out, i = [], 0
    while i < len(s):
        if s[i] == '\\':
            if i + 1 >= len(s) or s[i + 1] not in SIMPLES:
                return None
            out.append(SIMPLES[s[i + 1]])
            i += 2
        else:
            out.append(s[i])
            i += 1
    return ''.join(out)


def sources(d):
    out = []
    for base, _, files in os.walk(os.path.join(ROOT, d)):
        out += [os.path.normpath(os.path.join(base, f)) for f in files if f.endswith(('.cpp', '.h', '.inc'))]
    return out


def main():
    data = open(EXE, 'rb').read()
    bad = []

    # 1. imports
    dlls = [] if MUTAR == 'sem_imports' else pe_imports(data)
    if MUTAR == 'dll_extra':
        dlls.append('flycast.dll')
    print('imports: %s' % ', '.join(sorted(dlls)))
    for d in dlls:
        if d not in DLLS and not DLLS_CRT.fullmatch(d):
            bad.append('import fora da lista do produto: %s' % d)
    for d in sorted(OBRIGATORIAS - set(dlls)):
        bad.append('import obrigatorio ausente (parser ou executavel errado): %s' % d)

    # 2. strings, por grupo de fontes fora do produto
    groups = {g: set() for g in FORA_DO_PRODUTO + ('captura', 'tradutor')}
    with open(MANIFEST, encoding='utf-8') as f:
        for line in list(f)[1:]:
            role, src = line.rstrip('\n').split('\t')
            if role in groups:
                groups[role].add(os.path.normpath(os.path.join(ROOT, 'runtime', src)))
    groups['captura'].add(os.path.normpath(os.path.join(ROOT, 'transitional', 'render_capture.cpp')))
    groups['tradutor'] |= set(sources('translator'))
    banned = set().union(*groups.values())
    allowed = set()
    for p in [p for d in ('app', 'runtime', 'transitional', 'native') for p in sources(d) if p not in banned]:
        allowed |= literals(p)
    allowed = '\x01'.join(sorted(allowed))  # um literal contido num literal do produto nao e exclusivo
    checked = {}  # literal -> (grupo, fonte)
    for g, paths in groups.items():
        count = 0
        for p in sorted(paths):
            if not os.path.isfile(p):
                bad.append('fonte do manifesto inexistente: %s' % os.path.relpath(p, ROOT))
                continue
            for s in literals(p):
                if len(s) >= 16 and s.isascii() and s not in allowed:
                    count += s not in checked
                    checked.setdefault(s, (g, os.path.relpath(p, ROOT).replace('\\', '/')))
        print('  %-11s %3d literais exclusivos (%d fontes)' % (g, count, len(paths)))
        if count < MIN_POR_GRUPO:
            bad.append('poucos literais conferidos no grupo %s: %d < %d' % (g, count, MIN_POR_GRUPO))
    if MUTAR.startswith('string_extra:'):
        g = MUTAR.split(':', 1)[1]
        probe = next(s for s, (gg, _) in sorted(checked.items()) if gg == g)
        data += b'\0' + probe.encode() + b'\0'
    print('literais exclusivos conferidos: %d' % len(checked))
    for s, (g, p) in sorted(checked.items(), key=lambda kv: kv[1]):
        if s.encode() in data:
            bad.append('literal do grupo %s (%s) no executavel: "%s"' % (g, p, s))

    for b in bad:
        print('FALHA: ' + b)
    print('fechamento do produto: %s' % ('ok' if not bad else '%d falhas' % len(bad)))
    return 1 if bad else 0


def synthetic_pe(delay_name_in_tail=False):
    """PE32+ minimo: uma secao com a tabela de imports (a.dll) e a de delay-load (b.dll); com delay_name_in_tail, o
    nome da b.dll aponta para a cauda virtual da secao (sem bytes no arquivo)."""
    pe, opt, sec_raw, va = 0x40, 0x40 + 24, 0x200, 0x1000
    optsize = 112 + 16 * 8
    img = bytearray(0x1000)  # o arquivo tem bytes depois dos dados da secao (o que a cauda virtual leria)
    img[0:2] = b'MZ'
    struct.pack_into('<I', img, 0x3C, pe)
    img[pe:pe + 4] = b'PE\0\0'
    struct.pack_into('<HH', img, pe + 4, 0x8664, 1)
    struct.pack_into('<H', img, pe + 20, optsize)
    struct.pack_into('<H', img, opt, 0x20B)
    struct.pack_into('<I', img, opt + 108, 16)
    struct.pack_into('<II', img, opt + 112 + 8 * 1, va + 0x00, 40)  # imports: 1 descritor + o nulo
    struct.pack_into('<II', img, opt + 112 + 8 * 13, va + 0x40, 64)  # delay-load: 1 descritor + o nulo
    s = opt + optsize
    img[s:s + 8] = b'.idata\0\0'
    struct.pack_into('<IIII', img, s + 8, 0x1000, va, 0x200, sec_raw)  # cauda virtual de 0x200 a 0x1000
    struct.pack_into('<IIIII', img, sec_raw + 0x00, va + 0x100, 0, 0, va + 0x80, va + 0x100)
    b_name = va + 0x800 if delay_name_in_tail else va + 0x90
    struct.pack_into('<II', img, sec_raw + 0x40, 1, b_name)
    img[sec_raw + 0x80:sec_raw + 0x86] = b'A.dll\0'
    img[sec_raw + 0x90:sec_raw + 0x96] = b'B.dll\0'
    img[sec_raw + 0x800:sec_raw + 0x806] = b'Z.dll\0'
    return bytes(img)


def autoteste():
    """o parser do PE (imports, delay-load, limites) e o lexer sobre entradas sinteticas."""
    import tempfile
    fails = []
    if pe_imports(synthetic_pe()) != ['a.dll', 'b.dll']:
        fails.append('imports e delay-load: %r' % pe_imports(synthetic_pe()))
    try:
        pe_imports(synthetic_pe(delay_name_in_tail=True))
        fails.append('RVA na cauda virtual aceito')
    except ValueError as e:
        if 'RVA fora' not in str(e):
            fails.append('RVA na cauda virtual: erro errado (%s)' % e)
    src = r'''// "comentario de linha"
/* "comentario de bloco" */ const char* a = "url http://x/y // nao e comentario";
char q = '"'; const char* b = "aspas \"dentro\" e\tescape"; const char* c = R"x(raw "com" aspas\n)x";
const wchar_t* d = L"largo nao conta"; const char* e = u8"prefixo u8"; const char* f = "hex \x41 descarta";
#define R "macro"
int uR = 0; const char* g = "fim";'''
    with tempfile.NamedTemporaryFile('w', suffix='.cpp', delete=False, encoding='utf-8') as t:
        t.write(src)
    try:
        got = literals(t.name)
    finally:
        os.unlink(t.name)
    want = {'url http://x/y // nao e comentario', 'aspas "dentro" e\tescape', 'raw "com" aspas\\n', 'prefixo u8',
            'macro', 'fim'}
    if got != want:
        fails.append('lexer: sobra %r, falta %r' % (sorted(got - want), sorted(want - got)))
    for f in fails:
        print('FALHA: ' + f)
    print('autoteste do fechamento: %s' % ('ok' if not fails else '%d falhas' % len(fails)))
    return 1 if fails else 0


if __name__ == '__main__':
    sys.exit(autoteste() if MUTAR == 'autoteste' else main())
