"""Confere que o icone de executaveis Windows e o de um .ico (lido do recurso do binario, nao do build).

Uso: python tools/dev/icone_exe.py <arquivo.ico> <exe> [<exe> ...]
Le o grupo de icones 1 (RT_GROUP_ICON, o "1 ICON" de app/res/headhunter.rc.in) e cada imagem (RT_ICON) pela API de
recursos do Windows, e compara com as imagens do .ico, na ordem. Saida 0 se todos iguais; 1 se algum diferir ou nao tiver
icone. Cada conferencia e repetida com um .ico alterado em um byte, que precisa ser recusado (portado do Blue Stinger,
MIT; aqui varios executaveis e o caso negativo).
"""
import ctypes
import struct
import sys
from ctypes import wintypes

RT_ICON, RT_GROUP_ICON = 3, 14
LOAD_LIBRARY_AS_DATAFILE, LOAD_LIBRARY_AS_IMAGE_RESOURCE = 0x2, 0x20

k32 = ctypes.WinDLL("kernel32", use_last_error=True)
k32.LoadLibraryExW.argtypes = [wintypes.LPCWSTR, wintypes.HANDLE, wintypes.DWORD]
k32.LoadLibraryExW.restype = ctypes.c_void_p
k32.FindResourceW.argtypes = [ctypes.c_void_p, ctypes.c_void_p, ctypes.c_void_p]
k32.FindResourceW.restype = ctypes.c_void_p
k32.LoadResource.argtypes = [ctypes.c_void_p, ctypes.c_void_p]
k32.LoadResource.restype = ctypes.c_void_p
k32.LockResource.argtypes = [ctypes.c_void_p]
k32.LockResource.restype = ctypes.c_void_p
k32.SizeofResource.argtypes = [ctypes.c_void_p, ctypes.c_void_p]
k32.SizeofResource.restype = wintypes.DWORD
k32.FreeLibrary.argtypes = [ctypes.c_void_p]


def recurso(h, nome, tipo):
    r = k32.FindResourceW(h, ctypes.c_void_p(nome), ctypes.c_void_p(tipo))
    if not r:
        return None
    return ctypes.string_at(k32.LockResource(k32.LoadResource(h, r)), k32.SizeofResource(h, r))


def imagens_exe(exe):
    h = k32.LoadLibraryExW(exe, None, LOAD_LIBRARY_AS_DATAFILE | LOAD_LIBRARY_AS_IMAGE_RESOURCE)
    if not h:
        raise SystemExit(f"FALHA nao abre {exe} (erro {ctypes.get_last_error()})")
    try:
        g = recurso(h, 1, RT_GROUP_ICON)
        if g is None or len(g) < 6:
            return None
        n = struct.unpack_from("<H", g, 4)[0]
        ids = [struct.unpack_from("<H", g, 6 + 14 * i + 12)[0] for i in range(n)]
        return [recurso(h, i, RT_ICON) for i in ids]
    finally:
        k32.FreeLibrary(h)


def imagens_ico(ico):
    with open(ico, "rb") as f:
        b = f.read()
    res, tipo, n = struct.unpack_from("<HHH", b, 0)
    if res != 0 or tipo != 1 or n == 0:
        raise SystemExit(f"FALHA {ico} nao e um .ico")
    out = []
    for i in range(n):
        tam, off = struct.unpack_from("<II", b, 6 + 16 * i + 8)
        out.append(b[off:off + tam])
    return out


def iguais(a, b):
    return a is not None and len(a) == len(b) and all(x == y for x, y in zip(a, b))


def main():
    if len(sys.argv) < 3:
        raise SystemExit(__doc__)
    b = imagens_ico(sys.argv[1])
    alterado = list(b)
    alterado[-1] = alterado[-1][:-1] + bytes([alterado[-1][-1] ^ 1])
    falhas = 0
    for exe in sys.argv[2:]:
        a = imagens_exe(exe)
        if a is None:
            print(f"FALHA {exe}: sem o grupo de icones 1")
            falhas += 1
        elif not iguais(a, b):
            print(f"FALHA {exe}: icone diferente ({len(a)} imagens no exe, {len(b)} no ico)")
            falhas += 1
        elif iguais(a, alterado):
            print(f"FALHA {exe}: a comparacao aceitou um .ico alterado")
            falhas += 1
        else:
            print(f"ok {exe}: icone igual ao .ico ({len(a)} imagens, {sum(map(len, a))} bytes)")
    return 1 if falhas else 0


if __name__ == "__main__":
    sys.exit(main())
