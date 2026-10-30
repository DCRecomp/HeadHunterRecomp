// SPDX-License-Identifier: MIT
// Derivado de recomp-BStinger transitional/render_capture.cpp (MIT, Copyright (c) 2026 Blue Stinger
// Recompiled contributors; ver third_party/licenses/recomp-BStinger-LICENSE.txt).
#include "render_capture.h"

#include "pvr/pvr_regs.h"
#include "pvr/pvr_texture.h"
#include "pvr/ta.h"
#include "render/frame_prep.h"
#include "render/kamui/frame_state.h"
#include "render/kamui/list_sink.h"
#include "render/kamui/texture_heap.h"
#include "render/kamui/texture_source.h"
#include "render/render_ir.h"
#include "render/texture_convert.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <deque>
#include <map>
#include <tuple>
#include <memory>
#include <set>
#include <utility>

namespace hh::transitional::render_capture {
namespace {

namespace km = hh::native::render::kamui;

constexpr uint32_t R(uint32_t off) { return 0x005F8000 + off; }

float f32(uint32_t w) {
    float f;
    std::memcpy(&f, &w, 4);
    return f;
}

uint32_t pack(const PvrColor& c) {
    auto q = [](float v) { return uint32_t(std::lround((v < 0 ? 0 : (v > 1 ? 1 : v)) * 255)); };
    return (q(c.a) << 24) | (q(c.r) << 16) | (q(c.g) << 8) | q(c.b);
}
PvrColor unpack(uint32_t c) {  // a mesma expressao do decodificador (x / 255.f): o round-trip e exato
    return {((c >> 24) & 0xFF) / 255.f, ((c >> 16) & 0xFF) / 255.f, ((c >> 8) & 0xFF) / 255.f, (c & 0xFF) / 255.f};
}

nr::ListType list_type(uint32_t l) {
    switch (l) {
    case 1: return nr::ListType::OpaqueModifier;
    case 2: return nr::ListType::Translucent;
    case 3: return nr::ListType::TranslucentModifier;
    case 4: return nr::ListType::PunchThrough;
    default: return nr::ListType::Opaque;
    }
}
uint32_t list_number(nr::ListType l) {
    switch (l) {
    case nr::ListType::OpaqueModifier: return 1;
    case nr::ListType::Translucent: return 2;
    case nr::ListType::TranslucentModifier: return 3;
    case nr::ListType::PunchThrough: return 4;
    default: return 0;
    }
}

uint32_t log2_size(uint32_t n) {  // 8 << k
    uint32_t k = 0;
    while ((8u << k) < n && k < 7) ++k;
    return k;
}

TextureKey key_of(const PvrTri& t, uint32_t stride) {
    return {t.tcw, 8u << ((t.tsp >> 3) & 7), 8u << (t.tsp & 7), stride};
}

bool same_vertex(const PvrVertex& a, const PvrVertex& b) {
    return std::memcmp(&a, &b, sizeof a) == 0;  // exato: os dois lados sao os mesmos floats
}

const km::ListSink* g_list = nullptr;  // HH15.7b: o ListSink nativo (set_list_sink)

// Os texels do runtime (VRAM e paleta do modelo; sem conversao nativa, o decodificador do legado)
class LegacyTextures final : public TextureSource {
public:
    void bytes(const TextureKey& k, std::vector<uint8_t>& raw) override { texture_source(k, raw); }
    bool argb(const TextureKey& k, std::vector<uint32_t>& argb, bool& legacy) override { return texture_argb(k, argb, legacy); }
};

// Os parametros do quadro campo a campo (os floats pelos bits: a mesma conta tem de dar o mesmo valor; sem o
// preenchimento entre os campos)
bool same_frame(const nr::FrameParams& a, const nr::FrameParams& b) {
    auto bits = [](float f) {
        uint32_t w;
        std::memcpy(&w, &f, 4);
        return w;
    };
    return a.width == b.width && a.height == b.height && a.clear_color == b.clear_color &&
           bits(a.clear_depth) == bits(b.clear_depth) && a.fog_table == b.fog_table && a.fog_color == b.fog_color &&
           a.fog_vertex_color == b.fog_vertex_color && a.fog_density == b.fog_density &&
           bits(a.punch_through_ref) == bits(b.punch_through_ref) && a.translucent_autosort == b.translucent_autosort &&
           a.sample_center == b.sample_center && a.attribute_center == b.attribute_center &&
           a.texel_center == b.texel_center && bits(a.small_cull) == bits(b.small_cull) && a.clamp_min == b.clamp_min &&
           a.clamp_max == b.clamp_max;
}

// Negativos dos comparadores do estado nativo (HH_KAMUI_NATIVO_MUTACAO=quadro|textura): o lado nativo muda
bool native_mutation(const char* what) {
    static const char* m = std::getenv("HH_KAMUI_NATIVO_MUTACAO");
    return m && std::strcmp(m, what) == 0;
}

class Capture final : public PvrCaptureSink {
public:
    Mode     mode  = Mode::Off;
    uint32_t every = 1;
    Frame    frame;
    Stats    st;

