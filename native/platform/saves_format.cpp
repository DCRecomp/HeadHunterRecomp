// SPDX-License-Identifier: MIT
// Derivado de recomp-BStinger native/platform/saves_format.cpp (MIT, Copyright (c) 2026 Blue Stinger
// Recompiled contributors; ver third_party/licenses/recomp-BStinger-LICENSE.txt).
// HH11: formato dos saves (docs/planos/HH11-PLANO.md, "Slots, formato VMS, CRC e preservacao").
#include "platform/saves.h"

#include <cstring>

namespace hh::native::saves {

namespace {

constexpr size_t   kEnvHeader = 32;
constexpr uint8_t  kMagic[4]  = {'H', 'H', 'S', 'V'};

uint16_t rd16(std::span<const std::byte> d, size_t o) { return uint16_t(uint16_t(d[o]) | uint16_t(d[o + 1]) << 8); }
uint32_t rd32(std::span<const std::byte> d, size_t o) { return uint32_t(rd16(d, o)) | uint32_t(rd16(d, o + 2)) << 16; }
void     wr(std::vector<std::byte>& v, uint64_t x, int n) {
    for (int i = 0; i < n; ++i) v.push_back(std::byte(x >> (8 * i)));
}

} // namespace

const char* save_error_name(SaveError e) {
    switch (e) {
    case SaveError::None: return "ok";
    case SaveError::NoCard: return "sem cartao";
    case SaveError::Unformatted: return "cartao nao formatado";
    case SaveError::NotFound: return "save nao encontrado";
    case SaveError::ReadOnly: return "somente leitura";
    case SaveError::NoSpace: return "sem blocos livres";
    case SaveError::NoEntries: return "sem entradas livres";
    case SaveError::BadName: return "nome invalido";
    case SaveError::BadImage: return "imagem invalida";
    case SaveError::Corrupt: return "arquivo corrompido";
    case SaveError::FutureVersion: return "versao mais nova do formato";
    case SaveError::Io: return "erro de disco";
    }
    return "?";
}

uint16_t vms_crc16(std::span<const std::byte> data) {
    uint32_t c = 0;
    for (std::byte b : data) {
        c ^= uint32_t(b) << 8;
        for (int i = 0; i < 8; ++i) c = (c & 0x8000) ? ((c << 1) ^ 0x1021) : (c << 1);
        c &= 0xFFFF;
    }
    return uint16_t(c);
}

namespace {
VmsLayout checkVms(std::span<const std::byte> v, std::string* why);
}

bool      vms_check(std::span<const std::byte> v, std::string* why) { return checkVms(v, why) != VmsLayout::Invalid; }
VmsLayout vms_layout(std::span<const std::byte> v) { return checkVms(v, nullptr); }

namespace {
VmsLayout checkVms(std::span<const std::byte> v, std::string* why) {
    auto fail = [why](const char* m) { if (why) *why = m; return VmsLayout::Invalid; };
    if (v.empty() || v.size() % kBlock || v.size() > size_t(kCardBlocks) * kBlock) return fail("tamanho fora do multiplo de 512");
    const uint32_t icons = rd16(v, 0x40), eyecatch = rd16(v, 0x44), crc = rd16(v, 0x46), data = rd32(v, 0x48);
    static constexpr uint32_t kEyecatch[4] = {0, 72 * 56 * 2, 512 + 72 * 56, 32 + 72 * 56 / 2};
    if (icons < 1 || icons > 3) return fail("numero de icones fora de 1..3");
    if (eyecatch > 3) return fail("tipo de eyecatch fora de 0..3");
    const uint64_t fixed = 0x80ull + 0x200ull * icons + kEyecatch[eyecatch];
    // HH11 (oraculo do Flycast, H_HUNTER.SYS do jogo original): no Head Hunter o campo 0x48 e o tamanho total
    // coberto pelo CRC (cabecalho + icones + eyecatch + dados: 2748 = 128 + 512 + 2048 + 60) e o resto da
    // imagem guarda o que estava no buffer do jogo. O CRC desse trecho decide entre esse leiaute e o padrao.
    if (data > fixed && data <= v.size()) {
        std::vector<std::byte> z(v.begin(), v.begin() + ptrdiff_t(data));
        z[0x46] = z[0x47] = std::byte{0};
        if (vms_crc16(z) == crc) {
            if ((data + kBlock - 1) / kBlock * kBlock != v.size()) return fail("blocos a mais na imagem");
            return VmsLayout::HeadHunter;
        }
    }
    const uint64_t used = fixed + data;
    if (used > v.size()) return fail("dados alem do fim da imagem");
    if ((used + kBlock - 1) / kBlock * kBlock != v.size()) return fail("blocos a mais na imagem");
    for (size_t i = size_t(used); i < v.size(); ++i)
        if (v[i] != std::byte{0}) return fail("preenchimento nao zerado");
    std::vector<std::byte> z(v.begin(), v.begin() + ptrdiff_t(used));
    z[0x46] = z[0x47] = std::byte{0};
    if (vms_crc16(z) != crc) return fail("CRC do VMS nao confere");
    return VmsLayout::Standard;
}
} // namespace

bool valid_save_name(std::string_view n) {
    if (n.empty() || n.size() > 12 || n.find("..") != std::string_view::npos || n.front() == '.') return false;
    for (char c : n)
        if (!((c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '_' || c == '.')) return false;
    return true;
}

uint32_t crc32(std::span<const std::byte> data) {
    uint32_t c = 0xFFFFFFFFu;
    for (std::byte b : data) {
        c ^= uint32_t(b);
        for (int i = 0; i < 8; ++i) c = (c >> 1) ^ (0xEDB88320u & (0u - (c & 1u)));
    }
    return ~c;
}

std::vector<std::byte> envelope_encode(std::span<const std::byte> image, int64_t modified_unix) {
    std::vector<std::byte> v;
    v.reserve(kEnvHeader + image.size());
    for (uint8_t m : kMagic) v.push_back(std::byte(m));
    wr(v, kEnvelopeVersion, 2);
    wr(v, kEnvHeader, 2);
    wr(v, image.size(), 4);
    wr(v, crc32(image), 4);
    wr(v, uint64_t(modified_unix), 8);
    wr(v, 0, 8);
    v.insert(v.end(), image.begin(), image.end());
    return v;
}

SaveError envelope_decode(std::span<const std::byte> f, std::vector<std::byte>& image, int64_t* modified_unix) {
    // a versao vem antes de exigir o cabecalho desta versao: um formato futuro pode ter outro tamanho
    if (f.size() < 6 || std::memcmp(f.data(), kMagic, 4) != 0) return SaveError::Corrupt;
    if (rd16(f, 4) > kEnvelopeVersion) return SaveError::FutureVersion;
    if (f.size() < kEnvHeader) return SaveError::Corrupt;
    const uint32_t hdr = rd16(f, 6), size = rd32(f, 8);
    if (rd16(f, 4) == 0 || hdr < kEnvHeader || size == 0 || size % kBlock || size > kCardBlocks * kBlock ||
        uint64_t(hdr) + size != f.size())
        return SaveError::Corrupt;
    const auto img = f.subspan(hdr, size);
    if (crc32(img) != rd32(f, 12)) return SaveError::Corrupt;
    image.assign(img.begin(), img.end());
    if (modified_unix) *modified_unix = int64_t(uint64_t(rd32(f, 16)) | uint64_t(rd32(f, 20)) << 32);
    return SaveError::None;
}

} // namespace hh::native::saves
