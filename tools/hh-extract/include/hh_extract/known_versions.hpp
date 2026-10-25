// SPDX-License-Identifier: MIT
// Matriz de versoes aceitas (fatos: hash, tamanho) e verificacao de boot.
#pragma once
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace hh {

struct KnownVersion {
    const char* id;
    const char* product;
    const char* version;
    uint32_t boot_size;
    const char* boot_sha256; // 1ST_READ.BIN desembaralhado
};

inline constexpr KnownVersion kKnownVersions[] = {
    {"hedhunter_v1006_a", "MK-5104150", "V1.006", 1545024,
     "aa5853253b49bc1b9a594ffb3e6ba0f49df687d3b2e00d6a1b14cfd4d4b640d7"},
    {"hedhunter_v1006_b", "MK-5104150", "V1.006", 1545024,
     "2d1ead048b28ee28ec3db2808fdd6fd04d6e9f50fd16d9f1203730b8dd578b0a"},
};

struct BootCheck {
    std::string sha256;        // hash final (do dado mantido)
    bool scrambled = false;    // foi preciso desembaralhar
    const KnownVersion* version = nullptr;
    std::vector<uint8_t> data; // conteudo a gravar (descrambled se aplicavel)
};

// Confere o hash cru; se unknown, tenta descramble. Mantem o dado como
// esta quando nenhuma versao confere.
BootCheck checkBoot(std::vector<uint8_t> raw);

} // namespace hh