    void begin(uint32_t w, uint32_t h) override {
        frame         = {};
        frame.cl.frame = frame_params(w, h);
        stride_       = (pvr_reg(R(0x0E4)) & 0x1F) * 32;  // TEXT_CONTROL
        frame.texture_stride = stride_;
        reset_strip();
        mod_open_ = false, mod_closing_ = false, mod_cur_closed_ = false;  // os volumes recomecam a cada render
        mod_instr_ = 0, mod_closed_ = 0;
    }

    void vertex(const PvrTri& state, const PvrVertex& v, bool end) override {
        strip_.push_back(v);
        const size_t n = strip_.size();
        if (n >= 3) {
            const nr::DrawState s = state_of(state);
            const bool contiguous = open_ >= 0 && frame.cl.draws[size_t(open_)].first_vertex +
                                                          frame.cl.draws[size_t(open_)].vertex_count ==
                                                      frame.cl.vertices.size();  // os vertices do strip sao os ultimos
            if (!tri_mode_ && contiguous && s == frame.cl.draws[size_t(open_)].state) {
                frame.cl.draws[size_t(open_)].vertex_count++;  // o strip continua com o mesmo estado
                frame.cl.vertices.push_back(render_capture::vertex(v));
            } else if (!tri_mode_ && n == 3) {  // o primeiro triangulo do strip abre o desenho
                open_ = int32_t(frame.cl.draws.size());
                frame.cl.draws.push_back({s, uint32_t(frame.cl.vertices.size()), 3});
                for (size_t i = 0; i < 3; ++i) frame.cl.vertices.push_back(render_capture::vertex(strip_[i]));
            } else {
                // estado mudou no meio do strip (recorte do usuario entre vertices): daqui ate o fim do strip, um
                // desenho por triangulo, na ordem exata do decodificador (n par: os dois primeiros trocados)
                tri_mode_ = true, open_ = -1;
                PvrVertex t[3] = {strip_[n - 3], strip_[n - 2], strip_[n - 1]};
                if (n % 2 == 0) std::swap(t[0], t[1]);
                frame.cl.draws.push_back({s, uint32_t(frame.cl.vertices.size()), 3});
                for (const PvrVertex& x : t) frame.cl.vertices.push_back(render_capture::vertex(x));
            }
        }
        if (end) reset_strip();
    }

    void strip_break() override { reset_strip(); }

    void sprite(const PvrTri& state, const PvrVertex& a, const PvrVertex& b, const PvrVertex& c,
                const PvrVertex& d) override {
        // dois desenhos de 3 vertices, (A, B, C) e (A, C, D), na ordem do decodificador: um strip de 4 so os
        // reproduz com os vertices numa ordem ciclica diferente, o que muda as somas da interpolacao
        const nr::DrawState s = state_of(state);
        const PvrVertex* t1[3] = {&a, &b, &c};
        const PvrVertex* t2[3] = {&a, &c, &d};
        for (const PvrVertex* const* t : {t1, t2}) {
            frame.cl.draws.push_back({s, uint32_t(frame.cl.vertices.size()), 3});
            for (int i = 0; i < 3; ++i) frame.cl.vertices.push_back(render_capture::vertex(*t[i]));
        }
        ++frame.sprites;
        open_ = -1;
    }

    void global(const TaParam& g) override {
        // volume modificador: a instrucao (ISP bits 30-29 do global) 0 = poligono do volume, 1 = ultimo poligono
        // (inclusao), 2 = ultimo poligono (exclusao). Os triangulos depois de um global 1/2 fecham o volume; o
        // proximo global (de qualquer lista) comeca outro (revisao Codex 2026-10-21: globais iguais nao se fundem)
        if (mod_closing_) mod_open_ = false;
        mod_closing_ = false;
        if (g.list != 1 && g.list != 3) return;
        mod_instr_   = (g.w[1] >> 29) & 3;
        mod_closing_ = mod_instr_ == 1 || mod_instr_ == 2;
    }

    void modifier(const TaParam&, const TaParam& p) override {
        // vertice de volume modificador (64 bytes): A, B, C em x, y, z nas palavras 1..9
        ++frame.modifier_params;
        if (!mod_open_ || frame.cl.modifiers.back().list != list_type(p.list)) {
            frame.cl.modifiers.push_back({uint32_t(frame.cl.vertices.size()), 0, true, list_type(p.list)});
            ++frame.modifier_volumes;
            mod_open_ = true, mod_cur_closed_ = false;
        }
        if (mod_closing_) {
            frame.cl.modifiers.back().inclusion = mod_instr_ == 1;
            if (!mod_cur_closed_) mod_cur_closed_ = true, ++mod_closed_;
        }
        for (int i = 0; i < 3; ++i) {
            nr::Vertex v;
            v.x = f32(p.w[1 + 3 * i]), v.y = f32(p.w[2 + 3 * i]), v.inv_w = f32(p.w[3 + 3 * i]);
            frame.cl.vertices.push_back(v);
        }
        frame.cl.modifiers.back().triangle_count++;
    }

    void skipped(const TaParam&) override { ++frame.skipped_params; }

