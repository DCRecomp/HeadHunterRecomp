// SPDX-License-Identifier: MIT
// HH15.7d1 (revisao Codex): o contrato do DMA do canal 2 e do system block com valores esperados fixos (os do modelo de
// antes), pela API que a memoria e o boot chamam (pvr/holly_sb.h, memory/sh4_dmac.h). Compilado duas vezes: no
// hh_runtime_tests (os modelos da verificacao, pvr/holly_sb e memory/sh4_dmac, que delegam aos servicos) e no
// hh15_produto_sb_tests com os stubs do produto (system/sb_ausente, system/dmac_ausente, HH_PRODUTO_STUBS): os dois tem
// de dar o mesmo efeito no contrato; fora dele o produto para com erro explicito e o modelo guarda o valor.
#include "test.h"

#include "cpu/sh4_mem.h"
#include "cpu/sh4_runtime.h"
#include "memory/sh4_dmac.h"
#include "pvr/holly_intc.h"
#include "pvr/holly_sb.h"
#include "system/dma_bus.h"

#include <array>
#include <stdexcept>
#include <utility>
#include <vector>

namespace {

struct Fatal : std::runtime_error {
    using std::runtime_error::runtime_error;
};

std::vector<std::pair<uint32_t, std::array<uint32_t, 8>>> g_got;

bool sink(hh::Sh4Context*, uint32_t target, const uint32_t* blk) {
    std::array<uint32_t, 8> b{};
    for (int i = 0; i < 8; ++i) b[size_t(i)] = blk[i];
    g_got.push_back({target, b});
    return true;
}

constexpr uint32_t kSar2 = 0xFFA00020, kDmatcr2 = 0xFFA00028, kChcr2 = 0xFFA0002C, kDmaor = 0xFFA00040;
constexpr uint32_t kC2Dstat = 0xA05F6800, kC2Dlen = 0xA05F6804, kC2Dst = 0xA05F6808, kFfst = 0xA05F688C;
constexpr uint32_t kSbRev = 0xA05F689C, kIstNrm = 0xA05F6900;

} // namespace

