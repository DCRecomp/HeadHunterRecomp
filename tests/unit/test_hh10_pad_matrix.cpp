// SPDX-License-Identifier: MIT
// HH10.1: matriz do contrato do decodificador do pd 1.50 (transitional/pad_contract) contra o codigo
// gerado do proprio jogo (f_8C15D900): o mesmo PDS, a mesma condicao, o mesmo estado dos gatilhos e o
// mesmo modo, em RAM sintetica. Cobre o que a rota nao exercita: todos os valores dos gatilhos com os
// dois estados de histerese e varios pares de limiares, cada botao sozinho e em pares, os eixos em todos
// os valores e as 64 combinacoes dos bits de support dos eixos, nos dois modos. So no build com o
// codigo gerado (HH_GENERATED_DIR).
#include "test.h"

#include "funcs.h"
#include "pad_contract.h"

#include "cpu/sh4_context.h"
#include "cpu/sh4_runtime.h"

#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

using namespace hh;
namespace pad = hh::transitional::pad;

namespace {

constexpr uint32_t kUnit = 0x8C311DAC;  // unidade 0 da tabela
constexpr uint32_t kRaw  = 0x8C100000;  // descricao bruta sintetica (+8 = quadro)
constexpr uint32_t kFrame = 0x8C100100; // quadro: condicao em +8
constexpr uint32_t kStack = 0x8CF00000;

struct Rig {
    std::vector<uint8_t> ram = std::vector<uint8_t>(16u << 20);
    Sh4Context           c{};
    bool                 loaded = false;
    Rig() {
        static bool once = [] {
            hhg::register_functions();
            return true;
        }();
        (void)once;
        c.ram        = ram.data();
        c.next_event = ~0ull;
        // o codigo gerado le os literais (pools) da imagem na RAM, como o original: o 1ST_READ.BIN
        // privado (HH07_BIN; padrao: a saida local do HH01) vai em 0x8C010000
        const char* env  = std::getenv("HH07_BIN");
        std::string path = env && *env ? env : "game-data/hh01/disc1_ref/1ST_READ.BIN";
        for (const std::string& p : {path, "../" + path, "../../" + path, "../../../" + path})
            if (FILE* f = std::fopen(p.c_str(), "rb")) {
                loaded = std::fread(ram.data() + 0x10000, 1, ram.size() - 0x10000, f) > 0x100000;
                std::fclose(f);
                break;
            }
    }
    uint8_t* at(uint32_t a) { return ram.data() + (a & 0xFFFFFF); }
    void     w32(uint32_t a, uint32_t v) { std::memcpy(at(a), &v, 4); }
    // roda f_8C15D900(unidade, bruta, i) e devolve o PDS e o estado dos gatilhos depois
    pad::Pds legacy(const pad::Pds& before, const uint8_t cond[8], uint32_t mode, pad::Triggers& t, int i) {
        pad::pds_store(at(kUnit), before);
        w32(pad::kModeAddr, mode);
        *at(pad::kThrHighAddr) = t.high, *at(pad::kThrLowAddr) = t.low;
        *at(pad::kLatchLAddr + uint32_t(i)) = t.latchL, *at(pad::kLatchRAddr + uint32_t(i)) = t.latchR;
        w32(kRaw + 8, kFrame);
        std::memcpy(at(kFrame + 8), cond, 8);
        c.r[4] = kUnit, c.r[5] = kRaw, c.r[6] = uint32_t(i), c.r[15] = kStack;
        c.pr = c.ret_expect = 0xFFFFFFF0u;
        c.ret_sp = c.r[15];
        sh4_lookup(0x8C15D900)(&c);
        t.latchL = *at(pad::kLatchLAddr + uint32_t(i)), t.latchR = *at(pad::kLatchRAddr + uint32_t(i));
        return pad::pds_load(at(kUnit));
    }
};

int g_cases = 0, g_diff = 0;

void compare(Rig& r, const pad::Pds& before, const uint8_t cond[8], uint32_t mode, pad::Triggers t, int i,
             uint32_t mut = pad::kMutNone) {
    pad::Triggers tl = t, tn = t;
    const pad::Pds want = r.legacy(before, cond, mode, tl, i);
    pad::Pds       got  = before;
    pad::Unit      u;
    std::memcpy(u.cond, cond, 8);
    u.support = before.support;
    // so o decodificador: o mesmo caminho do server_unit sem as bordas
    if (mut == pad::kMutNone) pad::decode_controller(got, cond, mode, tn);
    else {
        u.infoFlags = 1, u.response = 8;
        got = pad::server_unit(0, before, u, mode, tn, mut);
        got.off = want.off, got.press = want.press, got.release = want.release, got.old = want.old;
        got.id = want.id, got.name = want.name, got.info = want.info;
    }
    ++g_cases;
    if (!(want == got) || tl.latchL != tn.latchL || tl.latchR != tn.latchR) ++g_diff;
}

} // namespace