    bool render(const PvrFrame&, uint32_t w, uint32_t h, std::vector<uint32_t>& image) override {
        maybe_save_frame_dump(st.renders + 1, frame, legacy_textures());
        if (mode == Mode::Software) return dev_.soft_render(frame, w, h, image);
        if (mode != Mode::Native && mode != Mode::Compare) return false;
        // N7c: a escala so no nativo (o modo comparar compara na resolucao do jogo)
        vkOk_ = dev_.vk_render(frame, mode == Mode::Native ? scale() : 1.f, w, h, mode == Mode::Native ? image : vkImage_);
        if (mode == Mode::Native && !vkOk_) return dev_.soft_render(frame, w, h, image);  // HH15.7a: o recuo
        return mode == Mode::Native && vkOk_;
    }

    bool renders() const override { return mode == Mode::Software || mode == Mode::Native; }

    // HH15.7c: com o estado nativo registrado (set_native_memory), os parametros do quadro, o stride e os texels de cada
    // textura do render lidos do estado do quadro e da memoria das texturas nativos tem de ser os que a captura leu dos
    // registradores e da VRAM do runtime (emprestados com o mesmo estado), e a imagem fica para o comparador da gravacao
    // do framebuffer (kamui_device). Negativos HH_KAMUI_NATIVO_MUTACAO=quadro|textura
    void check_native(std::span<const uint32_t> image, uint32_t w, uint32_t h) {
        const km::FrameState*  fs   = native_frame_state();
        const km::TextureHeap* heap = native_texture_heap();
        if (!fs || !heap) return;
        lastImage_.assign(image.begin(), image.end()), lastW_ = w, lastH_ = h;
        ++st.native_checked;
        auto fail = [&](uint64_t& counter, const std::string& why) {
            ++counter;
            if (st.native_error.empty()) st.native_error = "render " + std::to_string(st.renders) + ": " + why;
        };
        nr::FrameParams nf = km::frame_params(*fs, *heap, frame.cl.frame.width, frame.cl.frame.height);
        if (native_mutation("quadro")) nf.clear_depth += 1.f;
        if (!same_frame(nf, frame.cl.frame) || fs->texture_stride() != stride_)
            fail(st.native_frame_mismatch, "parametros do quadro");
        for (const TextureKey& k : frame.textures) {
            std::vector<uint8_t>  raw, nraw;
            std::vector<uint32_t> argb, nargb;
            bool                  legacy = false;
            texture_source(k, raw);
            km::texture_bytes(*fs, *heap, k, nraw);
            const bool have  = texture_argb(k, argb, legacy);
            const bool nhave = km::convert_texture(*fs, *heap, k, nargb);
            if (native_mutation("textura") && !nargb.empty()) nargb[0] ^= 0x00FFFFFF;
            ++st.native_tex_checked;
            char b[64];
            std::snprintf(b, sizeof b, "textura TCW %08X %ux%u", k.tcw, k.width, k.height);
            if (have && !nhave) fail(st.native_tex_missing, std::string(b) + " sem conversao nativa");
            else if (raw != nraw || argb != nargb) fail(st.native_tex_mismatch, b);
        }
    }

    // HH15.7b: com o ListSink nativo registrado (set_list_sink), as listas que ele juntou decodificadas direto tem de dar
    // exatamente a IR que a captura montou dos retornos do decodificador do legado (em todo render que passou pelo
    // caminho do TA). Negativo HH_LISTA_NATIVA_MUTACAO=vertice: o primeiro vertice do ListSink anda meio pixel
    void check_list_sink() {
        if (!g_list) return;
        km::ListFrame lf;
        g_list->build(frame.cl.frame, stride_, lf);
        static const bool mutate = [] {
            const char* m = std::getenv("HH_LISTA_NATIVA_MUTACAO");
            return m && std::strcmp(m, "vertice") == 0;
        }();
        if (mutate && !lf.cl.vertices.empty()) lf.cl.vertices[0].x += 0.5f;
        ++st.list_checked;
        const nr::CommandList &a = lf.cl, &b = frame.cl;
        std::string            why;
        if (!same_frame(a.frame, b.frame)) why = "quadro";
        else if (a.vertices.size() != b.vertices.size() ||
                 (!a.vertices.empty() && std::memcmp(a.vertices.data(), b.vertices.data(), a.vertices.size() * sizeof(nr::Vertex))))
            why = "vertices (" + std::to_string(a.vertices.size()) + " x " + std::to_string(b.vertices.size()) + ")";
        else if (a.draws.size() != b.draws.size())
            why = "desenhos (" + std::to_string(a.draws.size()) + " x " + std::to_string(b.draws.size()) + ")";
        else if (a.modifiers.size() != b.modifiers.size())
            why = "volumes (" + std::to_string(a.modifiers.size()) + " x " + std::to_string(b.modifiers.size()) + ")";
        else if (lf.textures != frame.textures)
            why = "texturas (" + std::to_string(lf.textures.size()) + " x " + std::to_string(frame.textures.size()) + ")";
        for (size_t i = 0; why.empty() && i < a.draws.size(); ++i)
            if (!(a.draws[i].state == b.draws[i].state) || a.draws[i].first_vertex != b.draws[i].first_vertex ||
                a.draws[i].vertex_count != b.draws[i].vertex_count)
                why = "desenho " + std::to_string(i);
        for (size_t i = 0; why.empty() && i < a.modifiers.size(); ++i) {
            const nr::ModifierVolume &x = a.modifiers[i], &y = b.modifiers[i];
            if (x.first_vertex != y.first_vertex || x.triangle_count != y.triangle_count || x.inclusion != y.inclusion ||
                x.list != y.list)
                why = "volume " + std::to_string(i);
        }
        if (why.empty() && (lf.modifier_params != frame.modifier_params || lf.modifier_volumes != frame.modifier_volumes ||
                            lf.modifier_unclosed != frame.modifier_unclosed || lf.skipped_params != frame.skipped_params ||
                            lf.sprites != frame.sprites || lf.texture_stride != frame.texture_stride ||
                            lf.unsupported != frame.skipped_params))
            why = "contagens do decodificador";
        if (!why.empty()) {
            ++st.list_mismatch;
            if (st.list_error.empty()) st.list_error = "render " + std::to_string(st.renders) + ": " + why;
        }
    }

