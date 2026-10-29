// SPDX-License-Identifier: MIT
// HH15.3: o servico nativo das syscalls (runtime/system/syscall_service) tem o mesmo efeito do BIOS HLE (r0 e RAM) no
// contrato medido, sem dados do jogo (as leituras de setor e o GETSCD sao conferidos pelas referencias da HH15, com os
// dados), e recusa com erro explicito o que esta fora do contrato.
#include "test.h"

#include "cpu/sh4_mem.h"
#include "cpu/sh4_runtime.h"
#include "gdrom/data_source.h"
#include "gdrom/gdrom.h"
#include "hle/bios.h"
#include "system/syscall_service.h"

#include <cstdlib>
#include <cstring>
#include <stdexcept>
#include <vector>

namespace {

struct Fatal : std::runtime_error {
    using std::runtime_error::runtime_error;
};

constexpr uint32_t kBuf = 0x8C200000, kSrc = 0x8C201000, kParams = 0x8C202000, kStat = 0x8C203000;

struct Machine {
    std::vector<uint8_t> ram = std::vector<uint8_t>(16u << 20);
    hh::Sh4Context       c{};
    Machine() { c.ram = ram.data(); }
    // syscall pelo vetor (como o stub do SDK: mov.l @vetor,r0; jmp @r0)
    uint32_t sys(uint32_t vec, uint32_t r4, uint32_t r5, uint32_t r6, uint32_t r7) {
        c.r[4] = r4, c.r[5] = r5, c.r[6] = r6, c.r[7] = r7, c.r[0] = 0xDEADBEEF;
        c.pr = c.ret_expect = 0x8C010000;
        hh::sh4_lookup(hh::sh4_read32(&c, vec))(&c);
        return c.r[0];
    }
    std::vector<uint8_t> bytes(uint32_t a, uint32_t n) const {
        return std::vector<uint8_t>(ram.begin() + (a & 0xFFFFFF), ram.begin() + (a & 0xFFFFFF) + n);
    }
};

// O roteiro do contrato medido (HH_HLE_LOG na rota nativa): o resultado de cada passo (r0) e as areas que ele grava.
std::vector<std::vector<uint8_t>> script(Machine& m) {
    using namespace hh;
    std::vector<std::vector<uint8_t>> out;
    auto r0 = [&](uint32_t v) { out.push_back({uint8_t(v), uint8_t(v >> 8), uint8_t(v >> 16), uint8_t(v >> 24)}); };
    r0(m.sys(kBiosVecSysinfo, 0, 0x10, 0x10, 0));  // INIT
    out.push_back(m.bytes(0x8C000068, 24));
    r0(m.sys(kBiosVecSysinfo, 0, 0x10, 0x10, 3));  // ID
    r0(m.sys(kBiosVecFlashrom, 2, kBuf, 0x8C30FA20, 0));  // INFO particao 2
    out.push_back(m.bytes(kBuf, 8));
    r0(m.sys(kBiosVecFlashrom, 0x1C000, kBuf, 0x40, 1));  // READ
    out.push_back(m.bytes(kBuf, 0x40));
    for (uint32_t i = 0; i < 0x12; ++i) sh4_write8(&m.c, kSrc + i, uint8_t(0xA5 ^ (i * 7)));
    r0(m.sys(kBiosVecFlashrom, 0x1C000, kSrc, 0x12, 2));  // WRITE (AND)
    r0(m.sys(kBiosVecFlashrom, 0x1C000, kBuf, 0x40, 1));
    out.push_back(m.bytes(kBuf, 0x40));
    r0(m.sys(kBiosVecFlashrom, 0x1FFC0, kBuf, 0x40, 1));
    out.push_back(m.bytes(kBuf, 0x40));
    r0(m.sys(kBiosVecFlashrom, 0x1C000, 0x8C1878E9, 0x0B, 3));  // DELETE
    r0(m.sys(kBiosVecFlashrom, 0x1C000, kBuf, 0x40, 1));
    out.push_back(m.bytes(kBuf, 0x40));
    r0(m.sys(kBiosVecGdrom, 0, 0, 0, 3));  // INIT_SYSTEM
    r0(m.sys(kBiosVecGdrom, kStat, 0, 0, 4));  // GET_DRV_STAT
    out.push_back(m.bytes(kStat, 8));
    for (uint32_t cmd : {0x18u, 0x28u}) {  // INIT e GET_VERSION
        sh4_write32(&m.c, kParams, kBuf);
        for (int i = 1; i < 4; ++i) sh4_write32(&m.c, kParams + 4 * i, 0);
        const uint32_t id = m.sys(kBiosVecGdrom, cmd, kParams, 0, 0);
        r0(id);
        r0(m.sys(kBiosVecGdrom, cmd, kParams, 0, 0));  // ocupado: 0
        r0(m.sys(kBiosVecGdrom, id, kStat, 0, 1));     // 1 ocupado
        m.sys(kBiosVecGdrom, 0, 0, 0, 2);              // EXEC_SERVER
        r0(m.sys(kBiosVecGdrom, id + 1, kStat, 0, 1)); // outro id: 0
        r0(m.sys(kBiosVecGdrom, id, kStat, 0, 1));     // 2 concluido (uma vez)
        out.push_back(m.bytes(kStat, 16));
        r0(m.sys(kBiosVecGdrom, id, kStat, 0, 1));     // 0
    }
    out.push_back(m.bytes(kBuf, 28));
    return out;
}

} // namespace