HH_TEST(hh15_dma_e_system_block_no_contrato) {
    std::vector<uint8_t> ram(16u << 20);
    hh::Sh4Context       c{};
    c.ram = ram.data();
    hh::holly_intc_reset(0);
    hh::holly_sb_reset();
    hh::sh4_dmac_reset();
    hh::dma_bus_set_sink(&sink);
    g_got.clear();
    for (uint32_t i = 0; i < 64; ++i) hh::sh4_write32(&c, 0x8C010000 + 4 * i, 0x1000 + i);
    auto dmac = [&](uint32_t a) {
        uint32_t v = 0xDEADBEEF;
        HH_CHECK(hh::sh4_dmac_read(&c, a, 4, v));
        return v;
    };
    auto sb = [&](uint32_t a) {
        uint32_t v = 0xDEADBEEF;
        HH_CHECK(hh::holly_sb_read(&c, a, 4, v));
        return v;
    };
    auto ist = [&] {
        uint32_t v = 0;
        HH_CHECK(hh::holly_intc_read(&c, kIstNrm, 4, v));
        return v;
    };
    // canal desabilitado (DMAOR.DME = 0): SB_C2DST fica 1 e nada e entregue
    HH_CHECK(hh::sh4_dmac_write(&c, kSar2, 4, 0x0C010000) && hh::sh4_dmac_write(&c, kChcr2, 4, 1));
    HH_CHECK(hh::holly_sb_write(&c, kC2Dstat, 4, 0x10000000) && hh::holly_sb_write(&c, kC2Dlen, 4, 64));
    HH_CHECK(hh::holly_sb_write(&c, kC2Dst, 4, 1));
    HH_CHECK(g_got.empty() && sb(kC2Dst) == 1 && !(ist() & (1u << 19)));
    // habilitado: dois blocos na ordem, o endereco da FIFO nao avanca; SAR2 avanca, DMATCR2 zera, CHCR2.TE acende;
    // C2DSTAT avanca, C2DLEN e C2DST zeram, ISTNRM bit 19 acende e o proximo evento e avaliado ja
    HH_CHECK(hh::sh4_dmac_write(&c, kDmaor, 4, 1) && hh::sh4_dmac_write(&c, kDmatcr2, 4, 2));
    c.next_event = 12345;
    HH_CHECK(hh::holly_sb_write(&c, kC2Dst, 4, 1));
    HH_CHECK(g_got.size() == 2 && g_got[0].first == 0x10000000 && g_got[1].first == 0x10000000);
    HH_CHECK(g_got[0].second[0] == 0x1000 && g_got[0].second[7] == 0x1007 && g_got[1].second[0] == 0x1008);
    HH_CHECK(dmac(kSar2) == 0x0C010040 && dmac(kDmatcr2) == 0 && dmac(kChcr2) == 3 && dmac(kDmaor) == 1);
    HH_CHECK(sb(kC2Dstat) == 0x10000040 && sb(kC2Dlen) == 0 && sb(kC2Dst) == 0 && c.next_event == 0);
    HH_CHECK(ist() & (1u << 19));
    // caminho de textura: o destino avanca bloco a bloco
    g_got.clear();
    HH_CHECK(hh::holly_sb_write(&c, kC2Dstat, 4, 0x11000100) && hh::holly_sb_write(&c, kC2Dlen, 4, 64));
    HH_CHECK(hh::holly_sb_write(&c, kC2Dst, 4, 1));
    HH_CHECK(g_got.size() == 2 && g_got[0].first == 0x11000100 && g_got[1].first == 0x11000120);
    HH_CHECK(g_got[0].second[0] == 0x1010 && dmac(kSar2) == 0x0C010080);
    // as escritas da tabela de inicializacao (0x8C15FAC8), inclusive as depois do INTC (revisao: a regressao do Builder
    // parou o boot de teste em 0x005F6940 antes desta faixa entrar no contrato)
    for (uint32_t a : {0xA05F6884u, 0xA05F6940u, 0xA05F6944u, 0xA05F6950u, 0xA05F6954u, 0xA05F7418u, 0xA05F7810u,
                       0xA05F7C00u, 0xA05F7C80u})
        HH_CHECK(hh::holly_sb_write(&c, a, 4, 0));
    // SB_SBREV e SB_FFST
    HH_CHECK(sb(kSbRev) == 0x0B && sb(kFfst) == 0);
    HH_CHECK(hh::holly_sb_write(&c, kFfst, 4, 0x21) && sb(kFfst) == 0x21);
    hh::sh4_set_fatal_handler([](hh::Sh4Context*, const char* w, uint32_t, uint32_t) { throw Fatal(w); });
    auto fatal = [](auto&& f) {
        try {
            f();
        } catch (const Fatal&) {
            return true;
        }
        return false;
    };
    uint32_t v = 0;
#ifdef HH_PRODUTO_STUBS
    // o produto: fora do contrato e erro explicito (canal 0 do DMAC; registrador do system block nao medido)
    HH_CHECK(fatal([&] { hh::sh4_dmac_read(&c, 0xFFA00000, 4, v); }));
    HH_CHECK(fatal([&] { hh::sh4_dmac_write(&c, 0xFFA0003C, 4, 1); }));
    HH_CHECK(fatal([&] { hh::holly_sb_read(&c, 0xA05F7000, 4, v); }));
    HH_CHECK(fatal([&] { hh::holly_sb_write(&c, 0xA05F7000, 4, 1); }));
    HH_CHECK(fatal([&] { hh::holly_sb_read(&c, kC2Dst, 2, v); }));
#else
    // a verificacao: o modelo guarda o resto (o valor escrito volta)
    HH_CHECK(!fatal([&] { HH_CHECK(hh::sh4_dmac_write(&c, 0xFFA00000, 4, 7) && dmac(0xFFA00000) == 7); }));
    HH_CHECK(!fatal([&] { HH_CHECK(hh::holly_sb_write(&c, 0xA05F7000, 4, 9) && sb(0xA05F7000) == 9); }));
#endif
    HH_CHECK(!hh::sh4_dmac_read(&c, 0xFFA00080, 4, v));  // fora da regiao do DMAC: nao e do DMAC
    HH_CHECK(!hh::holly_sb_read(&c, 0xA05F8000, 4, v));  // o banco do PVR: nao e do system block
    hh::sh4_set_fatal_handler(nullptr);
    hh::dma_bus_set_sink(nullptr);
    hh::holly_sb_reset();
    hh::sh4_dmac_reset();
}