    void end(const PvrFrame& used, std::span<const PvrTri> drawn, std::span<const uint32_t> image, uint32_t w,
             uint32_t h) override {
        frame.modifier_unclosed = frame.modifier_volumes - mod_closed_;
        st.modifier_unclosed += frame.modifier_unclosed;
        ++st.renders;
        if (mode == Mode::Compare && vkOk_) compare(image, w, h);
        account(frame, st);
        check_list_sink();
        check_native(image, w, h);
        // HH15.7a: em todo render do conferir (nao so nos amostrados), o software sobre a IR (o dispositivo, com as
        // texturas pelas conversoes nativas) desenha a mesma imagem que o rasterizador sobre o TA; uma falha dele e
        // contada a parte (soft_failures), nunca como imagem divergente
        if (mode == Mode::Check) {
            std::vector<uint32_t> dev;
            if (dev_.soft_render(frame, w, h, dev)) {
                ++st.soft_checked;
                if (dev.size() != image.size() || std::memcmp(dev.data(), image.data(), image.size() * 4) != 0) {
                    size_t diff = 0;
                    for (size_t i = 0; i < std::min(dev.size(), image.size()); ++i) diff += dev[i] != image[i];
                    ++st.soft_mismatch;
                    if (st.first_error.empty())
                        st.first_error = "render " + std::to_string(st.renders) + ": imagem do software sobre a IR difere em " +
                                         std::to_string(diff) + " pixels";
                }
            }
        }
        if (mode != Mode::Check || (st.renders - 1) % every) return;
        ++st.checked;
        auto fail = [&](uint64_t& counter, const std::string& what) {
            ++counter;
            if (st.first_error.empty()) st.first_error = "render " + std::to_string(st.renders) + ": " + what;
        };
        std::string err;
        if (!valid(frame, &err)) {
            fail(st.invalid, "IR invalida: " + err);
            return;  // indices nao confiaveis: nada mais e percorrido
        }
        const PvrFrame from_ir = pvr_frame(frame);
        if (!(from_ir == used)) fail(st.frame_mismatch, "FrameParams nao reproduzem os registradores do quadro");
        const std::vector<PvrTri> tris = triangles(frame);
        st.triangles += tris.size();
        if (tris.size() != drawn.size()) {
            fail(st.triangle_mismatch, "IR com " + std::to_string(tris.size()) + " triangulos, rasterizador " +
                                           std::to_string(drawn.size()));
        } else {
            for (size_t i = 0; i < tris.size(); ++i) {
                const PvrTri& a = tris[i];
                const PvrTri& b = drawn[i];
                const bool    vtx = same_vertex(a.v[0], b.v[0]) && same_vertex(a.v[1], b.v[1]) && same_vertex(a.v[2], b.v[2]);
                const bool    est = a.list == b.list && state_of(a) == state_of(b) &&
                                 (!b.tex || key_of(a, stride_) == key_of(b, stride_));
                if (!vtx || !est) {
                    fail(st.triangle_mismatch, "triangulo " + std::to_string(i) + (vtx ? " (estado)" : " (vertices)"));
                    break;
                }
            }
        }
        const std::vector<uint32_t> again = pvr_render_triangles(tris, from_ir, w, h);
        if (again.size() != image.size() || std::memcmp(again.data(), image.data(), image.size() * 4) != 0) {
            size_t diff = 0;
            for (size_t i = 0; i < std::min(again.size(), image.size()); ++i) diff += again[i] != image[i];
            fail(st.image_mismatch, "imagem a partir da IR difere em " + std::to_string(diff) + " pixels");
        }
        for (const TextureKey& k : frame.textures) {
            std::vector<uint32_t> argb;
            if (!convert_texture(k, argb)) {
                ++st.textures_without_native;
                continue;
            }
            ++st.textures_checked;
            const nr::TextureId id = nr::texture_id(k.width, k.height, argb);
            if (keys_.insert({k.tcw, (uint64_t(k.width) << 32) | k.height}).second) ++st.distinct_texture_keys;
            if (ids_.insert({id.content, (uint64_t(id.width) << 32) | id.height}).second) ++st.distinct_texture_ids;
            const TexInfo t{k.tcw, k.width, k.height, k.stride};
            for (uint32_t y = 0, bad = 0; y < k.height && !bad; ++y)
                for (uint32_t x = 0; x < k.width; ++x)
                    if (argb[size_t(y) * k.width + x] != pvr_texel(t, x, y)) {
                        char b[96];
                        std::snprintf(b, sizeof b, "textura TCW %08X %ux%u: texel (%u,%u)", k.tcw, k.width, k.height, x, y);
                        fail(st.texture_mismatch, b);
                        bad = 1;
                        break;
                    }
        }
    }

private:
    std::vector<PvrVertex> strip_;
    int32_t                open_     = -1;
    bool                   tri_mode_ = false;
    uint32_t               stride_   = 0;
    bool                   mod_open_ = false, mod_closing_ = false, mod_cur_closed_ = false;
    uint32_t               mod_instr_ = 0, mod_closed_ = 0;  // instrucao do ultimo global; volumes com ultimo poligono
    std::set<std::pair<uint64_t, uint64_t>> keys_, ids_;  // para o resumo: quantas chaves e quantos conteudos

