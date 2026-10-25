// SPDX-License-Identifier: MIT
#include "hh_extract/known_versions.hpp"

#include "hh_extract/hash.hpp"
#include "hh_extract/scramble.hpp"

#include <cstring>

namespace hh {
namespace {

std::string sha256Hex(const std::vector<uint8_t>& v) {
    Sha256 h;
    h.update(v.data(), v.size());
    return h.hexdigest();
}

} // namespace

BootCheck checkBoot(std::vector<uint8_t> raw) {
    BootCheck r;
    r.sha256 = sha256Hex(raw);
    for (const auto& v : kKnownVersions) {
        if (r.sha256 == v.boot_sha256) {
            r.version = &v;
            r.data = std::move(raw);
            return r;
        }
    }
    std::vector<uint8_t> plain = descramble(raw);
    const std::string ph = sha256Hex(plain);
    for (const auto& v : kKnownVersions) {
        if (ph == v.boot_sha256) {
            r.version = &v;
            r.scrambled = true;
            r.sha256 = ph;
            r.data = std::move(plain);
            return r;
        }
    }
    r.data = std::move(raw); // desconhecido: mantem como esta
    return r;
}

} // namespace hh
