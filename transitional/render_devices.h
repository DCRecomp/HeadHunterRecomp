// SPDX-License-Identifier: MIT
// Derivado de recomp-BStinger transitional/render_capture.h (MIT, Copyright (c) 2026 Blue Stinger
// Recompiled contributors; ver third_party/licenses/recomp-BStinger-LICENSE.txt).
#pragma once
// HH15.7d4b: o desenho da IR de render (native/render/render.h) separado da captura (render_capture.h, so verificacao):
//   - os dois dispositivos sobre a IR, o Vulkan (native/render/vulkan, com a recuperacao da perda do dispositivo, a
//     resolucao interna e as substituicoes HD) e o software (native/render/soft), com os caches de textura (refeita quando
//     os bytes dela ou a paleta mudam), pela fonte de texels que o dono escolhe (TextureSource);
//   - o renderizador do produto (install_renderer, draws_lists, draw_list): a IR do ListSink nativo
//     (transitional/kamui_device, modo nativo) desenhada no Vulkan com o software de recuo, ou no software, com os
//     texels so da memoria nativa (set_native_memory); o resumo no mesmo formato da captura (format_summary), que os
//     portoes leem.
// Os tipos comuns (Frame, Stats, Mode, Fault, HiresImage, DumpTexture) e os ajustes globais (escala, substituicoes HD,
// falhas injetadas, memoria nativa) ficam aqui e valem para os dois donos; a captura monta a IR do caminho do TA e usa
// os mesmos dispositivos com os texels do runtime.
#include "render/kamui/frame_state.h"
#include "render/kamui/list_sink.h"
#include "render/kamui/texture_heap.h"
#include "render/render.h"
#include "render/texture_replacement.h"

#include <array>
#include <cstdint>
#include <deque>
#include <map>
#include <memory>
#include <span>
#include <string>
#include <tuple>
#include <vector>

namespace hh::native::render {
class VulkanDevice;
namespace soft {
class SoftwareDevice;
}
} // namespace hh::native::render

