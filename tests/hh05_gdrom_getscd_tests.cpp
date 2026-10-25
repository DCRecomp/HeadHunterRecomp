// SPDX-License-Identifier: MIT
// HH05 (revisao Codex): GETSCD formato 1 reproduz o contrato observado no oraculo Flycast no
// 1o pedido do Head Hunter: depois da leitura FAD 0x3BBA9 x 0x32 na faixa 2 (inicio FAD
// 230845, dados), os 14 bytes sao 00 15 00 0E 41 02 01 00 36 1E 00 03 BB DB. Formato,
// tamanho ou posicao nao observados sao fatais.
#include "test.h"

#include "cpu/sh4_mem.h"
#include "cpu/sh4_runtime.h"
#include "gdrom/gdrom.h"

#include <cstring>
#include <memory>
#include <stdexcept>
#include <vector>

namespace {

struct FakeDisc : hh::DiscSource {
    std::vector<hh::DiscTrack> t{{1, 1, 150, 219295, 4}, {2, 2, 230845, 128784, 4}};
    const std::vector<hh::DiscTrack>& tracks() const override { return t; }
    bool read(uint32_t, uint8_t* out) override { std::memset(out, 0, 2048); return true; }
};

struct Fatal : std::runtime_error { using std::runtime_error::runtime_error; };

constexpr uint32_t kParams = 0x8C100000, kStat = 0x8C100100, kBuf = 0x8C200000, kDst = 0x8C300000;

// REQ_CMD + EXEC_SERVER + GET_CMD_STAT; devolve o estado final.
uint32_t command(hh::Sh4Context& c, uint32_t cmd, std::initializer_list<uint32_t> p) {
    uint32_t i = 0;
    for (uint32_t v : p) hh::sh4_write32(&c, kParams + 4 * i++, v);
    hh::gdrom_syscall(&c, 0, cmd, kParams);
    const uint32_t id = c.r[0];
    hh::gdrom_syscall(&c, 2, 0, 0);
    hh::gdrom_syscall(&c, 1, id, kStat);
    return c.r[0];
}

} // namespace

HH_TEST(hh05_getscd_matches_oracle_contract) {
    std::vector<uint8_t> ram(16u << 20);
    hh::Sh4Context       c{};
    c.ram = ram.data();
    hh::sh4_set_fatal_handler([](hh::Sh4Context*, const char* w, uint32_t, uint32_t) { throw Fatal(w); });
    hh::gdrom_set_timing(false);
    hh::gdrom_reset();
    hh::gdrom_set_disc(std::make_unique<FakeDisc>());

    // antes de qualquer leitura: posicao nao observada -> fatal
    bool fatal = false;
    try { command(c, 0x22, {1, 14, kBuf, 0}); } catch (const Fatal&) { fatal = true; }
    HH_CHECK(fatal);
    hh::gdrom_reset();
    hh::gdrom_set_disc(std::make_unique<FakeDisc>());

    HH_CHECK(command(c, 0x11, {0x3BBA9, 0x32, kDst, 0}) == 2);  // DMA read concluida
    hh::sh4_write32(&c, kBuf + 12, 0xAAAAAAAA);                 // bytes 14-15 nao podem ser tocados
    HH_CHECK(command(c, 0x22, {1, 14, kBuf, 0}) == 2);
    const uint8_t want[16] = {0x00, 0x15, 0x00, 0x0E, 0x41, 0x02, 0x01, 0x00, 0x36, 0x1E, 0x00, 0x03, 0xBB, 0xDB, 0xAA, 0xAA};
    bool same = true;
    for (int i = 0; i < 16; ++i) same &= hh::sh4_read8(&c, kBuf + i) == want[i];
    HH_CHECK(same);
    HH_CHECK(hh::sh4_read32(&c, kStat + 8) == 14);  // result[2] = bytes devolvidos

    fatal = false;
    try { command(c, 0x22, {0, 100, kBuf, 0}); } catch (const Fatal&) { fatal = true; }
    HH_CHECK(fatal);  // formato 0 nao observado

    hh::gdrom_set_disc(nullptr);
    hh::sh4_set_fatal_handler(nullptr);
}
