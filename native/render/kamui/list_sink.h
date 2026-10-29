// SPDX-License-Identifier: MIT
// Derivado de recomp-BStinger native/render/kamui/list_sink.h (MIT, Copyright (c) 2026 Blue Stinger
// Recompiled contributors; ver third_party/licenses/recomp-BStinger-LICENSE.txt).
#pragma once
// HH15.7b: o destino nativo das listas que o Kamui entrega ao TA. Recebe os blocos de 32 bytes da FIFO de poligonos
// (pelo DMA do canal 2), junta os parametros de 64 bytes, acompanha a lista aberta e conta os bytes (o prazo do fim de
// render depende deles). No STARTRENDER, decodifica o que chegou desde o TA_LIST_INIT direto para a IR (CommandList + a
// tabela das texturas usadas), com as regras do decodificador do legado (o Parser do pvr_render + a montagem da captura
// da IR), sem ler nenhum registrador: o que vem do quadro (FrameParams, stride das texturas) chega pronto.
//
// O formato e o dos parametros do TA (PCW, ISP/TSP/TCW, vertices): e o que o codigo recompilado escreve. O que sai e a
// IR nativa; o hardware simulado (FIFO, interrupcoes, registradores) nao passa por aqui.
#include "render/render.h"
#include "render/soft/soft_raster.h"

#include <cstdint>
#include <vector>

namespace hh::native::render::kamui {

// Um parametro do TA: 8 ou 16 palavras e a lista em que chegou (0 opaca, 1 modificador opaco, 2 translucida,
// 3 modificador translucido, 4 punch-through).
struct ListParam {
    uint32_t list = 0, words = 0;
    uint32_t w[16] = {};
};

// O que identifica os texels de uma textura (a IR so guarda o handle): TCW, tamanho do TSP e o stride linear.
struct TextureKey {
    uint32_t tcw = 0, width = 0, height = 0, stride = 0;
    bool     operator==(const TextureKey&) const = default;
};

// O resultado de um render: a IR, a tabela das texturas (TextureHandle.value - 1) e as contagens do decodificador.
struct ListFrame {
    CommandList             cl;
    std::vector<TextureKey> textures;
    uint32_t                texture_stride = 0;
    uint32_t modifier_params = 0, modifier_volumes = 0, modifier_unclosed = 0, skipped_params = 0, sprites = 0;
    // vertices de estado sem suporte (textura que o decodificador nao le, dois volumes), que a IR nao representa: os
    // parametros omitidos do decodificador do legado (skipped_params)
    uint32_t unsupported = 0;
};

// Evento de um bloco: fim de uma lista (o numero dela) ou um parametro com tipo invalido.
struct BlockResult {
    int  list_end = -1;  // 0..4: a lista que acabou de fechar
    bool invalid  = false;
};

class ListSink {
public:
    // TA_LIST_INIT: listas novas (descarta o que havia); TA_LIST_CONT: continua as listas, sem lista aberta
    void list_init();
    void list_continue();
    // um bloco de 32 bytes da FIFO de poligonos
    BlockResult                   feed(const uint32_t block[8]);
    uint32_t                      bytes() const { return bytes_; }  // bytes recebidos desde o TA_LIST_INIT
    const std::vector<ListParam>& params() const { return params_; }

    // Decodifica as listas recebidas na IR do render: `frame` (o tamanho e os parametros do quadro) e `texture_stride`
    // (TEXT_CONTROL) vem do chamador.
    void build(const FrameParams& frame, uint32_t texture_stride, ListFrame& out) const;

private:
    std::vector<ListParam> params_;
    uint32_t               bytes_ = 0;
    int                    list_ = -1;  // lista aberta (-1: nenhuma)
    bool                   vertex64_ = false, pending_half_ = false;
    ListParam              half_;
    void                   push(const uint32_t* w, uint32_t words);
};

// Conversoes do estado e dos vertices decodificados para a IR (as regras ficam aqui, num lugar so; a captura do legado
// as usa tambem).
DrawState draw_state(const soft::Tri& t, uint32_t texture_handle);
Vertex    ir_vertex(const soft::Vertex& v);
// Os formatos de textura que o decodificador le (os outros: o poligono e omitido), os do decodificador do legado.
bool texture_supported(uint32_t tcw);

} // namespace hh::native::render::kamui