HH_TEST(hh10_pad_decode_matrix_matches_generated) {
    Rig r;
    if (!r.loaded) HH_SKIP("sem o 1ST_READ.BIN (HH07_BIN)");
    g_cases = g_diff = 0;
    const uint8_t thresholds[][2] = {{0x80, 0x40}, {0xFF, 0x00}, {0x10, 0x10}, {0xC8, 0x32}, {0x01, 0x00}};
    for (uint32_t mode : {0u, 1u})
        for (const auto& th : thresholds)
            for (int latch = 0; latch < 4; ++latch)
                for (int v = 0; v < 256; ++v)
                    for (int which = 0; which < 2; ++which) {  // gatilho R (byte 2) ou L (byte 3)
                        pad::Pds before;
                        before.support = 0x000F06FE;
                        uint8_t cond[8] = {0xFF, 0xFF, 0, 0, 0x80, 0x80, 0x80, 0x80};
                        cond[2 + which] = uint8_t(v);
                        pad::Triggers t{th[0], th[1], uint8_t(latch & 1), uint8_t(latch >> 1)};
                        compare(r, before, cond, mode, t, (v + latch) % 24);
                    }
    // botoes: cada bit e cada par (ativos em 0)
    for (uint32_t mode : {0u, 1u})
        for (int a = 0; a < 16; ++a)
            for (int b = a; b < 16; ++b) {
                pad::Pds       before;
                before.support = 0x000F06FE;
                const uint16_t w = uint16_t(~((1u << a) | (1u << b)));
                uint8_t cond[8]  = {uint8_t(w), uint8_t(w >> 8), 0, 0, 0x80, 0x80, 0x80, 0x80};
                compare(r, before, cond, mode, pad::Triggers{0x80, 0x40, 0, 0}, 0);
            }
    // eixos: todos os valores com as 64 combinacoes dos bits 16..21 do support, nos dois modos
    // (revisao Codex HH10: antes v += 3 e so o modo 0)
    for (uint32_t mode : {0u, 1u})
    for (uint32_t s = 0; s < 64; ++s)
        for (int v = 0; v < 256; ++v) {
            pad::Pds before;
            before.support = s << 16;
            uint8_t cond[8] = {0xFF, 0xFF, uint8_t(v), uint8_t(255 - v), uint8_t(v), uint8_t(v ^ 0x55), uint8_t(255 - v),
                               uint8_t(v + 7)};
            compare(r, before, cond, mode, pad::Triggers{0x80, 0x40, 0, 0}, 0);
        }
    std::printf("  matriz: %d casos, %d diferencas\n", g_cases, g_diff);
    HH_CHECK(g_cases > 50000 && g_diff == 0);
}

// Negativos: um contrato errado de proposito (limiar - 1, A/B trocados) tem de divergir do gerado.
HH_TEST(hh10_pad_decode_matrix_detects_mutations) {
    Rig r;
    if (!r.loaded) HH_SKIP("sem o 1ST_READ.BIN (HH07_BIN)");
    for (uint32_t mut : {uint32_t(pad::kMutThresholdOff1), uint32_t(pad::kMutSwapAB)}) {
        g_cases = g_diff = 0;
        for (int v = 0; v < 256; ++v) {
            pad::Pds before;
            before.support = 0x000F06FE;
            const uint16_t w = uint16_t(~(v & 0x06));
            uint8_t cond[8]  = {uint8_t(w), uint8_t(w >> 8), uint8_t(v), uint8_t(v), 0x80, 0x80, 0x80, 0x80};
            compare(r, before, cond, 0, pad::Triggers{0x80, 0x40, 0, 0}, 0, mut);
        }
        HH_CHECK(g_diff > 0);
    }
}
