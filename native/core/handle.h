// SPDX-License-Identifier: MIT
// Derivado de recomp-BStinger native/core/handle.h (MIT, Copyright (c) 2026 Blue Stinger
// Recompiled contributors; ver third_party/licenses/recomp-BStinger-LICENSE.txt).
#pragma once
// Handle opaco e tipado das interfaces nativas. O valor e de 32 bits para caber nos
// registradores/estruturas do guest durante a fase hibrida (transitional/handle_table.h
// gera os valores: indice + geracao), mas o tipo impede misturar handles de recursos
// diferentes. 0 e sempre invalido.
#include <compare>
#include <cstdint>

namespace hh::native {

template <class Tag>
struct Handle {
    uint32_t value = 0;
    constexpr bool operator==(const Handle&) const = default;
    constexpr explicit operator bool() const { return value != 0; }
};

} // namespace hh::native
