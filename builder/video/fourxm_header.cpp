// SPDX-License-Identifier: MIT
#include "video/fourxm_header.h"

#include <cstring>
#include <fstream>
#include <vector>

namespace hh::builder::video {

namespace {

bool fail(std::string* err, const std::string& m) {
    if (err) *err = m;
    return false;
}

uint32_t u32(const uint8_t* p) { return uint32_t(p[0]) | uint32_t(p[1]) << 8 | uint32_t(p[2]) << 16 | uint32_t(p[3]) << 24; }

} // namespace

bool read_4xm_header(const std::filesystem::path& file, FourXmHeader& h, std::string* err) {
    h = {};
    std::ifstream in(file, std::ios::binary);
    if (!in) return fail(err, "nao abre " + file.filename().string());
    std::vector<uint8_t> b(64 * 1024);
    in.read(reinterpret_cast<char*>(b.data()), std::streamsize(b.size()));
    b.resize(size_t(in.gcount()));
    if (b.size() < 12 || std::memcmp(b.data(), "RIFF", 4) != 0 || std::memcmp(b.data() + 8, "4XMV", 4) != 0)
        return fail(err, file.filename().string() + ": nao e um 4X Movie (RIFF 4XMV)");
    for (size_t i = 12; i + 8 <= b.size(); ++i) {
        if (std::memcmp(b.data() + i, "vtrk", 4) != 0) continue;
        const uint32_t len = u32(b.data() + i + 4);
        if (len < 44 || i + 8 + len > b.size()) return fail(err, file.filename().string() + ": chunk vtrk truncado");
        const uint8_t* v = b.data() + i + 8;
        h.frames      = u32(v + 4 * 4);
        h.codedWidth  = u32(v + 7 * 4), h.codedHeight = u32(v + 8 * 4);
        h.shownWidth  = u32(v + 9 * 4), h.shownHeight = u32(v + 10 * 4);
        if (!h.frames || !h.codedWidth || !h.codedHeight || h.shownWidth > h.codedWidth || h.shownHeight > h.codedHeight)
            return fail(err, file.filename().string() + ": cabecalho vtrk invalido");
        if (!h.shownWidth || !h.shownHeight) h.shownWidth = h.codedWidth, h.shownHeight = h.codedHeight;
        return true;
    }
    return fail(err, file.filename().string() + ": sem chunk vtrk");
}

} // namespace hh::builder::video
