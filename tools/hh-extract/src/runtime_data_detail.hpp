// SPDX-License-Identifier: MIT
// Derivado de recomp-BStinger tools/extractor/runtime_data.cpp (MIT, Copyright (c) 2026 Blue Stinger
// Recompiled contributors; ver third_party/licenses/recomp-BStinger-LICENSE.txt).
#pragma once
// Auxiliares internos dos dados de execucao, comuns a leitura (runtime_data.cpp, entra no
// produto) e a preparacao a partir do CDI (runtime_data_cdi.cpp, so Builder/ferramentas).
// HH07.8: separados para que o executavel do jogo nao ligue nada da leitura de CDI.
#include "hh_extract/ip_bin.hpp"
#include "hh_extract/runtime_data.hpp"

#include <atomic>
#include <cstdint>
#include <cstdio>
#include <filesystem>
#include <string>

namespace hh::disc::detail {

inline constexpr const char* kManifest = "disc.manifest";
inline constexpr const char* kMetaIdx  = "disc_meta.idx";
inline constexpr const char* kMetaBin  = "disc_meta.bin";
inline constexpr uint32_t    kSector   = kUserDataSize;

std::filesystem::path u8path(const std::string& s);
std::string           lowerAscii(std::string s);
uint32_t              sectorsOf(uint32_t size);
bool                  safeIsoPath(const std::string& p);
FILE*                 openRead(const std::filesystem::path& p);
bool                  readAt(FILE* f, uint64_t off, void* dst, size_t n, size_t& got);
void                  checkCancel(const std::atomic<bool>* cancel);
std::string           pathText(const std::filesystem::path& p);
std::string           sha256Hex(const uint8_t* data, size_t n);
std::string           fileSha256(const std::filesystem::path& p);
std::string           regenerate();

} // namespace hh::disc::detail