    // ---- os dispositivos sobre a IR (render_devices.h), com os texels do runtime ----
    Devices               dev_{st, legacy_textures()};
    bool                  vkOk_ = false;
    std::vector<uint32_t> vkImage_;
    std::vector<uint32_t> lastImage_;  // HH15.7c: a imagem do ultimo render com o estado nativo registrado
    uint32_t              lastW_ = 0, lastH_ = 0;

public:
    const std::vector<uint32_t>* last_image(uint32_t& w, uint32_t& h) const {
        if (lastImage_.empty()) return nullptr;
        w = lastW_, h = lastH_;
        return &lastImage_;
    }

    std::string dumpDir;
    uint32_t    dumpEvery = 300;

private:
    void compare(std::span<const uint32_t> sw, uint32_t w, uint32_t h) {
        if (vkImage_.size() != sw.size()) return;
        uint64_t diff = 0, diff8 = 0;
        uint32_t mx   = 0;
        double   se   = 0;  // erro quadratico (RGB) para o PSNR
        for (size_t i = 0; i < sw.size(); ++i) {
            if (sw[i] == vkImage_[i]) continue;
            ++diff;
            uint32_t m = 0;
            for (int sh = 0; sh < 32; sh += 8) {
                const int d = int((sw[i] >> sh) & 0xFF) - int((vkImage_[i] >> sh) & 0xFF);
                m = std::max<uint32_t>(m, uint32_t(std::abs(d)));
                if (sh < 24) se += double(d) * d;
            }
            diff8 += m > 8, mx = std::max(mx, m);
        }
        const double mse  = se / (3.0 * double(sw.size()));
        const double psnr = mse > 0 ? 10.0 * std::log10(255.0 * 255.0 / mse) : 99.0;
        st.cmp_min_psnr   = st.cmp_frames ? std::min(st.cmp_min_psnr, psnr) : psnr;
        st.cmp_psnr_sum += psnr, st.cmp_below40 += psnr < 40.0;
        ++st.cmp_frames, st.cmp_pixels += sw.size(), st.cmp_diff += diff, st.cmp_diff8 += diff8;
        st.cmp_max = std::max(st.cmp_max, mx);
        const double frac = double(diff8) / double(sw.size());
        if (frac > st.cmp_worst_fraction) st.cmp_worst_fraction = frac, st.cmp_worst_render = st.renders;
        static const bool anyDiff = std::getenv("HH_RENDER_IR_BMP_DIFF") != nullptr;  // qualquer pixel diferente
        if (!dumpDir.empty() && (st.renders % dumpEvery == 0 || frac > 0.01 || psnr < 40.0 || (anyDiff && diff)))
            dump(sw, w, h, frac);
    }

    void dump(std::span<const uint32_t> sw, uint32_t w, uint32_t h, double frac) {
        if (dumped_ >= 200) return;  // limite de arquivos
        ++dumped_;
        char name[96];
        std::snprintf(name, sizeof name, "render_%06llu_%05.2f.bmp", static_cast<unsigned long long>(st.renders), frac * 100);
        const uint32_t W = w * 3, row = (W * 3 + 3) & ~3u;
        std::vector<uint8_t> px(size_t(row) * h);
        for (uint32_t y = 0; y < h; ++y)
            for (uint32_t x = 0; x < w; ++x) {
                const uint32_t a = sw[size_t(y) * w + x], b = vkImage_[size_t(y) * w + x];
                uint8_t* r = px.data() + size_t(h - 1 - y) * row;
                for (int c = 0; c < 3; ++c) {
                    const int sh = 8 * c;  // B, G, R
                    r[(x) * 3 + c]         = uint8_t((a >> sh) & 0xFF);
                    r[(w + x) * 3 + c]     = uint8_t((b >> sh) & 0xFF);
                    r[(2 * w + x) * 3 + c] = uint8_t(std::min(255, 8 * std::abs(int((a >> sh) & 0xFF) - int((b >> sh) & 0xFF))));
                }
            }
        std::filesystem::create_directories(dumpDir);
        std::ofstream f(std::filesystem::path(dumpDir) / name, std::ios::binary);
        auto u32 = [&](uint32_t v) { f.write(reinterpret_cast<const char*>(&v), 4); };
        auto u16 = [&](uint16_t v) { f.write(reinterpret_cast<const char*>(&v), 2); };
        f.write("BM", 2), u32(54 + uint32_t(px.size())), u32(0), u32(54);
        u32(40), u32(W), u32(h), u16(1), u16(24), u32(0), u32(uint32_t(px.size())), u32(2835), u32(2835), u32(0), u32(0);
        f.write(reinterpret_cast<const char*>(px.data()), std::streamsize(px.size()));
    }
    uint32_t dumped_ = 0;

