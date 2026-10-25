// SPDX-License-Identifier: MIT
// HH04.8: o C++ gerado do 1ST_READ.BIN completo compila e linka contra o nucleo da CPU.
// Registra a tabela de funcoes e confere que o despacho encontra o entry point e uma
// amostra de funcoes, sem executar nada (o hardware so existe a partir do HH05).
#include "funcs.h"

#include "cpu/sh4_runtime.h"

#include <cstdio>

int main() {
    hhg::register_functions();
    int fails = 0;
    for (uint32_t addr : {0x8C010000u, 0xAC010000u, 0x0C010000u}) {  // P1, P2 e P0: o despacho usa o endereco fisico
        if (!hh::sh4_lookup(addr)) {
            std::printf("FALHA: despacho sem funcao em %08X\n", addr);
            ++fails;
        }
    }
    // Vetores copiados para o VBR (config [[relocated]]): VBR+0 (alvo do jmp @VBR em
    // 8C180D98), +0x100 (excecao), +0x400 (TLB miss) e +0x600 (interrupcao).
    for (uint32_t addr : {0x8C00F400u, 0x8C00F500u, 0x8C00F800u, 0x8C00FA00u}) {
        if (!hh::sh4_lookup(addr)) {
            std::printf("FALHA: vetor relocado %08X fora do despacho\n", addr);
            ++fails;
        }
    }
    if (hh::sh4_lookup(0x8C010002u)) {  // meio de instrucao: nunca e funcao
        std::printf("FALHA: despacho aceitou 8C010002\n");
        ++fails;
    }
    std::printf("[%s] codigo gerado linkado; entry 8C010000 registrado\n", fails ? "FAIL" : " ok ");
    return fails ? 1 : 0;
}
