// SPDX-License-Identifier: MIT
// Derivado de recomp-BStinger native/render/kamui/display_service.cpp (MIT, Copyright (c) 2026 Blue Stinger
// Recompiled contributors; ver third_party/licenses/recomp-BStinger-LICENSE.txt).
// HH15.7c: a imagem exibida (display_service.h), com as contas do pvr_video do runtime.
#include "render/kamui/display_service.h"

namespace hh::native::render::kamui {
namespace {

uint32_t expand5(uint32_t v) { return (v << 3) | (v >> 2); }
uint32_t expand6(uint32_t v) { return (v << 2) | (v >> 4); }

uint32_t pixel(const TextureHeap& heap, uint32_t addr, uint32_t fmt) {
    auto at = [&](uint32_t a) { return uint32_t(heap.byte32(a)); };
    switch (fmt) {
    case 0: {  // 0555
        const uint32_t v = at(addr) | (at(addr + 1) << 8);
        return 0xFF000000 | (expand5((v >> 10) & 31) << 16) | (expand5((v >> 5) & 31) << 8) | expand5(v & 31);
    }
    case 1: {  // 565
        const uint32_t v = at(addr) | (at(addr + 1) << 8);
        return 0xFF000000 | (expand5(v >> 11) << 16) | (expand6((v >> 5) & 63) << 8) | expand5(v & 31);
    }
    default:  // 888 empacotado e 0888: B, G, R nos bytes baixos
        return 0xFF000000 | (at(addr + 2) << 16) | (at(addr + 1) << 8) | at(addr);
    }
}

} // namespace

void scanout(const FrameState& fs, uint32_t spg_control, const TextureHeap& heap, std::vector<uint32_t>& argb,
             uint32_t& width, uint32_t& height) {
    const uint32_t ctrl = fs.reg(0x044), size = fs.reg(0x05C);
    const uint32_t fmt = (ctrl >> 2) & 3, bpp = fmt == 2 ? 3 : (fmt == 3 ? 4 : 2);
    const uint32_t words = (size & 0x3FF) + 1, lines = ((size >> 10) & 0x3FF) + 1, mod = (size >> 20) & 0x3FF;
    const uint32_t stride    = (words + mod - (mod ? 1 : 0)) * 4;
    const bool     interlace = (spg_control >> 4) & 1;
    const bool     dbl       = !interlace && ((ctrl >> 1) & 1);
    width  = words * 4 / bpp;
    height = lines * (interlace || dbl ? 2 : 1);
    argb.assign(size_t(width) * height, 0xFF000000);
    const bool on = (ctrl & 1) && !((fs.reg(0x0E8) >> 3) & 1);
    if (!on) return;  // video desligado ou em blank: preto
    const uint32_t sof1 = fs.reg(0x050) & 0x00FFFFFC, sof2 = fs.reg(0x054) & 0x00FFFFFC;
    for (uint32_t y = 0; y < height; ++y) {
        uint32_t base;
        if (interlace) base = ((y & 1) ? sof2 : sof1) + (y >> 1) * stride;
        else base = sof1 + (dbl ? y >> 1 : y) * stride;
        uint32_t* row = argb.data() + size_t(y) * width;
        for (uint32_t x = 0; x < width; ++x) row[x] = pixel(heap, base + x * bpp, fmt);
    }
}

} // namespace hh::native::render::kamui