HH_TEST(hh15_servico_nativo_igual_ao_hle_no_contrato) {
    hh::sh4_set_fatal_handler([](hh::Sh4Context*, const char* w, uint32_t, uint32_t) { throw Fatal(w); });
    Machine hle;
    hh::bios_hle_install(&hle.c, true);
    const auto ref = script(hle);
    Machine nat;
    hh::bios_hle_install(&nat.c, true);  // como o boot; o servico regrava os mesmos enderecos
    hh::syscall_service_install(&nat.c, {});
    for (uint32_t v : {hh::kBiosVecSysinfo, hh::kBiosVecRomfont, hh::kBiosVecFlashrom, hh::kBiosVecGdrom,
                       hh::kBiosVecGdrom2, hh::kBiosVecMisc})
        HH_CHECK(hh::sh4_read32(&nat.c, v) == hh::sh4_read32(&hle.c, v));
    const auto got = script(nat);
    HH_CHECK(got.size() == ref.size());
    for (size_t i = 0; i < ref.size() && i < got.size(); ++i) HH_CHECK(got[i] == ref[i]);
    HH_CHECK(nat.ram == hle.ram);  // a RAM inteira depois do roteiro (revisao Codex)
    HH_CHECK(hh::syscall_service_stats().refused == 0);
    hh::syscall_service_uninstall();
    hh::bios_hle_install(&nat.c, true);
    hh::sh4_set_fatal_handler(nullptr);
}

// Com os dados do disco (HH_DATA_DIR): as 6 leituras por DMA do boot (os FADs medidos com HH_HLE_LOG) e o GETSCD
// depois delas, no HLE (gdrom_set_disc com a mesma fonte) e no servico: r0, estado do pedido e a RAM inteira iguais.
HH_TEST(hh15_servico_nativo_le_setores_como_o_hle) {
    const char* dir = std::getenv("HH_DATA_DIR");
    if (!dir || !*dir) HH_SKIP("HH_DATA_DIR ausente");
    hh::sh4_set_fatal_handler([](hh::Sh4Context*, const char* w, uint32_t, uint32_t) { throw Fatal(w); });
    auto run = [&](Machine& m) {
        std::vector<uint32_t> r0s;
        r0s.push_back(m.sys(hh::kBiosVecGdrom, 0, 0, 0, 3));  // INIT_SYSTEM
        uint32_t dest = 0x8C296220;
        for (uint32_t fad : {0x385CDu, 0x385D0u, 0x385D4u, 0x385E0u, 0x385E1u, 0x385E2u}) {
            hh::sh4_write32(&m.c, kParams, fad);
            hh::sh4_write32(&m.c, kParams + 4, 1);
            hh::sh4_write32(&m.c, kParams + 8, dest);
            hh::sh4_write32(&m.c, kParams + 12, 0);
            const uint32_t id = m.sys(hh::kBiosVecGdrom, 0x11, kParams, 0, 0);
            m.sys(hh::kBiosVecGdrom, 0, 0, 0, 2);
            r0s.push_back(id), r0s.push_back(m.sys(hh::kBiosVecGdrom, id, kStat, 0, 1));
            dest += 0x800;
        }
        hh::sh4_write32(&m.c, kParams, 1), hh::sh4_write32(&m.c, kParams + 4, 14), hh::sh4_write32(&m.c, kParams + 8, kBuf);
        const uint32_t id = m.sys(hh::kBiosVecGdrom, 0x22, kParams, 0, 0);
        m.sys(hh::kBiosVecGdrom, 0, 0, 0, 2);
        r0s.push_back(id), r0s.push_back(m.sys(hh::kBiosVecGdrom, id, kStat, 0, 1));
        return r0s;
    };
    Machine hle;
    hh::bios_hle_install(&hle.c, true);
    hh::gdrom_set_disc(hh::open_runtime_data(dir, {}));
    const auto ref = run(hle);
    hh::gdrom_set_disc(nullptr);
    Machine nat;
    hh::bios_hle_install(&nat.c, true);
    hh::syscall_service_install(&nat.c, dir);
    const auto got = run(nat);
    HH_CHECK(got == ref);
    HH_CHECK(nat.ram == hle.ram);
    HH_CHECK(hh::syscall_service_stats().sectors == 6 && hh::syscall_service_stats().refused == 0);
    uint32_t w = 0;
    std::memcpy(&w, &nat.ram[kBuf & 0xFFFFFF], 4);
    HH_CHECK(w != 0);  // o GETSCD gravou o canal Q (nao so zeros iguais)
    hh::syscall_service_uninstall();
    hh::bios_hle_install(&nat.c, true);
    hh::sh4_set_fatal_handler(nullptr);
}

