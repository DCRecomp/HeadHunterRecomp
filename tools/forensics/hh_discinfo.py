#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
"""hh_discinfo.py — segunda implementação independente de leitura forense CDI/ISO9660.

Implementação própria (nenhum código copiado da referência MIT), validada
empiricamente contra bs-extract em 'ISO/HeadHunter (Disc 1|2).cdi':
  - rodapé/cabeçalho CDI v3.5 (rodapé: [u32 versão][u32 bytes de cabeçalho|ho];
    header_pos = size-ho, cabeçalho = [header_pos, size-8));
  - entradas de faixa em leitor linear a partir do início do cabeçalho
    (sessões/contagens; por faixa: u32 extra condicional, marca de 20 bytes,
    skip 4, nome prefixado por u8, skip 19, u32 0x80000000 condicional, skip 2,
    pregap, length, skip 6, modo, skip 12, start_lba, total, skip 16,
    código de setor, skip 29; para v!=v2: skip 5 e skip 78 condicional);
  - contorno de dados: cursor = soma dos totals das faixas anteriores;
    file_offset = cursor + pregap*tamanho_setor; cursor += total*tamanho_setor;
  - setor bruto 2336 (Modo 2): subcabeçalho em +8, usuário 2048, ECC até 2336;
    dados de usuário sempre lidos com passada por setor (nunca contínua);
  - IP.BIN = primeiros 16 setores do usuário da faixa de boot (32 768 bytes);
  - uma árvore ISO9660 por faixa de dados (PVD na LBA absoluta start_lba+16);
  - hashes SHA-256/CRC32 por arquivo em streaming com metadados no JSON;
  - extração segura (recusa traversal e caminhos absolutos).

Somente metadados vão ao terminal. Saídas extensas vão para caminhos ignorados.
"""
from __future__ import annotations

import argparse
import hashlib
import io
import json
import os
import struct
import sys
import zlib

MARK = bytes([0, 0, 1, 0, 0, 0, 0xFF, 0xFF, 0xFF, 0xFF,
              0, 0, 1, 0, 0, 0, 0xFF, 0xFF, 0xFF, 0xFF])
CDI_V2, CDI_V3, CDI_V35 = 0x80000004, 0x80000005, 0x80000006
SECTOR_SIZE = {0: 2048, 1: 2336, 2: 2352}
USER_CHUNK = 1 << 20


class CdiError(Exception):
    pass


LE = "little"


class Reader:
    """Leitor linear little-endian do cabeçalho (equivalente ao esquema do formato)."""

    def __init__(self, buf):
        self.b = buf
        self.pos = 0

    def _need(self, n):
        if self.pos + n > len(self.b):
            raise CdiError(f"cabecalho CDI truncado (offset {self.pos}, faltam {n})")

    def skip(self, n):
        self._need(n)
        self.pos += n

    def u8(self):
        self._need(1)
        v = self.b[self.pos]
        self.pos += 1
        return v

    def u16(self):
        self._need(2)
        v = int.from_bytes(self.b[self.pos:self.pos + 2], LE)
        self.pos += 2
        return v

    def u32(self):
        self._need(4)
        v = int.from_bytes(self.b[self.pos:self.pos + 4], LE)
        self.pos += 4
        return v

    def matches(self, pat, n=None):
        n = len(pat) if n is None else n
        return self.pos + n <= len(self.b) and self.b[self.pos:self.pos + n] == pat[:n]


def u(buf, pos, n):
    if pos < 0 or pos + n > len(buf):
        raise CdiError(f"leitura fora do buffer (pos {pos}, n {n}, buf {len(buf)})")
    return int.from_bytes(buf[pos:pos + n], LE)


class Track:
    def __init__(self, session, number, mode, pregap, length, start_lba, total,
                 sector_size, file_offset, code):
        self.session = session
        self.number = number
        self.mode = mode
        self.pregap = pregap
        self.length = length
        self.start_lba = start_lba
        self.total = total
        self.sector_size = sector_size
        self.file_offset = file_offset
        self.code = code
        self.name = None

    def user_offset(self):
        if self.sector_size == 2048:
            return 0
        if self.sector_size == 2336:
            return 8
        if self.sector_size == 2352:
            return 16 if self.mode == 1 else 24  # Mode1: sync+cabecalho; resto: +subcabec
        raise CdiError(f"tamanho de setor invalido: {self.sector_size}")

    def contains(self, lba):
        return self.start_lba <= lba < self.start_lba + self.length

    def end_lba(self):
        return self.start_lba + self.length

    def is_data(self):
        return self.mode != 0

    def pos(self, lba):
        return self.file_offset + (lba - self.start_lba) * self.sector_size + self.user_offset()

    def as_dict(self):
        return {"session": self.session, "number": self.number,
                "mode": self.mode, "pregap": self.pregap, "length": self.length,
                "start_lba": self.start_lba, "total": self.total,
                "sector_code": self.code, "sector_size": self.sector_size,
                "file_offset": self.file_offset, "user_offset": self.user_offset(),
                "name": self.name}


