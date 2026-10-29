// SPDX-License-Identifier: MIT
// Derivado de recomp-BStinger native/render/kamui/frame_state.cpp (MIT, Copyright (c) 2026 Blue Stinger
// Recompiled contributors; ver third_party/licenses/recomp-BStinger-LICENSE.txt).
// HH15.7c: o estado do quadro (frame_state.h). As contas sao as do caminho do TA: o banco do funil do Kamui
// (kamui_device), a paleta do PVR, o fundo e o aviso do Region Array do pvr_render, os parametros do quadro da captura e
// a gravacao do framebuffer do pvr_render.
#include "render/kamui/frame_state.h"

#include <algorithm>
#include <cstring>

namespace hh::native::render::kamui {
namespace {

float f32(uint32_t w) {
    float f;
    std::memcpy(&f, &w, 4);
    return f;
}

// Um pixel do framebuffer no formato de FB_W_CTRL (bits 2-0 formato, 15-8 K: bit de alfa do 0555 e byte alto do 0888,
// 23-16 limiar de alfa do 1555), e os bytes dele (2, 3 ou 4).
struct FbFormat {
    uint32_t fmt, kval, thr, bpp;
    explicit FbFormat(uint32_t ctrl) : fmt(ctrl & 7), kval((ctrl >> 8) & 0xFF), thr((ctrl >> 16) & 0xFF) {
        static const uint32_t bytes[8] = {2, 2, 2, 2, 3, 4, 4, 4};
        bpp = bytes[fmt];
    }
    uint32_t encode(uint32_t c) const {
        const uint32_t a = c >> 24, rr = (c >> 16) & 0xFF, g = (c >> 8) & 0xFF, b = c & 0xFF;
        switch (fmt) {
        case 0: return ((kval >> 7) << 15) | ((rr >> 3) << 10) | ((g >> 3) << 5) | (b >> 3);  // 0555
        case 1: return ((rr >> 3) << 11) | ((g >> 2) << 5) | (b >> 3);                       // 565
        case 2: return ((a >> 4) << 12) | ((rr >> 4) << 8) | ((g >> 4) << 4) | (b >> 4);     // 4444
        case 3: return ((a >= thr) << 15) | ((rr >> 3) << 10) | ((g >> 3) << 5) | (b >> 3);   // 1555
        case 4: return c & 0xFFFFFF;                                                          // 888
        case 5: return (kval << 24) | (c & 0xFFFFFF);                                         // 0888
        default: return c;                                                                    // 8888
        }
    }
};

// Os pixels do recorte que a gravacao alcanca: fn(endereco no caminho de 32 bits, valor, bytes)
template <class Fn> void forEachFramebufferPixel(const FrameState& fs, std::span<const uint32_t> image, uint32_t width,
                                                 uint32_t height, Fn fn) {
    const FbFormat f(fs.reg(0x048));                       // FB_W_CTRL
    const uint32_t sof    = fs.reg(0x060) & 0x00FFFFFC;    // FB_W_SOF1
    const uint32_t stride = (fs.reg(0x04C) & 0x1FF) * 8;   // FB_W_LINESTRIDE
    const uint32_t xc = fs.reg(0x068), yc = fs.reg(0x06C);  // FB_X_CLIP, FB_Y_CLIP
    const int      xmin = int(xc & 0x7FF), xmax = int((xc >> 16) & 0x7FF);
    const int      ymin = int(yc & 0x3FF), ymax = int((yc >> 16) & 0x3FF);
    const int      w = int(width), h = int(height);
    for (int y = std::max(0, ymin); y <= std::min(h - 1, ymax); ++y)
        for (int x = std::max(0, xmin); x <= std::min(w - 1, xmax); ++x)
            fn((sof + uint32_t(y) * stride + uint32_t(x) * f.bpp) & (TextureHeap::kSize - 1),
               f.encode(image[size_t(y) * width + x]), f.bpp);
}

} // namespace

void FrameState::load(const uint32_t* words) { std::memcpy(regs_.data(), words, sizeof regs_); }

FrameState::Effect FrameState::write(uint32_t off, uint32_t v) {
    off &= kSize - 1;
    if (off == kStartRender) return Effect::StartRender;
    if (off == kTaListInit) {
        if (!(v & 0x80000000u)) return Effect::None;
        set(kTaNextOpb, reg(kTaNextOpbInit));
        set(kTaItpCurrent, reg(kTaIspBase));
        return Effect::ListInit;
    }
    if (off == kSoftReset || off == kSpgStatus || off == kId || off == kRevision || off == kTaYuvTexCnt)
        return Effect::None;  // comando e registradores so de leitura: o valor nao fica
    set(off, v);
    return off == kTaListCont ? Effect::ListContinue : Effect::None;  // continua as listas atuais (o valor fica)
}

uint32_t FrameState::read(uint32_t off) const {
    off &= kSize - 1;
    if (off == kId) return 0x17FD11DB;
    if (off == kRevision) return 0x00000011;
    if (off == kTaYuvTexCnt) return 0;  // o conversor YUV nao e usado (o destino nativo o recusa)
    return reg(off);
}

uint32_t FrameState::palette_argb(uint32_t index) const {
    const uint32_t e   = reg(kPalette + 4 * (index & 1023));
    const uint32_t fmt = reg(kPalRamCtrl) & 3;
    if (fmt == 3) return e;
    auto x = [](uint32_t v, int bits) {
        v <<= 8 - bits;
        return v | (v >> bits);
    };
    uint32_t a, r, g, b;
    if (fmt == 0) a = (e & 0x8000) ? 0xFF : 0, r = x((e >> 10) & 31, 5), g = x((e >> 5) & 31, 5), b = x(e & 31, 5);
    else if (fmt == 1) a = 0xFF, r = x((e >> 11) & 31, 5), g = x((e >> 5) & 63, 6), b = x(e & 31, 5);
    else a = x((e >> 12) & 15, 4), r = x((e >> 8) & 15, 4), g = x((e >> 4) & 15, 4), b = x(e & 15, 4);
    return (a << 24) | (r << 16) | (g << 8) | b;
}

uint32_t background_color(const FrameState& fs, const TextureHeap& heap, bool* textured) {
    const uint32_t t    = fs.reg(0x08C);  // ISP_BACKGND_T
    const uint32_t base = ((fs.reg(0x020) & 0xF00000) + ((t >> 3) & 0x1FFFFF) * 4) & (TextureHeap::kSize - 1);  // PARAM_BASE
    const uint32_t isp  = heap.word32(base);
    const uint32_t skip = (t >> 24) & 7, vsize = (3 + skip) * 4;
    uint32_t       p    = base + 12 + (t & 7) * vsize + 12;  // pula x, y, z do 1o vertice
    if (textured) *textured = isp & (1u << 25);
    if (isp & (1u << 25)) p += (isp & (1u << 22)) ? 4 : 8;
    return heap.word32(p);
}

FrameParams frame_params(const FrameState& fs, const TextureHeap& heap, uint32_t width, uint32_t height) {
    FrameParams f;
    f.width = width, f.height = height;
    f.clear_color = 0xFF000000 | background_color(fs, heap);
    f.clear_depth = f32(fs.reg(0x088));  // ISP_BACKGND_D
    for (size_t i = 0; i < f.fog_table.size(); ++i) f.fog_table[i] = uint16_t(fs.reg(0x200 + uint32_t(i) * 4) & 0xFFFF);
    f.fog_color            = fs.reg(0x0B0);           // FOG_COL_RAM
    f.fog_vertex_color     = fs.reg(0x0B4);           // FOG_COL_VERT
    f.fog_density          = fs.reg(0x0B8) & 0xFFFF;  // FOG_DENSITY
    f.punch_through_ref    = float(fs.reg(0x11C) & 0xFF) / 255.f;
    f.translucent_autosort = !(fs.reg(0x098) & 1);  // ISP_FEED_CFG bit 0
    const uint32_t half    = fs.reg(0x080);         // HALF_OFFSET
    f.sample_center = half & 1, f.attribute_center = half & 2, f.texel_center = half & 4;
    f.small_cull = f32(fs.reg(0x078));  // FPU_CULL_VAL
    f.clamp_max = fs.reg(0x0BC), f.clamp_min = fs.reg(0x0C0);
    return f;
}

bool region_sort_differs(const FrameState& fs, const TextureHeap& heap, bool autosort) {
    if (!((fs.reg(0x07C) >> 21) & 1)) return false;  // FPU_PARAM_CFG: Region Array tipo 1
    uint32_t a = fs.reg(0x02C) & 0x7FFFFC;           // REGION_BASE; entradas de 6 palavras, a ultima com o bit 31
    for (int n = 0; n < 4096; ++n, a += 24) {
        const uint32_t ctrl    = heap.word32(a);
        const bool     presort = (ctrl >> 29) & 1;
        if (presort == autosort) return true;
        if (ctrl >> 31) return false;
    }
    return false;
}

void write_framebuffer(const FrameState& fs, TextureHeap& heap, std::span<const uint32_t> image, uint32_t width,
                       uint32_t height) {
    uint8_t* mem = heap.data();
    forEachFramebufferPixel(fs, image, width, height, [&](uint32_t addr, uint32_t v, uint32_t bpp) {
        for (uint32_t i = 0; i < bpp; ++i) mem[(addr + i) & (TextureHeap::kSize - 1)] = uint8_t(v >> (8 * i));
    });
}

uint32_t framebuffer_mismatch(const FrameState& fs, const TextureHeap& heap, std::span<const uint32_t> image,
                              uint32_t width, uint32_t height) {
    uint32_t bad = 0;
    forEachFramebufferPixel(fs, image, width, height, [&](uint32_t addr, uint32_t v, uint32_t bpp) {
        for (uint32_t i = 0; i < bpp; ++i)
            if (heap.byte32(addr + i) != uint8_t(v >> (8 * i))) {
                ++bad;
                return;
            }
    });
    return bad;
}

} // namespace hh::native::render::kamui
