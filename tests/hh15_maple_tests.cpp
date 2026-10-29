// SPDX-License-Identifier: MIT
// HH15.4 (revisao Codex): o barramento sem dispositivos (runtime/system/maple_bus) tem o efeito do modelo do Maple
// (runtime/maple/maple.cpp) com o controle presente e desconectado: os mesmos buffers de recepcao, o mesmo prazo do fim
// do DMA, o MDST antes e depois do prazo, o segundo disparo depois do fim e os mesmos contadores. Tambem: trocar a
// presenca do controle so vale fora da sessao.
#include "test.h"

#include "cpu/sh4_context.h"
#include "input/input.h"
#include "maple/maple.h"

#include <cstring>
#include <memory>
#include <vector>

namespace {

struct Disconnected final : hh::InputSource {
    hh::PadState read(int, uint64_t) override {
        hh::PadState s;
        s.connected = false;
        return s;
    }
};

constexpr uint32_t kTable = 0x0C100000, kRecv = 0x0C110000;
constexpr uint32_t kMdstar = 0xA05F6C04, kMden = 0xA05F6C14, kMdst = 0xA05F6C18;

struct Result {
    std::vector<uint8_t> recv;
    uint64_t             due1 = 0, due2 = 0;
    uint32_t             mdstBefore = 0, mdstAt = 0;
    hh::MapleCounters    counters;
};

void put32(std::vector<uint8_t>& ram, uint32_t phys, uint32_t v) { std::memcpy(&ram[phys & 0xFFFFFF], &v, 4); }

Result run(bool present) {
    std::vector<uint8_t> ram(16u << 20);
    hh::Sh4Context       c{};
    c.ram = ram.data();
    const uint8_t cmds[4] = {0x01, 0x09, 0x03, 0x09};  // DEVICE_INFO, GET_CONDITION, RESET, GET_CONDITION
    uint32_t      a       = kTable;
    for (uint32_t p = 0; p < 4; ++p) {
        put32(ram, a, (p == 3 ? 0x80000000u : 0) | (p << 16) | 1);
        put32(ram, a + 4, kRecv + p * 0x100);
        put32(ram, a + 8, cmds[p] | (((p << 6) | 0x20) << 8) | ((p << 6) << 16) | (1u << 24));
        put32(ram, a + 12, 0x01000000);
        std::memset(&ram[(kRecv + p * 0x100) & 0xFFFFFF], 0xAB, 16);
        a += 16;
    }
    hh::maple_reset();
    hh::maple_set_controller_present(present);
    if (present) hh::input_set_source(std::make_unique<Disconnected>());
    Result r;
    uint32_t v = 0;
    c.cycles   = 1000;
    hh::maple_write(&c, kMden, 1);
    hh::maple_write(&c, kMdstar, kTable);
    hh::maple_write(&c, kMdst, 1);
    r.due1   = hh::maple_dma_due();
    c.cycles = r.due1 - 1;
    hh::maple_read(&c, kMdst, v), r.mdstBefore = v;
    c.cycles = r.due1;
    hh::maple_read(&c, kMdst, v), r.mdstAt = v;
    hh::maple_write(&c, kMdst, 1);  // fim anterior visto no disparo seguinte: ISTNRM bit 12 e DMA novo
    r.due2 = hh::maple_dma_due();
    for (uint32_t p = 0; p < 4; ++p) {
        const uint8_t* b = &ram[(kRecv + p * 0x100) & 0xFFFFFF];
        r.recv.insert(r.recv.end(), b, b + 16);
    }
    r.counters = hh::maple_counters();
    if (present) hh::input_set_source(nullptr);
    hh::maple_reset();
    hh::maple_set_controller_present(true);
    return r;
}

} // namespace

HH_TEST(hh15_barramento_igual_ao_modelo_sem_dispositivo) {
    const Result model = run(true), bus = run(false);
    HH_CHECK(bus.recv == model.recv);
    HH_CHECK(bus.due1 == model.due1 && bus.due1 > 1000);
    HH_CHECK(bus.due2 == model.due2 && bus.due2 > bus.due1);
    HH_CHECK(bus.mdstBefore == 1 && model.mdstBefore == 1);
    HH_CHECK(bus.mdstAt == 0 && model.mdstAt == 0);
    HH_CHECK(bus.counters.dmas == 2 && model.counters.dmas == 2);
    for (int p = 0; p < 4; ++p) {
        HH_CHECK(bus.counters.devInfo[p] == model.counters.devInfo[p]);
        HH_CHECK(bus.counters.getCond[p] == model.counters.getCond[p]);
        HH_CHECK(bus.counters.other[p] == model.counters.other[p]);
    }
    for (size_t i = 0; i < bus.recv.size(); i += 16) {  // nenhum dispositivo: so a primeira palavra vira 0xFFFFFFFF
        uint32_t w;
        std::memcpy(&w, &bus.recv[i], 4);
        HH_CHECK(w == 0xFFFFFFFFu && bus.recv[i + 4] == 0xAB);
    }
}