class CdiImage:
    """Leitor CDI v3.5 com duas sessões (formato observado nos discos alvo)."""

    def __init__(self, path):
        self.path = path
        size = os.path.getsize(path)
        if size < 40:
            raise CdiError("arquivo pequeno demais para CDI")
        with open(path, "rb") as f:
            f.seek(size - 8)
            version, ho = struct.unpack("<II", f.read(8))
        if version not in (CDI_V2, CDI_V3, CDI_V35):
            raise CdiError(f"versao CDI nao reconhecida: 0x{version:08X}")
        self.cdi = version
        self.format = {CDI_V2: "CDI v2", CDI_V3: "CDI v3", CDI_V35: "CDI v3.5"}[version]
        if version == CDI_V2:
            header_pos = ho  # v2: cabecalho literal no offset gravado
        else:
            header_pos = size - ho  # v3/v3.5: cabecalho ao fim dos dados
        if ho == 0 or header_pos >= size - 8:
            raise CdiError("offset de cabecalho CDI invalido")
        header_len = size - 8 - header_pos
        with open(path, "rb") as f:
            f.seek(header_pos)
            content = f.read(header_len)
        self.header_pos = header_pos
        self.tracks = self._parse_content(content, version)

    # ---------------------------------------------------------------- parser
    def _parse_content(self, content, version):
        """Leitor linear do cabeçalho: sessões/contagens e descritores encadeados."""
        r = Reader(content)
        sessions = r.u16()
        if not 0 < sessions <= 99:
            raise CdiError(f"numero de sessoes CDI invalido: {sessions}")
        tracks = []
        cursor = 0  # bytes de dados brutos das faixas anteriores (contugo do arquivo)
        for s in range(1, sessions + 1):
            count = r.u16()
            if count > 99:
                raise CdiError(f"numero de faixas CDI invalido na sessao {s}: {count}")
            for _ in range(count):
                if r.u32() != 0:
                    r.skip(8)  # dados extras de versoes 3.00.780+
                if not r.matches(MARK):
                    raise CdiError(f"marca de faixa CDI ausente no offset {r.pos}")
                name_pos = r.pos + 20 + 4  # nome fica a marca+0x18, prefixado por u8
                r.skip(20)
                r.skip(4)
                nl = r.u8()
                name = None
                if 0 < nl <= 255:
                    if name_pos + 1 + nl <= len(content):
                        name = content[name_pos + 1:name_pos + 1 + nl].decode("ascii", "replace")
                r.skip(nl)
                r.skip(11 + 4 + 4)
                if r.u32() == 0x80000000:
                    r.skip(8)  # variante DiscJuggler 4
                r.skip(2)
                pregap = r.u32()
                length = r.u32()
                r.skip(6)
                mode = r.u32()
                if mode > 2:
                    raise CdiError(f"modo de faixa CDI desconhecido: {mode}")
                r.skip(12)
                start_lba = r.u32()
                total = r.u32()
                r.skip(16)
                code = r.u32()
                if code not in SECTOR_SIZE:
                    raise CdiError(f"codigo de tamanho de setor CDI desconhecido: {code}")
                r.skip(29)
                if version != CDI_V2:
                    r.skip(5)
                    if r.u32() == 0xFFFFFFFF:
                        r.skip(78)
                if total < pregap + length:
                    raise CdiError("faixa CDI com comprimento inconsistente")
                file_offset = cursor + pregap * SECTOR_SIZE[code]
                cursor += total * SECTOR_SIZE[code]
                if cursor > self.header_pos:
                    raise CdiError("faixa CDI ultrapassa o fim dos dados")
                t = Track(s, len(tracks) + 1, mode, pregap, length, start_lba,
                          total, SECTOR_SIZE[code], file_offset, code)
                t.name = name
                tracks.append(t)
            r.skip(4 + 8)  # trilha de fim de sessao
            if version != CDI_V2:
                r.skip(1)
        if not tracks:
            raise CdiError("imagem CDI sem faixas")
        return tracks

    # -------------------------------------------------------------- leitura
    def _find_track(self, lba):
        for t in self.tracks:
            if t.contains(lba):
                return t
        raise CdiError(f"LBA {lba} fora de todas as faixas")

    def read_user(self, lba):
        t = self._find_track(lba)
        pos = t.pos(lba)
        with open(self.path, "rb") as f:
            f.seek(pos)
            data = f.read(2048)
        if len(data) != 2048:
            raise CdiError(f"leitura truncada na LBA {lba}")
        return data

    def iter_file(self, lba, size):
        """Itera dados de usuario de um extent, com passada por setor e troca de faixa."""
        remaining = size
        lba_cur = lba
        buf = b""
        while remaining > 0:
            t = self._find_track(lba_cur)
            with open(self.path, "rb") as f:
                f.seek(t.pos(lba_cur))
                block = f.read(2048)
            if len(block) != 2048:
                raise CdiError(f"leitura truncada na LBA {lba_cur}")
            take = min(remaining, 2048)
            yield block[:take]
            remaining -= take
            lba_cur += 1

    def hash_file(self, lba, size, algo="sha256"):
        h = None
        n = 0
        if algo == "crc32":
            acc = 0
            for blk in self.iter_file(lba, size):
                n += len(blk)
                acc = zlib.crc32(blk, acc)
            return f"{acc & 0xFFFFFFFF:08x}"
        h = hashlib.new(algo)
        for blk in self.iter_file(lba, size):
            n += len(blk)
            h.update(blk)
        return h.hexdigest()

    def extract_file(self, lba, size, dest):
        os.makedirs(os.path.dirname(dest), exist_ok=True)
        with open(dest, "wb") as out:
            for blk in self.iter_file(lba, size):
                out.write(blk)

    # --------------------------------------------------------------- ISO9660
    def pvd_of(self, track):
        lba = track.start_lba + 16
        data = self.read_user(lba)
        if data[0] != 1 or data[1:6] != b"CD001":
            raise CdiError(f"PVD ausente na LBA {lba} da faixa {track.number}")
        return data

    def iso_tree(self, track, depth_limit=16):
        pvd = self.pvd_of(track)
        meta = {
            "system": pvd[8:40].decode("ascii", "replace").strip(),
            "volume": pvd[40:72].decode("ascii", "replace").strip(),
            "root_lba": u(pvd, 158, 4),
            "root_size": u(pvd, 166, 4),
        }
        out = []
        self._walk(track, meta["root_lba"], meta["root_size"], "", 0, out, depth_limit)
        return meta, out

    def _walk(self, track, lba, size, prefix, depth, out, depth_limit):
        if depth > depth_limit:
            raise CdiError("aninhamento ISO9660 profundo demais")
        if size > 1024 * 1024 * 8:
            raise CdiError("diretorio ISO9660 suspeitamente grande")
        for s in range((size + 2047) // 2048):
            buf = self.read_user(lba + s)  # ISO9660 usa LBA absoluta do disco
            o = 0
            while o < 2048:
                rlen = buf[o]
                if rlen == 0:
                    break
                rec = buf[o:o + rlen]
                o += rlen
                if len(rec) < 34:
                    raise CdiError("registro ISO9660 corrompido")
                namelen = rec[32]
                if 33 + namelen > rlen:
                    raise CdiError("nome ISO9660 forca o limite do registro")
                nm = rec[33:33 + namelen]
                if namelen == 1 and nm in (b"\x00", b"\x01"):
                    continue
                name = nm.decode("ascii", "replace").split(";")[0].rstrip(".")
                if not name:
                    raise CdiError("nome ISO9660 vazio")
                for c in name:
                    if c in "\\/:*?\"<>|":
                        raise CdiError(f"caractere invalido no nome: {name!r}")
                e_lba = u(rec, 2, 4)
                e_size = u(rec, 10, 4)
                is_dir = bool(rec[25] & 2)
                path = f"{prefix}/{name}" if prefix else f"/{name}"
                out.append({"path": path, "lba": e_lba, "size": e_size,
                            "is_dir": is_dir, "origin_track": track.number})
                if is_dir:
                    self._walk(track, e_lba, e_size, path, depth + 1, out, depth_limit)


# ------------------------------------------------------------------ IP.BIN
def field(buf, off, ln):
    s = "".join(chr(c) if 0x20 <= c < 0x7F else "?" for c in buf[off:off + ln])
    return s.rstrip(" ")


def parse_ip_bin(data):
    if len(data) < 0xC00:
        raise CdiError(f"IP.BIN truncado ({len(data)} bytes; esperado >= 0xC00)")
    info = {
        "hardware_id": field(data, 0x00, 16),
        "maker_id": field(data, 0x10, 16),
        "device_info": field(data, 0x20, 16),
        "area": field(data, 0x30, 8),
        "peripherals": field(data, 0x38, 8),
        "product": field(data, 0x40, 10),
        "version": field(data, 0x4A, 6),
        "release_date": field(data, 0x50, 16),
        "boot_file": field(data, 0x60, 16),
        "company": field(data, 0x70, 16),
        "title": field(data, 0x80, 96),
    }
    try:
        info["peripherals_mask"] = int(info["peripherals"] or "0", 16)
    except ValueError:
        info["peripherals_mask"] = None
    info["windows_ce"] = bool(info["peripherals_mask"] & 0x1) if info["peripherals_mask"] is not None else None
    md5_area = data[0x900:0xC00]
    info["md5_area_hex"] = md5_area[:32].hex() + ("..." if len(md5_area) > 32 else "")
    info["md5_area_nonzero"] = any(b for b in md5_area)
    if info["hardware_id"] != "SEGA SEGAKATANA":
        raise CdiError(f"IP.BIN invalido: hardware_id '{info['hardware_id']}'")
    per = info["peripherals"]
    if per and not all(c in "0123456789ABCDEF" for c in per):
        raise CdiError(f"perifericos IP.BIN invalidos: '{per}'")
    return info


def ip_bin_buffer(img):
    t = img.tracks[-1]  # faixa de boot (ultima sessao)
    out = bytearray()
    for i in range(16):
        out += img.read_user(t.start_lba + i)
    return t, bytes(out)


# --------------------------------------------------------------------- CLI
def cmd_info(img, as_json, tree_meta=False):
    boot_track, ip_buf = ip_bin_buffer(img)
    ip = parse_ip_bin(ip_buf)
    md = {
        "image": os.path.basename(img.path),
        "size": os.path.getsize(img.path),
        "format": img.format,
        "header": {"content_start": img.header_pos,
                   "content_end": os.path.getsize(img.path) - 8},
        "tracks": [t.as_dict() for t in img.tracks],
        "ip_bin": ip,
        "boot_track": boot_track.as_dict(),
    }
    trees = {}
    for t in img.tracks:
        try:
            meta, entries = img.iso_tree(t)
            files = [e for e in entries if not e["is_dir"]]
            trees[f"track{t.number}"] = {
                "pvd": meta,
                "n_entries": len(entries),
                "n_files": len(files),
                "bytes": sum(f["size"] for f in files),
            }
        except CdiError as exc:
            trees[f"track{t.number}"] = {"erro": str(exc)}
    md["iso_trees"] = trees
    if tree_meta:
        md["iso_tree_detail"] = {}
        for t in img.tracks:
            try:
                _, entries = img.iso_tree(t)
                md["iso_tree_detail"][f"track{t.number}"] = entries
            except CdiError:
                pass
    if as_json:
        print(json.dumps(md, indent=2, ensure_ascii=False))
        return 0
    print(f"Imagem: {md['image']} ({img.format}, {md['size']} bytes)")
    for t in img.tracks:
        print(f"  faixa {t.session}#{t.number} mode{t.mode} LBA {t.start_lba} "
              f"+{t.length} de {t.sector_size} (total {t.total}, pregap {t.pregap}, "
              f"dados@{t.file_offset})")
    for k, v in ip.items():
        if k not in ("md5_area_hex", "peripherals_mask"):
            print(f"  {k}: {v}")
    print(f"  peripherals: {ip['peripherals']} mask {ip['peripherals_mask']} windows_ce={ip['windows_ce']}")
    print(f"  md5_area: {ip['md5_area_hex']} presente={ip['md5_area_nonzero']}")
    for key, meta in trees.items():
        print(f"  {key}: {meta}")
    return 0


def cmd_tree(img, out_path):
    boot, ip_buf = ip_bin_buffer(img)
    ip = parse_ip_bin(ip_buf)
    combined = {}
    trees = {}
    for t in img.tracks:
        meta, entries = img.iso_tree(t)
        trees[t.number] = {"pvd": meta}
        for e in entries:
            if e["is_dir"]:
                continue
            key = e["path"]
            rec = {"lba": e["lba"], "size": e["size"], "track": e["origin_track"]}
            prev = combined.get(key)
            if prev is not None and (prev["lba"], prev["size"]) != (e["lba"], e["size"]):
                combined.setdefault(key, rec)
                rec["conflict"] = {"track": prev["track"], "lba": prev["lba"], "size": prev["size"]}
            else:
                combined[key] = rec
    files = list(combined)
    result = {
        "image": os.path.basename(img.path),
        "size": os.path.getsize(img.path),
        "format": img.format,
        "ip_bin": ip,
        "trees": {f"track{k}": v for k, v in trees.items()},
        "n_files": len(files),
        "bytes": sum(combined[f]["size"] for f in files),
    }
    hashes = {}
    for path in files:
        rec = combined[path]
        try:
            rec["sha256"] = img.hash_file(rec["lba"], rec["size"])
            rec["crc32"] = img.hash_file(rec["lba"], rec["size"], "crc32")
        except CdiError as exc:
            rec["sha256"] = None
            rec["erro"] = str(exc)
    result["files"] = {f: combined[f] for f in sorted(files)}
    os.makedirs(os.path.dirname(out_path) or ".", exist_ok=True)
    with open(out_path, "w", encoding="utf-8") as f:
        json.dump(result, f, indent=1, ensure_ascii=False)
    print(f"{result['n_files']} arquivos, {result['bytes']} bytes -> {out_path}")
    return 0


def safe_rel(path):
    parts = [p for p in path.replace("\\", "/").split("/") if p]
    for p in parts:
        if p in (".", "..") or (len(p) >= 2 and p[1] == ":"):
            raise CdiError(f"caminho inseguro recusado: {path}")
    if not parts:
        raise CdiError("caminho vazio")
    return parts


def cmd_extract(img, dest, wanted):
    tree = {}
    for t in img.tracks:
        try:
            _, entries = img.iso_tree(t)
        except CdiError:
            continue
        for e in entries:
            if e["is_dir"]:
                continue
            tree.setdefault(e["path"].lstrip("/").upper(),
                            (e["lba"], e["size"]))
    for want in wanted:
        key = want.strip().replace("\\", "/")
        hit = tree.get(key.upper())
        if hit is None:
            cands = [k for k in tree if k.split("/")[-1] == key.split("/")[-1].upper()]
            if len(cands) != 1:
                print(f"NAO ENCONTRADO/AMBIGUO: {want} (candidatos: {cands[:5]})", file=sys.stderr)
                return 1
            key = cands[0]
            hit = tree[key]
        lba, size = hit
        out_path = os.path.join(dest, *safe_rel(key))
        img.extract_file(lba, size, out_path)
        print(f"extraido {key} ({size} bytes) -> {out_path}")
    return 0


def main(argv=None):
    sys.stdout = io.TextIOWrapper(sys.stdout.buffer, encoding="utf-8", errors="replace")
    sys.stderr = io.TextIOWrapper(sys.stderr.buffer, encoding="utf-8", errors="replace")
    ap = argparse.ArgumentParser(description="Leitor forense CDI/ISO9660 independente")
    ap.add_argument("image")
    ap.add_argument("--info", action="store_true", help="metadados do disco")
    ap.add_argument("--json", action="store_true", help="saida do --info em JSON")
    ap.add_argument("--tree", metavar="SAIDA", help="arvore completa com hashes (JSON)")
    ap.add_argument("--extract", nargs=2, action="append", metavar=("DEST", "ARQ"),
                    help="extrai arquivo(s) da arvore; repita a opacao")
    args = ap.parse_args(argv)
    try:
        img = CdiImage(args.image)
        if args.info:
            return cmd_info(img, args.json)
        if args.tree:
            return cmd_tree(img, args.tree)
        if args.extract:
            dest = args.extract[0][0]
            wanted = [a[1] for a in args.extract]
            return cmd_extract(img, dest, wanted)
        ap.error("selecione --info, --tree ou --extract")
    except (CdiError, OSError) as exc:
        print(f"erro: {exc}", file=sys.stderr)
        return 1


if __name__ == "__main__":
    sys.exit(main())
