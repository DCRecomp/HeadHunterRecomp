// SPDX-License-Identifier: MIT
// Teste de vida do build (HH00, corrigido): apenas prova que a toolchain
// compila, liga e executa em C++20.
#include <cstdint>
#include <string_view>

int main() {
    static_assert(__cplusplus >= 202002L, "C++20 requerido");
    std::string_view nome = "hh-smoke";
    std::uint32_t soma = 0;
    for (char c : nome) soma += static_cast<std::uint32_t>(c);
    if (nome.empty() || soma == 0) {
        return 1;
    }
    return 0;
}
