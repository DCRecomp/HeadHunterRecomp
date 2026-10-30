// SPDX-License-Identifier: MIT
// HH15.7d2: o banco de status de interrupcao nativo (native/render/kamui/event_status, derivado do BS) e o contrato do
// controlador de interrupcoes do Holly pela API que a memoria, o tempo e os servicos chamam (pvr/holly_intc.h), com
// valores esperados fixos (os do controlador anterior). Compilado duas vezes: no hh_runtime_tests (o controlador
// anterior, papel intc, com o banco nativo em sombra comparado a cada operacao) e no hh15_produto_sb_tests com o
// servico do produto (system/event_bus.cpp, HH_PRODUTO_STUBS). Negativo: HH_EVENT_STATUS_MUTACAO=limpa (a sombra
// ignora a limpeza do bit 19) reprova hh15_intc_banco_nativo_no_contrato na verificacao.
#include "test.h"

#include "cpu/sh4_mem.h"
#include "cpu/sh4_runtime.h"
#include "cpu/time_hooks.h"
#include "maple/maple.h"
#include "pvr/holly_intc.h"
#include "system/render_bus.h"
#include "system/video_bus.h"
#include "render/kamui/event_status.h"

#include <string>
#include <vector>

using hh::native::render::kamui::EventStatus;

// a sequencia do tratador do Kamui por render: fim do DMA do canal 2 + fim de lista, limpos pelo tratador
HH_TEST(hh15_event_status_levanta_e_limpa_com_1) {
    EventStatus s;
    s.reset();
    s.raise((1u << 19) | (1u << 8) | (1u << 4));
    HH_CHECK(s.reg(EventStatus::kIstNrm) == 0x80110 && s.events() == 1);
    s.write(EventStatus::kIstNrm, (1u << 19) | (1u << 8));  // limpa so os bits em 1
    HH_CHECK(s.reg(EventStatus::kIstNrm) == 0x10);
    s.write(EventStatus::kIstErr, 0xFFFFFFFF);  // limpar o que nao esta aceso nao acende nada
    HH_CHECK(s.reg(EventStatus::kIstErr) == 0);
}

// VBlank-In entregue com o bit ainda aceso conta como fundido; fim de render acende 0-2 e o Maple, o 12
HH_TEST(hh15_event_status_entrega_conta_vblanks_e_fundidos) {
    EventStatus      s;
    EventStatus::Due d;
    s.reset();
    const uint64_t e0 = s.events();
    d.vblank_in       = true;
    s.deliver(d);
    HH_CHECK(s.vblanks() == 1 && s.fused() == 0 && s.reg(EventStatus::kIstNrm) == EventStatus::kVblankIn);
    s.deliver(d);  // nao reconhecido: um atendimento para dois campos
    HH_CHECK(s.vblanks() == 2 && s.fused() == 1);
    EventStatus::Due r;
    r.render = r.maple = r.vblank_out = true;
    s.deliver(r);
    HH_CHECK(s.reg(EventStatus::kIstNrm) == (EventStatus::kVblankIn | EventStatus::kVblankOut | 0x7 | (1u << 12)));
    HH_CHECK(s.events() == e0 + 3);
    s.deliver(EventStatus::Due{});  // nada venceu: nao conta
    HH_CHECK(s.events() == e0 + 3);
    s.reset();  // as contagens de VBlank zeram; a de eventos e do processo
    HH_CHECK(s.vblanks() == 0 && s.fused() == 0 && s.events() == e0 + 3 && s.reg(EventStatus::kIstNrm) == 0);
}

// o nivel e o maior cujas mascaras (NRM, EXT, ERR) pegam um bit pendente; o ISTEXT e nivel da fonte (a escrita do jogo
// nao o apaga) e acende o bit 30 do ISTNRM lido
HH_TEST(hh15_event_status_nivel_mascaras_e_fonte_externa) {
    EventStatus s;
    s.reset();
    s.raise(EventStatus::kVblankIn);
    HH_CHECK(s.irl_level() == 0);  // sem mascara
    s.write(0x10, EventStatus::kVblankIn);  // IML2NRM
    HH_CHECK(s.irl_level() == 2);
    s.write(0x20, EventStatus::kVblankIn);  // IML4NRM
    HH_CHECK(s.irl_level() == 4);
    s.write(0x30, EventStatus::kVblankIn);  // IML6NRM
    HH_CHECK(s.irl_level() == 6);
    s.write(EventStatus::kIstNrm, EventStatus::kVblankIn);
    HH_CHECK(s.irl_level() == 0);
    s.write(0x28, 1);  // IML4ERR
    HH_CHECK(s.irl_level() == 0 && s.reg(0x28) == 1);
    s.set_external(1u << 1, true);
    HH_CHECK(s.read(EventStatus::kIstNrm) == EventStatus::kExtSummary && s.reg(EventStatus::kIstNrm) == 0);
    s.write(0x24, 1u << 1);  // IML4EXT
    HH_CHECK(s.irl_level() == 4);
    s.write(EventStatus::kIstExt, 0xFFFFFFFF);  // so a fonte apaga
    HH_CHECK(s.reg(EventStatus::kIstExt) == (1u << 1));
    s.set_external(1u << 1, false);
    HH_CHECK(s.read(EventStatus::kIstNrm) == 0 && s.irl_level() == 0);
}

