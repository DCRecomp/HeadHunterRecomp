// SPDX-License-Identifier: MIT
// Derivado de recomp-BStinger native/render/kamui/texture_heap.h (MIT, Copyright (c) 2026 Blue Stinger
// Recompiled contributors; ver third_party/licenses/recomp-BStinger-LICENSE.txt).
#pragma once
// HH15.7c (D-K2 do BS): a memoria das texturas, do fundo e dos framebuffers que o Kamui enderecado como a VRAM (8 MB).
// E memoria do guest, nao hardware simulado: os mesmos bytes nos mesmos enderecos, com as duas vistas do jogo (o caminho
// de 32 bits, linear, que e o layout do armazenamento, e o de 64 bits, com as palavras de 32 bits intercaladas entre os
// dois bancos de 4 MB). O cache de texturas compara os bytes (render_capture), entao nao ha geracoes por pagina.
#include <cstdint>
#include <cstring>
#include <vector>

namespace hh::native::render::kamui {

class TextureHeap {
public:
    static constexpr uint32_t kSize = 8u << 20;  // 8 MB

    TextureHeap() : mem_(kSize) {}

    // offset no armazenamento (layout de 32 bits) de um offset do caminho de 64 bits
    static uint32_t offset64(uint32_t o) {
        o &= kSize - 1;
        return ((o & ~7u) >> 1) | (o & 3u) | ((o & 4u) ? kSize / 2 : 0u);
    }

    uint8_t*       data() { return mem_.data(); }
    const uint8_t* data() const { return mem_.data(); }

    // caminho de 32 bits (com a volta no fim dos 8 MB)
    uint8_t  byte32(uint32_t off) const { return mem_[off & (kSize - 1)]; }
    uint32_t word32(uint32_t off) const {  // little-endian, byte a byte com a volta
        return uint32_t(byte32(off)) | (uint32_t(byte32(off + 1)) << 8) | (uint32_t(byte32(off + 2)) << 16) |
               (uint32_t(byte32(off + 3)) << 24);
    }
    // caminho de 64 bits: n bytes a partir de `off`, byte a byte (cada um no seu banco)
    void read64(uint32_t off, uint8_t* out, uint32_t n) const {
        for (uint32_t i = 0; i < n; ++i) out[i] = mem_[offset64(off + i)];
    }

private:
    std::vector<uint8_t> mem_;
};

} // namespace hh::native::render::kamui