HH_TEST(hh15_servico_nativo_recusa_fora_do_contrato) {
    hh::sh4_set_fatal_handler([](hh::Sh4Context*, const char* w, uint32_t, uint32_t) { throw Fatal(w); });
    Machine m;
    hh::bios_hle_install(&m.c, true);
    hh::syscall_service_install(&m.c, {});
    int refused = 0;
    auto expectRefused = [&](uint32_t vec, uint32_t r4, uint32_t r5, uint32_t r6, uint32_t r7) {
        bool fatal = false;
        try {
            m.sys(vec, r4, r5, r6, r7);
        } catch (const Fatal&) {
            fatal = true;
        }
        HH_CHECK(fatal);
        refused += fatal;
    };
    using namespace hh;
    expectRefused(kBiosVecSysinfo, 0, 0, 0, 2);               // ICON
    expectRefused(kBiosVecRomfont, 0, 0, 0, 0);               // ROMFONT
    expectRefused(kBiosVecGdrom2, 0, 0, 0, 0);                // GDROM2
    expectRefused(kBiosVecMisc, 7, 0, 0, 0);                  // MISC com outro codigo
    expectRefused(kBiosVecFlashrom, 0, kBuf, 0, 0);           // INFO de outra particao
    expectRefused(kBiosVecFlashrom, 0x1A000, kBuf, 0x40, 1);  // READ fora da particao 2
    expectRefused(kBiosVecFlashrom, 0x1FFF0, kBuf, 0x40, 1);  // READ que passa do fim da particao 2
    expectRefused(kBiosVecFlashrom, 0x10000, kBuf, 0x40, 2);  // WRITE fora da particao 2
    expectRefused(kBiosVecFlashrom, 0x10000, 0, 0, 3);        // DELETE de outra particao
    expectRefused(kBiosVecFlashrom, 0, 0, 0, 4);              // funcao desconhecida
    expectRefused(kBiosVecGdrom, 0, 0, 1, 0);                 // superfuncao
    expectRefused(kBiosVecGdrom, 0, 0, 0, 9);                 // RESET
    for (uint32_t cmd : {0x10u, 0x13u, 0x22u, 0x11u}) {       // PIOREAD, GETTOC2, GETSCD sem leitura, DMAREAD sem dados
        m.sys(kBiosVecGdrom, 0, 0, 0, 3);
        sh4_write32(&m.c, kParams, cmd == 0x22u ? 1u : 0x3BBA9u);
        sh4_write32(&m.c, kParams + 4, cmd == 0x22u ? 14u : 1u);
        sh4_write32(&m.c, kParams + 8, kBuf);
        if (cmd == 0x10u || cmd == 0x13u) {  // fora do contrato: recusado ja no pedido (revisao Codex)
            expectRefused(kBiosVecGdrom, cmd, kParams, 0, 0);
            continue;
        }
        HH_CHECK(m.sys(kBiosVecGdrom, cmd, kParams, 0, 0) != 0);
        expectRefused(kBiosVecGdrom, 0, 0, 0, 2);  // EXEC_SERVER: GETSCD sem leitura e DMAREAD sem dados
    }
    // comando fora do contrato com o drive ocupado: recusado no pedido, nao devolvido como "ocupado" (revisao Codex)
    m.sys(kBiosVecGdrom, 0, 0, 0, 3);
    HH_CHECK(m.sys(kBiosVecGdrom, 0x18, kParams, 0, 0) != 0);
    expectRefused(kBiosVecGdrom, 0x10, kParams, 0, 0);
    HH_CHECK(refused == 17);
    HH_CHECK(hh::syscall_service_stats().refused == 16);  // o DMAREAD sem dados e instalacao incompleta, nao recusa
    hh::syscall_service_uninstall();
    hh::sh4_set_fatal_handler(nullptr);
}
