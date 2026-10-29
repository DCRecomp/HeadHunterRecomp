// SPDX-License-Identifier: MIT
// Derivado de recomp-BStinger native/render/kamui/frame_state.h (MIT, Copyright (c) 2026 Blue Stinger
// Recompiled contributors; ver third_party/licenses/recomp-BStinger-LICENSE.txt).
#pragma once
// HH15.7c: o estado do quadro que o Kamui programa pelo funil de registradores (o banco grafico, deslocamentos 0x0000 a
// 0x1FFF), como um servico nativo, sem o hardware simulado. Guarda o que o jogo grava, responde o que ele le (ID,
// REVISION, contador do YUV) e diz o efeito das escritas de comando (inicio do render, listas novas ou continuadas), que
// o chamador executa. Sobre ele e a memoria das texturas (texture_heap.h): os parametros do quadro da IR, a cor do
// fundo, o aviso do Region Array e a gravacao da imagem do render no framebuffer, com as contas do caminho do TA.
// A temporizacao do video (SPG, 0x0C8-0x0E0 e SPG_STATUS) fica com o chamador (HH15.7d).
#include "render/kamui/texture_heap.h"
#include "render/render.h"

#include <array>
#include <cstdint>
#include <span>

namespace hh::native::render::kamui {

class FrameState {
public:
    static constexpr uint32_t kSize = 0x2000;  // bytes do banco
    // deslocamentos no banco
    static constexpr uint32_t kId = 0x000, kRevision = 0x004, kSoftReset = 0x008, kStartRender = 0x014;
    static constexpr uint32_t kSpgStatus = 0x10C, kTaIspBase = 0x128, kTaNextOpb = 0x134, kTaItpCurrent = 0x138;
    static constexpr uint32_t kTaListInit = 0x144, kTaYuvTexCnt = 0x150, kTaListCont = 0x160, kTaNextOpbInit = 0x164;
    static constexpr uint32_t kPalRamCtrl = 0x108, kPalette = 0x1000, kTextControl = 0x0E4, kTileClip = 0x13C;

    enum class Effect { None, StartRender, ListInit, ListContinue };

    void load(const uint32_t* words);  // o banco inteiro deixado pelo boot (kSize / 4 palavras)
    uint32_t        reg(uint32_t off) const { return regs_[(off & (kSize - 1)) / 4]; }  // o valor guardado
    void            set(uint32_t off, uint32_t v) { regs_[(off & (kSize - 1)) / 4] = v; }
    const uint32_t* data() const { return regs_.data(); }
    uint32_t*       data() { return regs_.data(); }  // o banco emprestado ao caminho do TA (modos de verificacao)

    // escrita pelo funil: guarda o valor (salvo nos registradores de comando e nos so de leitura) e devolve o efeito;
    // TA_LIST_INIT com o bit 31 recomeca os ponteiros do TA (NEXT_OPB = NEXT_OPB_INIT, ITP_CURRENT = ISP_BASE)
    Effect write(uint32_t off, uint32_t v);
    // leitura pelo funil (ID, REVISION, contador do YUV; o resto, o valor guardado). SPG_STATUS e do chamador.
    uint32_t read(uint32_t off) const;

    uint32_t palette_argb(uint32_t index) const;  // entrada 0..1023 em ARGB8888 (formato de PAL_RAM_CTRL)
    uint32_t texture_stride() const { return (reg(kTextControl) & 0x1F) * 32; }  // TEXT_CONTROL
    uint32_t render_width() const { return ((reg(kTileClip) & 0x3F) + 1) * 32; }  // TA_GLOB_TILE_CLIP
    uint32_t render_height() const { return (((reg(kTileClip) >> 16) & 0xF) + 1) * 32; }

private:
    std::array<uint32_t, kSize / 4> regs_{};
};

// A cor RGB do fundo (ISP_BACKGND_T: ISP/TSP/TCW e 3 vertices na memoria de parametros, PARAM_BASE): a cor base do
// primeiro vertice. `textured` = o fundo pede textura (o rasterizador usa so a cor).
uint32_t background_color(const FrameState& fs, const TextureHeap& heap, bool* textured = nullptr);
// Os parametros do quadro da IR (width x height): fundo, profundidade do fundo, fog, punch-through, ordenacao, amostras,
// descarte pequeno e clamp.
FrameParams frame_params(const FrameState& fs, const TextureHeap& heap, uint32_t width, uint32_t height);
// Region Array tipo 2 (FPU_PARAM_CFG bit 21): true se algum tile pede um modo de ordenacao diferente do global (o
// rasterizador usa um modo para a tela inteira; o chamador avisa).
bool region_sort_differs(const FrameState& fs, const TextureHeap& heap, bool autosort);
// A imagem do render (ARGB8888 width x height) no framebuffer: FB_W_SOF1, formato de FB_W_CTRL (com o K e o limiar de
// alfa), FB_W_LINESTRIDE e o recorte FB_X_CLIP/FB_Y_CLIP. framebuffer_mismatch conta os pixels cujos bytes no
// framebuffer nao sao os que a gravacao poria (o comparador da verificacao, depois da gravacao do caminho do TA).
void     write_framebuffer(const FrameState& fs, TextureHeap& heap, std::span<const uint32_t> image, uint32_t width,
                           uint32_t height);
uint32_t framebuffer_mismatch(const FrameState& fs, const TextureHeap& heap, std::span<const uint32_t> image,
                              uint32_t width, uint32_t height);

} // namespace hh::native::render::kamui
