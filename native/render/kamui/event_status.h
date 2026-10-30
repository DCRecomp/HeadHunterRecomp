// SPDX-License-Identifier: MIT
// Derivado de recomp-BStinger native/render/kamui/event_status.h (MIT, Copyright (c) 2026 Blue Stinger
// Recompiled contributors; ver third_party/licenses/recomp-BStinger-LICENSE.txt).
#pragma once
// HH15.7d2: o banco minimo de status de interrupcao que o tratador do Kamui, a Ninja e o jogo leem e limpam
// (o controlador de interrupcoes do system block, deslocamentos 0x00 a 0x3F), alimentado por eventos nativos: VBlank-In/
// Out, fim de render, fim de lista, fim do DMA do canal 2 e do Maple, e o nivel da fonte externa da AICA. Nao e o
// controlador simulado: guarda o status e as mascaras, aplica os eventos entregues na ordem de hoje e diz o nivel IRL
// pedido ao SH-4. Quando um evento vence continua com o provedor de tempo do runtime (nenhum relogio, ordem ou prazo muda).
//   ISTNRM 0x00 (normal), ISTEXT 0x04 (externo, nivel da fonte), ISTERR 0x08 (erro): escrever 1 limpa o bit de ISTNRM e
//   ISTERR; o ISTEXT so a fonte apaga. Lido, o ISTNRM tem o bit 30 aceso enquanto o ISTEXT tiver algum.
//   IML2NRM..IML6ERR 0x10-0x38: mascaras por nivel (NRM, EXT, ERR), guardadas.
#include <array>
#include <cstdint>

namespace hh::native::render::kamui {

class EventStatus {
public:
    static constexpr uint32_t kSize = 0x40;  // bytes do banco
    static constexpr uint32_t kIstNrm = 0x00, kIstExt = 0x04, kIstErr = 0x08;
    static constexpr uint32_t kVblankIn = 1u << 3, kVblankOut = 1u << 4, kRenderDone = 0x7, kMapleDma = 1u << 12;
    static constexpr uint32_t kExtSummary = 1u << 30;

    // eventos agendados que venceram num ponto de avaliacao (o provedor de tempo decide quais)
    struct Due {
        bool vblank_in = false, vblank_out = false, render = false, maple = false;
        bool any() const { return vblank_in || vblank_out || render || maple; }
    };

    static bool contains(uint32_t off) { return off < kSize; }

    void reset();
    // aplica os eventos vencidos, na ordem do controlador anterior: VBlank-In (com a contagem dos que chegam com o bit
    // ainda aceso), VBlank-Out, fim de render (bits 0-2), fim do DMA do Maple (bit 12)
    void deliver(const Due& d);
    // evento levantado na hora (fim de lista, fim do DMA do canal 2, fim do Maple, fim do DMA do G2)
    void raise(uint32_t bits);
    // nivel de uma fonte externa (ISTEXT): nivel, nao pulso; o reconhecimento e na fonte
    void set_external(uint32_t bits, bool on);
    // escrita do jogo (deslocamento): ISTNRM/ISTERR limpam os bits em 1, ISTEXT nao muda, IML* guarda a mascara
    void write(uint32_t off, uint32_t v);

    uint32_t reg(uint32_t off) const { return regs_[off / 4]; }
    // o valor lido pelo jogo (o ISTNRM com o resumo do ISTEXT no bit 30)
    uint32_t read(uint32_t off) const { return off == kIstNrm && reg(kIstExt) ? reg(off) | kExtSummary : reg(off); }
    // maior nivel (6, 4, 2) cujas mascaras IMLn tem algum bit pendente em ISTNRM/ISTEXT/ISTERR; 0 se nenhum
    uint32_t irl_level() const;

    uint64_t vblanks() const { return vblanks_; }  // VBlank-In entregues desde o reset
    uint64_t fused() const { return fused_; }      // VBlank-In que chegaram com o bit ainda aceso
    uint64_t events() const { return events_; }    // eventos entregues ou levantados (mesmo mascarados)

private:
    uint32_t& at(uint32_t off) { return regs_[off / 4]; }

    std::array<uint32_t, kSize / 4> regs_{};
    uint64_t                        vblanks_ = 0, fused_ = 0, events_ = 0;
};

} // namespace hh::native::render::kamui