    void reset_strip() {
        strip_.clear();
        open_ = -1, tri_mode_ = false;
    }

    nr::DrawState state_of(const PvrTri& t) {
        uint32_t handle = 0;
        if (t.tex) {
            const TextureKey k = key_of(t, stride_);
            size_t           i = 0;
            while (i < frame.textures.size() && !(frame.textures[i] == k)) ++i;
            if (i == frame.textures.size()) frame.textures.push_back(k);
            handle = uint32_t(i + 1);
        }
        return draw_state(t, handle);
    }
};

std::unique_ptr<Capture> g_cap;

} // namespace

nr::DrawState draw_state(const PvrTri& t, uint32_t texture_handle) {
    return km::draw_state(t, texture_handle);  // HH15.7b: as regras ficam no ListSink
}

PvrTri pvr_state(const nr::DrawState& s, const TextureKey* tex) {
    PvrTri t;
    t.list = list_number(s.list);
    t.isp  = (uint32_t(s.depth) << 29) | (uint32_t(s.cull) << 27) | (uint32_t(!s.depth_write) << 26);
    const uint32_t flt = uint32_t(s.filter);
    uint32_t       env = 0;
    switch (s.env) {
    case nr::TexEnv::Replace: env = 0; break;
    case nr::TexEnv::Modulate: env = 1; break;
    case nr::TexEnv::Decal: env = 2; break;
    case nr::TexEnv::ModulateAlpha: env = 3; break;
    }
    t.tsp = (uint32_t(s.src) << 29) | (uint32_t(s.dst) << 26) | (uint32_t(s.fog) << 22) | (uint32_t(s.color_clamp) << 21) |
            (uint32_t(s.use_alpha) << 20) | (uint32_t(s.ignore_tex_alpha) << 19) |
            (uint32_t(s.wrap_u == nr::Wrap::Mirror) << 18) | (uint32_t(s.wrap_v == nr::Wrap::Mirror) << 17) |
            (uint32_t(s.wrap_u == nr::Wrap::Clamp) << 16) | (uint32_t(s.wrap_v == nr::Wrap::Clamp) << 15) | (flt << 13) |
            (uint32_t(s.src_select) << 25) | (uint32_t(s.dst_select) << 24) | (uint32_t(s.supersample) << 12) |
            (uint32_t(s.mip_bias & 0xF) << 8) | (env << 6);
    t.tex = bool(s.texture) && tex;
    if (t.tex) t.tcw = tex->tcw, t.tsp |= (log2_size(tex->width) << 3) | log2_size(tex->height);
    t.offset   = s.offset_color;
    t.gouraud  = s.gouraud;
    t.clipMode = s.clip == nr::Clip::Inside ? 2 : (s.clip == nr::Clip::Outside ? 3 : 0);
    if (s.clip != nr::Clip::None) t.cx0 = s.clip_x0, t.cy0 = s.clip_y0, t.cx1 = s.clip_x1, t.cy1 = s.clip_y1;
    return t;
}

nr::Vertex vertex(const PvrVertex& v) { return km::ir_vertex(v); }

PvrVertex pvr_vertex(const nr::Vertex& v) {
    PvrVertex o;
    o.x = v.x, o.y = v.y, o.z = v.inv_w, o.u = v.u, o.v = v.v;
    o.col = unpack(v.color), o.off = unpack(v.offset);
    return o;
}

nr::FrameParams frame_params(uint32_t width, uint32_t height) {
    nr::FrameParams f;
    f.width = width, f.height = height;
    f.clear_color = 0xFF000000 | pvr_background_color();
    f.clear_depth = f32(pvr_reg(R(0x088)));  // ISP_BACKGND_D
    for (size_t i = 0; i < f.fog_table.size(); ++i) f.fog_table[i] = uint16_t(pvr_reg(R(0x200 + uint32_t(i) * 4)) & 0xFFFF);
    f.fog_color             = pvr_reg(R(0x0B0));  // FOG_COL_RAM
    f.fog_vertex_color      = pvr_reg(R(0x0B4));  // FOG_COL_VERT
    f.fog_density           = pvr_reg(R(0x0B8)) & 0xFFFF;  // FOG_DENSITY
    f.punch_through_ref     = float(pvr_reg(R(0x11C)) & 0xFF) / 255.f;
    f.translucent_autosort  = !(pvr_reg(R(0x098)) & 1);  // ISP_FEED_CFG bit 0
    const uint32_t half     = pvr_reg(R(0x080));           // HALF_OFFSET
    f.sample_center         = half & 1, f.attribute_center = half & 2, f.texel_center = half & 4;
    f.small_cull            = f32(pvr_reg(R(0x078)));      // FPU_CULL_VAL
    f.clamp_max             = pvr_reg(R(0x0BC)), f.clamp_min = pvr_reg(R(0x0C0));
    return f;
}

PvrFrame pvr_frame(const Frame& f) {
    const nr::FrameParams& p = f.cl.frame;
    PvrFrame               r;
    r.background       = p.clear_color & 0xFFFFFF;
    r.background_depth = p.clear_depth;
    r.cull_val         = p.small_cull;
    r.tex_stride       = f.texture_stride;
    r.pt_ref           = uint32_t(std::lround(p.punch_through_ref * 255));  // byte / 255.f: a volta e exata
    r.half_offset      = (p.sample_center ? 1u : 0u) | (p.attribute_center ? 2u : 0u) | (p.texel_center ? 4u : 0u);
    r.clamp_max = p.clamp_max, r.clamp_min = p.clamp_min;
    r.autosort  = p.translucent_autosort;
    return r;
}

bool valid(const Frame& f, std::string* error) {
    if (!nr::validate(f.cl, error)) return false;
    for (size_t i = 0; i < f.cl.draws.size(); ++i)
        if (f.cl.draws[i].state.texture.value > f.textures.size()) {
            if (error) *error = "desenho " + std::to_string(i) + ": textura fora da tabela do quadro";
            return false;
        }
    return true;
}

std::vector<PvrTri> triangles(const Frame& f) {
    std::vector<PvrTri> out;
    for (const nr::Draw& d : f.cl.draws) {
        const TextureKey* tex = d.state.texture ? &f.textures[d.state.texture.value - 1] : nullptr;
        const PvrTri      base = pvr_state(d.state, tex);
        for (uint32_t k = 0; k + 3 <= d.vertex_count; ++k) {
            PvrTri t = base;
            for (uint32_t i = 0; i < 3; ++i) t.v[i] = pvr_vertex(f.cl.vertices[d.first_vertex + k + i]);
            if (k & 1) std::swap(t.v[0], t.v[1]);  // strip: os triangulos alternam o sentido
            out.push_back(t);
        }
    }
    return out;
}

size_t linear_texture_texels(uint32_t width, uint32_t height, uint32_t stride) {
    if (!width || !height) return 0;
    const size_t row = stride ? stride : width;
    return (size_t(height) - 1) * row + std::max<size_t>(row, width);
}

bool convert_texture(const TextureKey& k, std::vector<uint32_t>& argb) {
    const uint32_t fmt = (k.tcw >> 27) & 7, base = (k.tcw & 0x1FFFFF) << 3;
    if (!k.width || !k.height) return false;
    const bool     mip = k.tcw >> 31, vq = (k.tcw >> 30) & 1, linear = (k.tcw >> 26) & 1, strideSel = (k.tcw >> 25) & 1;
    const uint32_t w = k.width, h = k.height;
    auto           read = [&](uint32_t n) {
        std::vector<uint8_t> b(n);
        pvr_texture_read(base, b.data(), n);
        return b;
    };
    if (fmt <= 2) {
        const auto f = nr::Texel16(fmt);
        if (vq) {
            const auto d = read(2048 + (mip ? pvr_mip_offset(fmt, true, w) : 0) + (w / 2) * (h / 2));
            return nr::convert_vq(d, w, h, f, mip, 256, argb);
        }
        if (mip) {
            const auto                  d = read(pvr_mip_offset(fmt, false, w) + 2 * w * w);
            std::vector<nr::MipLevel>   levels;
            if (!nr::convert16_mips(d, w, f, levels) || levels.empty()) return false;
            argb = std::move(levels[0].argb);
            return true;
        }
        const uint32_t rowW = linear && strideSel && k.stride ? k.stride : w;  // linear com stride select
        // a ultima linha vai ate x = w - 1 mesmo com stride menor que a largura (como o legado, que le a VRAM direto):
        // linear_texture_texels (HH13: com rowW * h a leitura passava do fim do vetor)
        const auto     d    = read(uint32_t(2 * (linear ? linear_texture_texels(w, h, rowW) : size_t(w) * h)));
        std::vector<uint16_t> px(size_t(w) * h);
        for (uint32_t y = 0; y < h; ++y)
            for (uint32_t x = 0; x < w; ++x) {
                const size_t from = linear ? size_t(y) * rowW + x : size_t(y) * w + x;  // twiddled: o vetor inteiro
                std::memcpy(&px[size_t(y) * w + x], d.data() + 2 * from, 2);
            }
        return nr::convert16(px, w, h, f, !linear, argb);
    }
    if (fmt == 5 && !mip) {  // paleta de 4 bits: o banco do seletor (TCW bits 26-21)
        std::vector<uint32_t> pal(16);
        const uint32_t        sel = (k.tcw >> 21) & 0x3F;
        for (uint32_t i = 0; i < 16; ++i) pal[i] = pvr_pal_entry(sel * 16 + i);
        return nr::convert_pal4(read(w * h / 2), w, h, pal, argb);
    }
    return false;  // paleta de 8 bits, YUV, bump: sem conversao nativa (o jogo nao os usa nos arquivos)
}

void texture_source(const TextureKey& k, std::vector<uint8_t>& raw) {
    const uint32_t fmt = (k.tcw >> 27) & 7, base = (k.tcw & 0x1FFFFF) << 3;
    const bool     mip = k.tcw >> 31, vq = (k.tcw >> 30) & 1, linear = (k.tcw >> 26) & 1, strideSel = (k.tcw >> 25) & 1;
    const uint32_t w = k.width, h = k.height;
    const uint32_t bits = fmt == 5 ? 4 : (fmt == 6 ? 8 : 16);  // paleta de 4 e de 8 bits; o resto, 16
    uint32_t       n;
    if (vq) n = 2048 + (mip ? pvr_mip_offset(fmt, true, w) : 0) + (w / 2) * (h / 2);
    // linear com stride so nos formatos de 16 bits sem mipmap (paleta: sempre twiddled, os bits 26/25 sao o seletor; o
    // legado confere o mipmap antes do modo linear): o mesmo alcance de convert_texture
    else if (fmt <= 4 && !mip && linear && strideSel && k.stride) n = uint32_t(2 * linear_texture_texels(w, h, k.stride));
    else if (mip) n = pvr_mip_offset(fmt, false, w) + bits * w * w / 8;
    else n = bits * w * h / 8;
    raw.resize(n);
    pvr_texture_read(base, raw.data(), n);
    if (fmt == 5 || fmt == 6) {  // o banco de paleta que o seletor escolhe
        const uint32_t first = fmt == 5 ? ((k.tcw >> 21) & 0x3F) * 16 : ((k.tcw >> 25) & 3) * 256, count = fmt == 5 ? 16 : 256;
        for (uint32_t i = 0; i < count; ++i) {
            const uint32_t e = pvr_pal_entry(first + i);
            raw.insert(raw.end(), reinterpret_cast<const uint8_t*>(&e), reinterpret_cast<const uint8_t*>(&e) + 4);
        }
    }
}

bool texture_argb(const TextureKey& k, std::vector<uint32_t>& argb, bool& legacy) {
    legacy = false;
    if (convert_texture(k, argb)) return true;
    // sem conversao nativa (paleta de 8 bits, YUV, bump): o decodificador do legado, o mesmo que o software amostra
    // (so chegam aqui formatos que ele aceita: os outros nem viram desenho)
    const TexInfo t{k.tcw, k.width, k.height, k.stride};
    argb.resize(size_t(k.width) * k.height);
    for (uint32_t y = 0; y < k.height; ++y)
        for (uint32_t x = 0; x < k.width; ++x) argb[size_t(y) * k.width + x] = pvr_texel(t, x, y);
    legacy = true;
    return true;
}

bool content_id(const TextureKey& k, nr::TextureId& id) {
    std::vector<uint32_t> argb;
    if (!convert_texture(k, argb)) return false;
    id = nr::texture_id(k.width, k.height, argb);
    return true;
}

TextureSource& legacy_textures() {
    static LegacyTextures src;
    return src;
}

bool save_frame_dump(const std::string& path, const Frame& f) { return save_frame_dump(path, f, legacy_textures()); }

void set_list_sink(const native::render::kamui::ListSink* sink) { g_list = sink; }

const std::vector<uint32_t>* last_image(uint32_t& width, uint32_t& height) {
    return g_cap ? g_cap->last_image(width, height) : nullptr;
}

void set_compare_dump(const std::string& dir, uint32_t every) {
    if (!g_cap) g_cap = std::make_unique<Capture>();
    g_cap->dumpDir = dir, g_cap->dumpEvery = every ? every : 300;
}

void install(Mode m, uint32_t every) {
    if (m == Mode::Off) {  // desinstala e descarta (estatisticas, quadro, dispositivo Vulkan)
        pvr_render_set_capture(nullptr);
        g_cap.reset();
        return;
    }
    std::string dir = g_cap ? g_cap->dumpDir : std::string();
    uint32_t    dumpEvery = g_cap ? g_cap->dumpEvery : 300;
    g_cap        = std::make_unique<Capture>();  // estatisticas, quadro e conjuntos de ids recomecam
    g_cap->dumpDir = dir, g_cap->dumpEvery = dumpEvery;
    g_cap->mode  = m;
    g_cap->every = every ? every : 1;
    pvr_render_set_capture(g_cap.get());
}

const Frame& last_frame() {
    static const Frame empty;
    return g_cap ? g_cap->frame : empty;
}

const Stats& stats() {
    static const Stats empty;
    return g_cap ? g_cap->st : empty;
}

std::string summary() { return g_cap ? format_summary(g_cap->st) : std::string(); }

} // namespace hh::transitional::render_capture
