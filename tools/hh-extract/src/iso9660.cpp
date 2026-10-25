// SPDX-License-Identifier: MIT
#include "hh_extract/iso9660.hpp"

#include <cctype>
#include <cstring>

namespace hh {
namespace {

constexpr int kMaxDepth = 16;
constexpr uint32_t kMaxDirBytes = 8u * 1024 * 1024;

uint32_t le32(const uint8_t* p) {
    return uint32_t(p[0]) | uint32_t(p[1]) << 8 | uint32_t(p[2]) << 16 | uint32_t(p[3]) << 24;
}

// Nomes vem do disco (dado nao confiavel): apenas caracteres de nome
// ISO9660, sem componentes especiais e sem version ";n".
std::string sanitizeName(const uint8_t* p, size_t n) {
    std::string s(reinterpret_cast<const char*>(p), n);
    if (auto semi = s.find(';'); semi != std::string::npos) s.resize(semi);
    if (!s.empty() && s.back() == '.') s.pop_back();
    if (s.empty() || s == "." || s == "..") {
        throw DiscError("nome de arquivo ISO9660 invalido");
    }
    for (char c : s) {
        if (!(std::isalnum(static_cast<unsigned char>(c)) || c == '_' || c == '.' || c == '-')) {
            throw DiscError("caractere invalido em nome ISO9660: " + s);
        }
    }
    return s;
}

bool iequals(const std::string& a, const std::string& b) {
    if (a.size() != b.size()) return false;
    for (size_t i = 0; i < a.size(); ++i) {
        if (std::toupper(static_cast<unsigned char>(a[i]))
            != std::toupper(static_cast<unsigned char>(b[i]))) {
            return false;
        }
    }
    return true;
}

} // namespace

Iso9660::Iso9660(CdiImage& img, uint32_t pvdLba) : img_(img) {
    uint8_t pvd[2048];
    img_.readUserData(pvdLba, pvd);
    if (pvd[0] != 1 || std::memcmp(pvd + 1, "CD001", 5) != 0) {
        throw DiscError("PVD ausente na LBA " + std::to_string(pvdLba));
    }
    // volume identificador em [40,72); busca o fim real (semriage)
    volume_id_.assign(reinterpret_cast<const char*>(pvd) + 40, 32);
    while (!volume_id_.empty() && (volume_id_.back() == ' ' || volume_id_.back() == '\0')) {
        volume_id_.pop_back();
    }
    const uint32_t rootLba = le32(pvd + 158);
    const uint32_t rootSize = le32(pvd + 166);
    walk(rootLba, rootSize, "", 0);
}

void Iso9660::walk(uint32_t lba, uint32_t size, const std::string& prefix, int depth) {
    if (depth > kMaxDepth) throw DiscError("aninhamento ISO9660 profundo demais");
    if (size > kMaxDirBytes) throw DiscError("diretorio ISO9660 suspeitamente grande");
    const uint32_t nSectors = (size + 2047) / 2048;
    std::vector<uint8_t> buf(2048);
    for (uint32_t s = 0; s < nSectors; ++s) {
        // ISO9660 usa LBA absoluta do disco (sem deslocamento de faixa).
        img_.readUserData(lba + s, buf.data());
        size_t o = 0;
        while (o < 2048) {
            const uint8_t rlen = buf[o];
            if (rlen == 0) break; // registros nao cruzam setores: resto e pad
            if (o + rlen > 2048) throw DiscError("registro de diretorio ISO9660 corrompido");
            const uint8_t* rec = buf.data() + o;
            o += rlen;
            if (rlen < 34) throw DiscError("registro ISO9660 corrompido");
            const uint8_t nameLen = rec[32];
            if (33u + nameLen > rlen) throw DiscError("nome ISO9660 fora do registro");
            const uint8_t* nm = rec + 33;
            if (nameLen == 1 && (nm[0] == 0 || nm[0] == 1)) continue; // "." / ".."
            const std::string name = sanitizeName(nm, nameLen);
            IsoEntry e;
            e.lba = le32(rec + 2);
            e.size = le32(rec + 10);
            e.is_dir = (rec[25] & 2) != 0;
            e.path = prefix + name;
            entries_.push_back(e);
            if (e.is_dir) {
                walk(e.lba, e.size, e.path + "/", depth + 1);
            }
        }
    }
}

const IsoEntry* Iso9660::find(const std::string& path) const {
    for (const auto& e : entries_) {
        if (iequals(e.path, path)) return &e;
    }
    return nullptr;
}

void Iso9660::read(const IsoEntry& e, const std::function<void(const uint8_t*, size_t)>& sink) {
    if (e.is_dir) throw DiscError("extent de diretorio nao tem leitura de arquivo");
    uint64_t left = e.size;
    uint32_t lba = e.lba;
    while (left > 0) {
        uint8_t buf[2048];
        img_.readUserData(lba, buf);
        const size_t take = left < 2048 ? (size_t)left : 2048;
        sink(buf, take);
        left -= take;
        ++lba;
    }
}

std::vector<uint8_t> Iso9660::readAll(const IsoEntry& e) {
    std::vector<uint8_t> out((size_t)e.size);
    size_t off = 0;
    read(e, [&](const uint8_t* p, size_t n) {
        std::memcpy(out.data() + off, p, n);
        off += n;
    });
    return out;
}

} // namespace hh