namespace hh::transitional::render_capture {

namespace nr = hh::native::render;

// O que identifica os texels de uma textura (a IR so guarda o handle): TCW (endereco, formato, paleta, modos),
// tamanho do TSP e o stride linear (TEXT_CONTROL).
// HH15.7b: a mesma chave do ListSink nativo (native/render/kamui/list_sink.h).
using TextureKey = native::render::kamui::TextureKey;

struct Frame {
    nr::CommandList         cl;
    std::vector<TextureKey> textures;  // TextureHandle.value - 1
    uint32_t                texture_stride = 0;  // TEXT_CONTROL do render (stride das texturas lineares)
    uint32_t                modifier_params = 0, modifier_volumes = 0, modifier_unclosed = 0, skipped_params = 0,
                            sprites = 0;
};

struct Stats {
    uint64_t renders = 0, checked = 0, draws = 0, vertices = 0, triangles = 0, sprites = 0;
    uint64_t invalid = 0, triangle_mismatch = 0, image_mismatch = 0;  // tem de ficar em 0
    uint64_t textures_checked = 0, texture_mismatch = 0, textures_without_native = 0;
    uint64_t modifier_params = 0, modifier_volumes = 0, modifier_unclosed = 0, skipped_params = 0;
    // Cobertura de fidelidade: quantos desenhos realmente pediram cada modo de fog e em qual render apareceram
    // primeiro; o primeiro render com volumes permite extrair uma cena real para o oraculo do Flycast.
    std::array<uint64_t, 4> fog_draws{}, fog_first_render{};
    uint64_t modifier_first_render = 0;
    uint64_t frame_mismatch = 0;  // FrameParams que nao reproduzem os registradores usados (tem de ficar em 0)
    // N7b (Vulkan)
    std::string vk_device, vk_error;
    uint64_t    vk_frames = 0, vk_failures = 0, tex_uploads = 0, tex_legacy = 0, tex_evicted = 0;
    uint64_t    tex_replaced = 0, tex_replace_rejected = 0;  // substituicoes HD usadas / incompativeis (original)
    uint64_t    vk_fragment_retries = 0, vk_negative_depth = 0;  // listas por pixel refeitas; quadros com 1/w negativo
    uint64_t    vk_recoveries = 0;                                 // dispositivo recriado depois de uma perda
    double      vk_gpu_ms_total = 0, vk_gpu_ms_max = 0;           // tempo de GPU por quadro (total e o pior)
    uint64_t    vk_max_layers = 0;                                 // maior numero de camadas translucidas num pixel
    uint64_t    cmp_frames = 0, cmp_pixels = 0, cmp_diff = 0, cmp_diff8 = 0, cmp_worst_render = 0;
    uint32_t    cmp_max = 0;
    double      cmp_worst_fraction = 0;
    double      cmp_min_psnr = 0, cmp_psnr_sum = 0;  // PSNR (RGB) por quadro: minimo e soma (media = soma / quadros)
    uint64_t    cmp_below40 = 0;                     // quadros com PSNR < 40 dB (criterio do plano, secao 5)
    uint64_t distinct_texture_keys = 0, distinct_texture_ids = 0;  // chaves (TCW+tamanho) e ids de conteudo distintos
    // N7c: resolucao interna (Mode::Native com escala != 1)
    uint64_t hires_renders = 0, hires_shown = 0, hires_missed = 0;  // renders na escala; VBlanks com/sem a imagem maior
    double   hires_cpu_ms_total = 0;                                // leitura de volta (so a reduzida) na CPU
    uint32_t hires_w = 0, hires_h = 0;                              // tamanho do ultimo render na escala
    // HH15.7a: software sobre a IR (Mode::Software e o recuo do Native)
    uint64_t    soft_frames = 0, soft_failures = 0, soft_checked = 0, soft_mismatch = 0, soft_tex_uploads = 0, soft_tex_legacy = 0, soft_tex_evicted = 0;
    std::string soft_error;
    // HH15.7b: o ListSink nativo contra a captura (renders conferidos; divergentes tem de ficar em 0)
    uint64_t    list_checked = 0, list_mismatch = 0;
    std::string list_error;
    // HH15.7c: o estado nativo (quadro e texturas) contra o do runtime (divergentes e sem conversao tem de ficar em 0)
    uint64_t    native_checked = 0, native_frame_mismatch = 0, native_tex_checked = 0, native_tex_mismatch = 0;
    uint64_t    native_tex_missing = 0, frame_textures = 0;  // frame_textures: texturas dos renders (o total)
    std::string native_error;
    std::string first_error;
};

// HH15.7a: Software desenha a IR no renderizador em software (native/render/soft/soft_device, texturas pelas conversoes
// nativas) e a imagem substitui a do rasterizador sobre o TA; e tambem o recuo do Native quando o Vulkan falha.
// Off, Native e Software servem o renderizador do produto; Capture, Check e Compare sao so da captura.
enum class Mode : uint8_t { Off, Capture, Check, Native, Compare, Software };

// As contagens de um render (desenhos, vertices, sprites, texturas, volumes, fog) depois de `st.renders` contar ele.
void account(const Frame& f, Stats& st);
// As linhas do resumo ([render-ir], [render-vk], [render-nativo], [render-lista], [render-soft], [render-escala]).
std::string format_summary(const Stats& s);

// N7c: resolucao interna do renderizador nativo (so Mode::Native; o modo comparar e a referencia continuam em 1). Com
// escala != 1 o Vulkan desenha a geometria em round(w x escala) x round(h x escala) (vk_device.h) e:
//   - o framebuffer emulado recebe essa imagem REDUZIDA na GPU para o tamanho do jogo (media dos pixels), entao o resto
//     do caminho (VRAM, capturas, hash dos quadros, a apresentacao SDL) continua coerente e e o recuo;
//   - a imagem maior fica na GPU (um slot do dispositivo compartilhado) para a apresentacao direta.
// 1 = o modo fiel. Vale a partir do proximo render; chamar na thread do jogo.
void  set_scale(float scale);
float scale();

// A imagem maior do que esta sendo EXIBIDO: `shown` e o quadro que o video leu do framebuffer. Se ele e um dos ultimos
// renders na escala (o conteudo confere com a imagem reduzida que foi para o framebuffer, tolerando a quantizacao do
// formato do framebuffer), devolve o slot desse render; senao (filme, tela escrita pela CPU, escala 1), seq = 0. Thread
// do jogo. Vale para os dispositivos que desenharam por ultimo no Vulkan (do renderizador ou da captura).
struct HiresImage {
    uint64_t seq = 0;                    // slot no dispositivo compartilhado (VulkanDevice::last_slot)
    uint32_t width = 0, height = 0;      // tamanho da imagem maior
    uint32_t y0 = 0, rows = 0;           // as linhas que correspondem ao exibido
};
HiresImage hires_for(std::span<const uint32_t> shown, uint32_t width, uint32_t height);
// Le de volta as linhas exibidas de um slot (so para capturas e testes). false = o slot ja foi reaproveitado.
bool read_hires(const HiresImage& h, std::vector<uint32_t>& argb);

// Ponto de extensao das texturas HD (contrato do N7a): com provedor, o backend Vulkan troca cada textura pela
// substituicao do mesmo TextureId quando ela e compativel (replacement_compatible); senao, o original. nullptr =
// so originais (o padrao ate o N9, que traz o carregador de mods). O provedor tem de viver enquanto instalado.
void         set_texture_replacements(nr::ITextureReplacements* provider);
// Testes do recuo para o software: a proxima textura (Texture) ou o proximo quadro (Device) falha no Vulkan.
// Soft (HH15.7a): o proximo render do software sobre a IR falha.
enum class Fault : uint8_t { None, Texture, Device, DeviceOnce, DeviceAgain, Soft };  // Device: falha permanente; DeviceOnce: transitoria
void         inject_fault(Fault f);

// HH15.7c: o estado do quadro e a memoria das texturas nativos (transitional/kamui_device, modo nativo). Registrados, o
// render pela IR do ListSink le os texels deles (sem eles a textura nao e enviada e o render falha), e a captura confere
// com eles os parametros do quadro e os texels do runtime em todo render pelo caminho do TA; nullptr desliga.
void set_native_memory(const native::render::kamui::FrameState* fs, const native::render::kamui::TextureHeap* heap);
const native::render::kamui::FrameState*  native_frame_state();
const native::render::kamui::TextureHeap* native_texture_heap();

// De onde vem os texels de uma textura da IR: os bytes que a definem (para o cache saber se ela mudou) e o ARGB8888
// linear do maior nivel (`legacy`: pelo decodificador do legado, contado a parte). false = nao ha texels.
class TextureSource {
public:
    virtual ~TextureSource() = default;
    virtual void bytes(const TextureKey& k, std::vector<uint8_t>& raw) = 0;
    virtual bool argb(const TextureKey& k, std::vector<uint32_t>& argb, bool& legacy) = 0;
};
// A memoria nativa registrada (set_native_memory) pelas conversoes nativas (native/render/kamui/texture_source.h).
TextureSource& native_textures();

// Os dispositivos sobre a IR de um dono (o renderizador do produto ou a captura): os renders contam em `st` (st.renders
// e o numero do render anterior: as mensagens dizem st.renders + 1). Os draws do quadro voltam com os handles do quadro.
class Devices {
public:
    Devices(Stats& st, TextureSource& src);
    ~Devices();
    Devices(const Devices&)            = delete;
    Devices& operator=(const Devices&) = delete;

