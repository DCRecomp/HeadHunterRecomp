// SPDX-License-Identifier: MIT
#include "hh_extract/scramble.hpp"

namespace hh {
namespace {

constexpr size_t kMaxChunk = 2048 * 1024;
constexpr size_t kSlice = 32;

template <bool kUnshuffle>
std::vector<uint8_t> transform(const std::vector<uint8_t>& in) {
    std::vector<uint8_t> out(in.size());
    uint32_t seed = static_cast<uint32_t>(in.size()) & 0xFFFF;
    auto rnd = [&] {
        seed = (seed * 2109 + 9273) & 0x7FFF;
        return (seed + 0xC000) & 0xFFFF;
    };
    std::vector<uint32_t> idx(kMaxChunk / kSlice);
    size_t pos = 0;
    size_t left = in.size();
    for (size_t chunk = kMaxChunk; chunk >= kSlice; chunk >>= 1) {
        while (left >= chunk) {
            const size_t slices = chunk / kSlice;
            for (size_t i = 0; i < slices; ++i) idx[i] = static_cast<uint32_t>(i);
            size_t seq = 0; // fatia sequencial no lado de saida
            for (size_t i = slices; i-- > 0;) {
                const uint32_t r = rnd();
                const uint32_t j = (r * static_cast<uint32_t>(i)) >> 16; // 0..i
                // troca idx[i] com idx[j]; a fatia processada agora e idx[i]
                const uint32_t tmp = idx[i];
                idx[i] = idx[j];
                idx[j] = tmp;
                const size_t src = static_cast<size_t>(idx[i]) * kSlice;
                const size_t dstPos = kUnshuffle ? pos + src : pos + seq * kSlice;
                const size_t srcPos = kUnshuffle ? pos + seq * kSlice : pos + src;
                for (size_t k = 0; k < kSlice; ++k) out[dstPos + k] = in[srcPos + k];
                ++seq;
            }
            pos += chunk;
            left -= chunk;
        }
    }
    // sobra (< 32 bytes) copiada crua
    for (size_t k = 0; k < left; ++k) out[pos + k] = in[pos + k];
    return out;
}

} // namespace

std::vector<uint8_t> descramble(const std::vector<uint8_t>& in) {
    return transform<true>(in);
}

std::vector<uint8_t> scramble(const std::vector<uint8_t>& in) {
    return transform<false>(in);
}

} // namespace hh
