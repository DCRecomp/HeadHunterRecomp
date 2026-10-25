// SPDX-License-Identifier: MIT
#include "hh_extract/cdi.hpp"

#include <array>
#include <cstring>

namespace hh {
namespace {

constexpr uint32_t kCdiV2 = 0x80000004;
constexpr uint32_t kCdiV3 = 0x80000005;
constexpr uint32_t kCdiV35 = 0x80000006;

// marca de faixa (20 bytes) presente no cabecalho antes de cada descritor
constexpr std::array<uint8_t, 20> kMark = {
    0, 0, 1, 0, 0, 0, 0xFF, 0xFF, 0xFF, 0xFF,
    0, 0, 1, 0, 0, 0, 0xFF, 0xFF, 0xFF, 0xFF
};

// Leitor linear little-endian do conteudo do cabecalho.
struct Reader {
    const std::vector<uint8_t>& b;
    size_t pos = 0;

    void need(size_t n) const {
        if (pos + n > b.size()) throw DiscError("cabecalho CDI truncado");
    }
    void skip(size_t n) {
        need(n);
        pos += n;
    }
    uint8_t u8() {
        need(1);
        return b[pos++];
    }
    uint16_t u16() {
        need(2);
        uint16_t v = uint16_t(b[pos]) | uint16_t(b[pos + 1]) << 8;
        pos += 2;
        return v;
    }
    uint32_t u32() {
        need(4);
        uint32_t v = uint32_t(b[pos]) | uint32_t(b[pos + 1]) << 8 | uint32_t(b[pos + 2]) << 16
                   | uint32_t(b[pos + 3]) << 24;
        pos += 4;
        return v;
    }
    bool matches(const std::array<uint8_t, 20>& pat) const {
        if (pos + pat.size() > b.size()) return false;
        return std::memcmp(b.data() + pos, pat.data(), pat.size()) == 0;
    }
};

uint32_t sectorSizeForCode(uint32_t code) {
    switch (code) {
    case 0: return 2048;
    case 1: return 2336;
    case 2: return 2352;
    default: throw DiscError("codigo de tamanho de setor CDI desconhecido: " + std::to_string(code));
    }
}

} // namespace

uint32_t Track::userOffset() const {
    if (sector_size == 2048) return 0;
    if (sector_size == 2336) return 8; // subcabecalho Mode2
    if (sector_size == 2352) return mode == 1 ? 16 : 24;
    throw DiscError("tamanho de setor invalido: " + std::to_string(sector_size));
}

CdiImage::CdiImage(const std::filesystem::path& path) : path_(path) {
    file_.open(path, std::ios::binary);
    if (!file_) throw DiscError("falha ao abrir imagem '" + path.string() + "'");
    file_.seekg(0, std::ios::end);
    file_size_ = (uint64_t)file_.tellg();
    if (file_size_ < 40) throw DiscError("arquivo pequeno demais para CDI");
    // rodape: [u32 versao][u32 bytes de cabecalho]
    file_.seekg((std::streamoff)(file_size_ - 8), std::ios::beg);
    std::array<uint8_t, 8> tail{};
    file_.read(reinterpret_cast<char*>(tail.data()), 8);
    if ((uint64_t)file_.gcount() != 8) throw DiscError("rodape CDI ilegivel");
    const uint32_t version = uint32_t(tail[0]) | uint32_t(tail[1]) << 8 | uint32_t(tail[2]) << 16
                           | uint32_t(tail[3]) << 24;
    const uint32_t ho = uint32_t(tail[4]) | uint32_t(tail[5]) << 8 | uint32_t(tail[6]) << 16
                      | uint32_t(tail[7]) << 24;
    if (version != kCdiV2 && version != kCdiV3 && version != kCdiV35) {
        throw DiscError("versao CDI nao reconhecida: " + std::to_string(version));
    }
    switch (version) {
    case kCdiV2: format_ = "CDI v2"; break;
    case kCdiV3: format_ = "CDI v3"; break;
    default: format_ = "CDI v3.5"; break;
    }
    // v2: cabecalho literal no offset gravado; v3/v3.5: cabecalho ao fim dos dados.
    header_pos_ = (version == kCdiV2) ? ho : file_size_ - ho;
    if (ho == 0 || header_pos_ >= file_size_ - 8) {
        throw DiscError("offset de cabecalho CDI invalido");
    }
    const size_t headerLen = (size_t)(file_size_ - 8 - header_pos_);
    std::vector<uint8_t> content(headerLen);
    file_.clear();
    file_.seekg((std::streamoff)header_pos_, std::ios::beg);
    file_.read(reinterpret_cast<char*>(content.data()), (std::streamsize)headerLen);
    if ((size_t)file_.gcount() != headerLen) throw DiscError("cabecalho CDI ilegivel");
    parseHeader(content, version);
}

void CdiImage::parseHeader(const std::vector<uint8_t>& content, uint32_t version) {
    Reader r{content};
    const uint16_t sessions = r.u16();
    if (sessions == 0 || sessions > 99) {
        throw DiscError("numero de sessoes CDI invalido: " + std::to_string(sessions));
    }
    uint64_t cursor = 0; // bytes brutos acumulados das faixas anteriores
    for (uint32_t s = 1; s <= sessions; ++s) {
        const uint16_t count = r.u16();
        if (count > 99) {
            throw DiscError("numero de faixas CDI invalido na sessao " + std::to_string(s));
        }
        for (uint16_t k = 0; k < count; ++k) {
            if (r.u32() != 0) r.skip(8); // dados extras de versoes 3.00.780+
            if (!r.matches(kMark)) throw DiscError("marca de faixa CDI ausente");
            const size_t namePos = r.pos + 20 + 4; // nome fica a marca+0x18 (u8 length prefix)
            r.skip(20);
            r.skip(4);
            const uint8_t nl = r.u8();
            std::string name;
            if (nl > 0 && namePos + 1 + nl <= content.size()) {
                name.assign(reinterpret_cast<const char*>(content.data()) + namePos + 1, nl);
            }
            r.skip(nl);
            r.skip(11 + 4 + 4);
            if (r.u32() == 0x80000000) r.skip(8); // variante DiscJuggler-4
            r.skip(2);
            Track t;
            t.pregap = r.u32();
            t.length = r.u32();
            r.skip(6);
            t.mode = r.u32();
            if (t.mode > 2) throw DiscError("modo de faixa CDI desconhecido: " + std::to_string(t.mode));
            r.skip(12);
            t.start_lba = r.u32();
            t.total = r.u32();
            r.skip(16);
            const uint32_t code = r.u32();
            t.sector_size = sectorSizeForCode(code);
            t.code = code;
            r.skip(29);
            if (version != kCdiV2) {
                r.skip(5);
                if (r.u32() == 0xFFFFFFFF) r.skip(78);
            }
            if (t.total < t.pregap + t.length) {
                throw DiscError("faixa CDI com comprimento inconsistente");
            }
            t.file_offset = cursor + (uint64_t)t.pregap * t.sector_size;
            cursor += (uint64_t)t.total * t.sector_size;
            if (cursor > header_pos_) throw DiscError("faixa CDI ultrapassa o fim dos dados");
            t.session = s;
            t.number = (uint32_t)tracks_.size() + 1;
            t.name = name;
            tracks_.push_back(t);
        }
        r.skip(4 + 8); // trilha de fim de sessao
        if (version != kCdiV2) r.skip(1);
    }
    if (tracks_.empty()) throw DiscError("imagem CDI sem faixas");
}

const Track& CdiImage::bootTrack() const {
    for (size_t i = tracks_.size(); i-- > 0;) {
        if (tracks_[i].isData()) return tracks_[i];
    }
    throw DiscError("imagem CDI sem faixa de dados (boot)");
}

const Track* CdiImage::findTrack(uint32_t lba) const {
    for (const auto& t : tracks_) {
        if (t.contains(lba)) return &t;
    }
    throw DiscError("LBA " + std::to_string(lba) + " fora de todas as faixas");
}

void CdiImage::readUserData(uint32_t lba, void* dst) {
    const Track* t = findTrack(lba);
    file_.clear();
    file_.seekg((std::streamoff)t->pos(lba), std::ios::beg);
    file_.read(reinterpret_cast<char*>(dst), 2048);
    if (file_.gcount() != 2048) throw DiscError("leitura truncada na LBA " + std::to_string(lba));
}

std::vector<uint8_t> CdiImage::readExtentAll(uint32_t lba, uint64_t size) {
    std::vector<uint8_t> out((size_t)size);
    readExtent(lba, size, [&out, off = size_t(0)](const uint8_t* p, size_t n) mutable {
        std::memcpy(out.data() + off, p, n);
        off += n;
    });
    return out;
}

} // namespace hh