    // O Vulkan (escala `sc`; o modo comparar usa 1). Perda do dispositivo: recriado a partir dos dados da CPU e o MESMO
    // render refeito, ate kRecoveries por sessao; falhou de novo, o Vulkan para ate o fim da sessao. false = sem imagem.
    bool vk_render(Frame& f, float sc, uint32_t w, uint32_t h, std::vector<uint32_t>& out);
    // O software sobre a IR; false (com o motivo em st.soft_error) = sem imagem.
    bool soft_render(Frame& f, uint32_t w, uint32_t h, std::vector<uint32_t>& out);

    HiresImage hires_for(std::span<const uint32_t> shown, uint32_t w, uint32_t h);
    bool       read_hires(const HiresImage& h, std::vector<uint32_t>& argb, uint32_t& w);

private:
    static constexpr uint32_t kRecoveries = 3;
    bool ensure_vk();
    bool texture_handle(const TextureKey& k, nr::TextureHandle& out);
    void evict();
    void drop_vk(bool lost);
    bool vk_attempt(Frame& f, float sc, uint32_t w, uint32_t h, std::vector<uint32_t>& out, std::string* why);
    bool soft_texture(const TextureKey& k, nr::TextureHandle& out);

    Stats&         st;
    TextureSource& src_;
    std::vector<uint8_t>  raw_;
    std::vector<uint32_t> argb_;

