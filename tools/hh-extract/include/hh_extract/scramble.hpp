// SPDX-License-Identifier: MIT
// Descramble/scramble de executaveis Dreamcast (1ST_READ.BIN).
// HH02 - reimplementado a partir da especificacao documentada do formato:
// o arquivo e dividido em blocos de 2 MB, depois 1 MB, ... ate 32 bytes.
// Dentro de cada bloco, as fatias de 32 bytes sao permutadas por um PRNG
// semeado com (tamanho & 0xFFFF). Sobra no fim (< 32 bytes) fica crua.
#pragma once
#include <cstddef>
#include <cstdint>
#include <vector>

namespace hh {

// Reverte o embaralhamento (scramble) usado nos executaveis Katana.
std::vector<uint8_t> descramble(const std::vector<uint8_t>& in);

// Aplica o embaralhamento (inverso de descramble; usado em testes).
std::vector<uint8_t> scramble(const std::vector<uint8_t>& in);

} // namespace hh