namespace {

uint64_t differ() {
    const std::string s = hh::holly_event_status_summary();
    const size_t      p = s.find("diferentes ");
    return p == std::string::npos ? ~0ull : std::stoull(s.substr(p + 11));
}

} // namespace

// o contrato pela API: levantados, mascaras, nivel IRL, limpeza pelo tratador (sh4_write32 em 0xA05F6900), ISTEXT
HH_TEST(hh15_intc_banco_nativo_no_contrato) {
    std::vector<uint8_t> ram(16u << 20);
    hh::Sh4Context       c{};
    c.ram = ram.data();
    hh::holly_intc_reset(0);
    auto rd = [&](uint32_t a) {
        uint32_t v = 0xDEADBEEF;
        HH_CHECK(hh::holly_intc_read(&c, a, 4, v));
        return v;
    };
    const uint64_t e0 = hh::holly_event_count();
    hh::holly_intc_raise((1u << 19) | (1u << 8));  // fim do DMA do canal 2 + fim do modificador opaco
    HH_CHECK(rd(0xA05F6900) == ((1u << 19) | (1u << 8)) && hh::holly_event_count() == e0 + 1);
    HH_CHECK(hh::holly_irl_level_now() == 0);
    hh::sh4_write32(&c, 0xA05F6920, 1u << 19);  // IML4NRM
    HH_CHECK(hh::holly_irl_level(c.cycles) == 4 && rd(0xA05F6920) == (1u << 19));
    hh::sh4_write32(&c, 0xA05F6930, 1u << 8);  // IML6NRM
    HH_CHECK(hh::holly_irl_level(c.cycles) == 6);
    hh::sh4_write32(&c, 0xA05F6900, (1u << 19) | (1u << 8));  // o tratador do Kamui limpa os dois
    HH_CHECK(rd(0xA05F6900) == 0 && hh::holly_irl_level(c.cycles) == 0);
    hh::holly_intc_set_external(hh::kIstExtAica, true);  // pedido da AICA: nivel no ISTEXT, resumo no bit 30
    HH_CHECK(rd(0xA05F6904) == hh::kIstExtAica && rd(0xA05F6900) == (1u << 30));
    hh::sh4_write32(&c, 0xA05F6914, hh::kIstExtAica);  // IML2EXT
    HH_CHECK(hh::holly_irl_level(c.cycles) == 2);
    hh::sh4_write32(&c, 0xA05F6904, 0xFFFFFFFF);  // o jogo nao apaga o ISTEXT
    HH_CHECK(rd(0xA05F6904) == hh::kIstExtAica);
    hh::holly_intc_set_external(hh::kIstExtAica, false);
    HH_CHECK(rd(0xA05F6900) == 0 && hh::holly_irl_level(c.cycles) == 0);
    uint32_t v = 0;
    HH_CHECK(!hh::holly_intc_read(&c, 0xA05F6940, 4, v) && !hh::holly_intc_read(&c, 0xA05F6900, 2, v));
#ifdef HH_PRODUTO_STUBS
    HH_CHECK(hh::holly_event_status_summary().empty());  // no produto nao ha referencia
#else
    // a sombra (o banco nativo) igual ao controlador anterior em toda operacao
    HH_CHECK(hh::holly_event_status_summary().find("[event-status] operacoes comparadas ") == 0);
    HH_CHECK(differ() == 0);
#endif
    hh::holly_intc_reset(0);
}

namespace {

// provedor de tempo roteirizado: holly_due devolve o proximo item do roteiro (um por avaliacao com instante novo)
struct Roteiro final : hh::TimeProvider {
    std::vector<uint32_t> due;
    size_t                next = 0;
    const char*           name() const override { return "roteiro"; }
    void                  reset(uint64_t) override {}
    uint64_t              clock(uint64_t cycles) override { return cycles; }
    uint32_t              holly_due(uint64_t) override { return next < due.size() ? due[next++] : 0; }
    void                  spg_changed() override {}
    void                  render_scheduled(uint64_t) override {}
    void                  maple_scheduled(uint64_t) override {}
    void                  maple_cancel() override {}
    void                  disc_scheduled(uint64_t) override {}
    bool                  disc_due(uint64_t) override { return false; }
    void                  disc_cancel() override {}
    uint32_t              rtc_seconds(uint64_t) override { return 0; }
    void                  rtc_set(uint32_t, uint64_t) override {}
};

struct Gancho {
    int      calls = 0;
    uint64_t vblanks = 0, at = 0;
    uint32_t ist = 0;
};

} // namespace