    // ---- N7b: backend Vulkan ----
    std::shared_ptr<nr::VulkanDevice> vk_;  // o dispositivo do processo (o mesmo da apresentacao direta)
    bool                              vkTried_ = false;
    struct VkTex {
        nr::TextureHandle    handle;
        std::vector<uint8_t> raw;
        uint64_t             used = 0;
        uint32_t             w = 0, h = 0;  // o tamanho no dispositivo (o da substituicao, se houver)
        const void*          provider = nullptr;
    };
    std::map<std::tuple<uint32_t, uint32_t, uint32_t, uint32_t>, VkTex> vkTex_;

    // N7c: os ultimos renders na escala (o mais novo na frente): a imagem reduzida que foi para o framebuffer (para
    // achar o render exibido) e a maior. Tres por causa do buffer duplo (o exibido e o anterior ao ultimo) com folga.
    struct Hires {
        std::vector<uint32_t> down;
        uint64_t              seq = 0;
        uint32_t              w = 0, h = 0, iw = 0, ih = 0;
    };
    std::deque<Hires> hires_;

    // ---- HH15.7a: software sobre a IR ----
    std::unique_ptr<nr::soft::SoftwareDevice> soft_;
    struct SoftTex {
        nr::TextureHandle    handle;
        std::vector<uint8_t> raw;
        uint64_t             used = 0;
    };
    std::map<std::tuple<uint32_t, uint32_t, uint32_t, uint32_t>, SoftTex> softTex_;
};

// Diagnostico (HH_RENDER_IR_GUARDAR=<render>:<arquivo>): a IR inteira de um render (parametros, vertices, desenhos,
// volumes e as texturas ja convertidas pela fonte, na ordem dos handles do quadro) num arquivo binario, gravada ANTES de
// ir ao dispositivo (sobrevive a uma perda do dispositivo nesse render); load_frame_dump a le de volta para reproduzir o
// render fora do jogo (tests/unit/test_render_vulkan.cpp, HH_RENDER_IR_REPRODUZIR).
struct DumpTexture {
    nr::TextureDesc       desc;
    std::vector<uint32_t> argb;
};
bool save_frame_dump(const std::string& path, const Frame& f, TextureSource& src);
bool load_frame_dump(const std::string& path, nr::CommandList& cl, std::vector<DumpTexture>& textures);
// O render pedido por HH_RENDER_IR_GUARDAR (0 = nenhum) e o arquivo; grava se `render` (o numero do render, a partir
// de 1) e ele.
void maybe_save_frame_dump(uint64_t render, const Frame& f, TextureSource& src);

// ---- o renderizador do produto ----
// Native (Vulkan, com o software sobre a IR de recuo) ou Software; zera as estatisticas. Off desinstala e descarta.
void         install_renderer(Mode m);
Mode         renderer_mode();
const Stats& renderer_stats();
std::string  renderer_summary();  // vazio sem renderizador instalado
// true: o renderizador esta instalado (Software ou Native), e o render vem da IR do ListSink
bool draws_lists();
// O render pela IR do ListSink (`image`: width x height, ja com o fundo): false = nao desenhado (sem renderizador, ou
// falha do dispositivo; o chamador para com erro explicito).
bool draw_list(const native::render::kamui::ListFrame& lf, uint32_t width, uint32_t height, std::vector<uint32_t>& image);

} // namespace hh::transitional::render_capture
