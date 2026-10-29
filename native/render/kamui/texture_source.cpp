// SPDX-License-Identifier: MIT
// HH15.7c: os texels das texturas da IR (texture_source.h). Movido da captura (transitional/render_capture), que lia a
// VRAM do runtime; as contas de alcance sao as mesmas.
#include "render/kamui/texture_source.h"

#include "render/texture_convert.h"

#include <algorithm>
#include <cstring>

namespace hh::native::render::kamui {

uint32_t mip_offset(uint32_t fmt, bool vq, uint32_t w) {
    if (w < 2) return vq ? 0 : (fmt == 6 ? 3 : 6);
    if (vq) return 1 + ((w / 2) * (w / 2) - 1) / 3;
    const uint32_t t = (w * w - 1) / 3;
    return fmt == 6 ? 3 + t : 6 + 2 * t;
}

size_t linear_texture_texels(uint32_t width, uint32_t height, uint32_t stride) {
    if (!width || !height) return 0;
    const size_t row = stride ? stride : width;
    return (size_t(height) - 1) * row + std::max<size_t>(row, width);
}

bool convert_texture(const FrameState& fs, const TextureHeap& heap, const TextureKey& k, std::vector<uint32_t>& argb) {
    const uint32_t fmt = (k.tcw >> 27) & 7, base = (k.tcw & 0x1FFFFF) << 3;
    if (!k.width || !k.height) return false;
    const bool     mip = k.tcw >> 31, vq = (k.tcw >> 30) & 1, linear = (k.tcw >> 26) & 1, strideSel = (k.tcw >> 25) & 1;
    const uint32_t w = k.width, h = k.height;
    auto           read = [&](uint32_t n) {
        std::vector<uint8_t> b(n);
        heap.read64(base, b.data(), n);
        return b;
    };
    if (fmt <= 2) {
        const auto f = Texel16(fmt);
        if (vq) {
            const auto d = read(2048 + (mip ? mip_offset(fmt, true, w) : 0) + (w / 2) * (h / 2));
            return convert_vq(d, w, h, f, mip, 256, argb);
        }
        if (mip) {
            const auto            d = read(mip_offset(fmt, false, w) + 2 * w * w);
            std::vector<MipLevel> levels;
            if (!convert16_mips(d, w, f, levels) || levels.empty()) return false;
            argb = std::move(levels[0].argb);
            return true;
        }
        const uint32_t rowW = linear && strideSel && k.stride ? k.stride : w;  // linear com stride select
        // a ultima linha vai ate x = w - 1 mesmo com stride menor que a largura (como o legado, que le a VRAM direto)
        const auto            d = read(uint32_t(2 * (linear ? linear_texture_texels(w, h, rowW) : size_t(w) * h)));
        std::vector<uint16_t> px(size_t(w) * h);
        for (uint32_t y = 0; y < h; ++y)
            for (uint32_t x = 0; x < w; ++x) {
                const size_t from = linear ? size_t(y) * rowW + x : size_t(y) * w + x;  // twiddled: o vetor inteiro
                std::memcpy(&px[size_t(y) * w + x], d.data() + 2 * from, 2);
            }
        return convert16(px, w, h, f, !linear, argb);
    }
    if (fmt == 5 && !mip) {  // paleta de 4 bits: o banco do seletor (TCW bits 26-21)
        std::vector<uint32_t> pal(16);
        const uint32_t        sel = (k.tcw >> 21) & 0x3F;
        for (uint32_t i = 0; i < 16; ++i) pal[i] = fs.palette_argb(sel * 16 + i);
        return convert_pal4(read(w * h / 2), w, h, pal, argb);
    }
    if (fmt == 6) {  // paleta de 8 bits (com ou sem mipmap: o maior nivel): o banco de 256 do seletor (TCW bits 26-25)
        std::vector<uint32_t> pal(256);
        const uint32_t        bank = (k.tcw >> 25) & 3;
        for (uint32_t i = 0; i < 256; ++i) pal[i] = fs.palette_argb(bank * 256 + i);
        const uint32_t        skip = mip ? mip_offset(6, false, w) : 0;
        std::vector<uint8_t>  d(skip + w * h);
        heap.read64(base, d.data(), uint32_t(d.size()));
        return convert_pal8(std::span<const uint8_t>(d).subspan(skip), w, h, pal, argb);
    }
    return false;  // YUV, bump, paleta de 4 bits com mipmap: sem conversao nativa
}

void texture_bytes(const FrameState& fs, const TextureHeap& heap, const TextureKey& k, std::vector<uint8_t>& raw) {
    const uint32_t fmt = (k.tcw >> 27) & 7, base = (k.tcw & 0x1FFFFF) << 3;
    const bool     mip = k.tcw >> 31, vq = (k.tcw >> 30) & 1, linear = (k.tcw >> 26) & 1, strideSel = (k.tcw >> 25) & 1;
    const uint32_t w = k.width, h = k.height;
    const uint32_t bits = fmt == 5 ? 4 : (fmt == 6 ? 8 : 16);  // paleta de 4 e de 8 bits; o resto, 16
    uint32_t       n;
    if (vq) n = 2048 + (mip ? mip_offset(fmt, true, w) : 0) + (w / 2) * (h / 2);
    // linear com stride so nos formatos de 16 bits sem mipmap (paleta: sempre twiddled, os bits 26/25 sao o seletor)
    else if (fmt <= 4 && !mip && linear && strideSel && k.stride) n = uint32_t(2 * linear_texture_texels(w, h, k.stride));
    else if (mip) n = mip_offset(fmt, false, w) + bits * w * w / 8;
    else n = bits * w * h / 8;
    raw.resize(n);
    heap.read64(base, raw.data(), n);
    if (fmt == 5 || fmt == 6) {  // o banco de paleta que o seletor escolhe
        const uint32_t first = fmt == 5 ? ((k.tcw >> 21) & 0x3F) * 16 : ((k.tcw >> 25) & 3) * 256, count = fmt == 5 ? 16 : 256;
        for (uint32_t i = 0; i < count; ++i) {
            const uint32_t e = fs.palette_argb(first + i);
            raw.insert(raw.end(), reinterpret_cast<const uint8_t*>(&e), reinterpret_cast<const uint8_t*>(&e) + 4);
        }
    }
}

} // namespace hh::native::render::kamui