// revisao Codex 7d2: a entrega dos eventos vencidos (holly_intc_sync, a avaliacao no acesso e no nivel IRL) com os
// efeitos do controlador anterior, nas duas versoes: os bits, as contagens, o instante do ultimo VBlank-In, o consumo do
// fim de render (render_bus_done_take) e do fim do DMA do Maple (maple_dma_take) e a apresentacao do quadro
// (video_bus_vblank) depois do estado atualizado
HH_TEST(hh15_intc_entrega_de_eventos_vencidos) {
    std::vector<uint8_t> ram(16u << 20);
    hh::Sh4Context       c{};
    c.ram = ram.data();
    Roteiro t;
    hh::time_set_provider(&t);
    hh::holly_intc_reset(0);
    hh::maple_reset();
    hh::maple_set_controller_present(false);  // o barramento sem dispositivos (o do produto)
    Gancho g;
    hh::video_bus_set_vblank_hook(
        [](void* arg) {
            auto& x = *static_cast<Gancho*>(arg);
            ++x.calls, x.vblanks = hh::holly_vblank_count(), x.at = hh::holly_last_vblank_instant();
            hh::Sh4Context k{};
            hh::holly_intc_read(&k, 0xA05F6900, 4, x.ist);  // instante 0: nada novo e avaliado
        },
        &g);
    const uint64_t e0 = hh::holly_event_count();
    // um render agendado e um DMA do Maple em curso
    c.cycles = 10;
    hh::render_bus_start(&c, 0, 0, [](hh::Sh4Context*) {}, nullptr, nullptr);
    hh::maple_write(&c, 0xA05F6C14, 1), hh::maple_write(&c, 0xA05F6C04, 0x0C100000);
    hh::sh4_write32(&c, 0x0C100000, 0x80000001u), hh::sh4_write32(&c, 0x0C100004, 0x0C110000);
    hh::sh4_write32(&c, 0x0C100008, 0x01002001), hh::sh4_write32(&c, 0x0C10000C, 0);
    hh::maple_write(&c, 0xA05F6C18, 1);
    HH_CHECK(hh::render_bus_done_due() != 0 && hh::maple_dma_due() != 0);
    t.due = {hh::kDueVblankIn, hh::kDueVblankOut | hh::kDueRender, hh::kDueMaple | hh::kDueVblankIn, 0};
    hh::holly_intc_sync(100);  // VBlank-In
    HH_CHECK(g.calls == 1 && g.vblanks == 1 && g.at == 100 && (g.ist & hh::kIstVblankIn));
    HH_CHECK(hh::holly_vblank_count() == 1 && hh::holly_last_vblank_instant() == 100 && hh::holly_vblank_fused() == 0);
    HH_CHECK(hh::render_bus_done_due() != 0 && hh::maple_dma_due() != 0);
    hh::holly_intc_sync(100);  // mesmo instante: nada avaliado
    HH_CHECK(t.next == 1);
    c.cycles = 200;  // a leitura avalia: VBlank-Out + fim de render (consumido)
    uint32_t v = 0;
    HH_CHECK(hh::holly_intc_read(&c, 0xA05F6900, 4, v) && v == (hh::kIstVblankIn | hh::kIstVblankOut | 0x7));
    HH_CHECK(hh::render_bus_done_due() == 0 && hh::maple_dma_due() != 0 && g.calls == 1);
    HH_CHECK(hh::holly_irl_level(300) == 0);  // o nivel avalia: fim do Maple (consumido) + VBlank-In nao reconhecido
    HH_CHECK(hh::maple_dma_due() == 0 && g.calls == 2 && g.at == 300);
    HH_CHECK(hh::holly_vblank_count() == 2 && hh::holly_vblank_fused() == 1);
    HH_CHECK(hh::holly_irl_level_now() == 0 && hh::holly_event_count() == e0 + 3);
    hh::holly_intc_read(&c, 0xA05F6900, 4, v);
    HH_CHECK(v == (hh::kIstVblankIn | hh::kIstVblankOut | 0x7 | hh::kIstMapleDma));
    hh::holly_intc_sync(50);  // instante anterior: ignorado
    HH_CHECK(t.next == 3);
#ifndef HH_PRODUTO_STUBS
    HH_CHECK(differ() == 0);
#endif
    hh::video_bus_set_vblank_hook(nullptr, nullptr);
    hh::maple_reset();
    hh::maple_set_controller_present(true);
    hh::time_set_provider(nullptr);
    hh::holly_intc_reset(0);
}
