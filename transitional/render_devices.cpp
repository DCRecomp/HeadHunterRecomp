// SPDX-License-Identifier: MIT
// Derivado de recomp-BStinger transitional/render_capture.cpp (MIT, Copyright (c) 2026 Blue Stinger
// Recompiled contributors; ver third_party/licenses/recomp-BStinger-LICENSE.txt).
// HH15.7d4b: os dispositivos sobre a IR e o renderizador do produto (render_devices.h), movidos da captura sem mudanca.
#include "render_devices.h"

#include "render/kamui/texture_source.h"
#include "render/render_ir.h"
#include "render/soft/soft_device.h"
#include "render/vulkan/vk_device.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <utility>

namespace hh::transitional::render_capture {
namespace {

namespace km = hh::native::render::kamui;

nr::ITextureReplacements* g_replacements = nullptr;  // texturas HD (set_texture_replacements)
Fault                     g_fault        = Fault::None;     // testes do recuo (inject_fault)
float                     g_scale        = 1.f;             // resolucao interna (set_scale)
const km::FrameState*     g_fs           = nullptr;         // HH15.7c: o estado do quadro nativo (set_native_memory)
const km::TextureHeap*    g_heap         = nullptr;         // HH15.7c: a memoria das texturas nativa
Devices*                  g_vkLast       = nullptr;         // os dispositivos que desenharam por ultimo no Vulkan

// HH15.7c: os texels pela memoria nativa (sem o decodificador do legado: uma textura sem conversao nativa nao e enviada
// e o render falha)
class NativeTextures final : public TextureSource {
public:
    void bytes(const TextureKey& k, std::vector<uint8_t>& raw) override {
        if (g_fs && g_heap) km::texture_bytes(*g_fs, *g_heap, k, raw);
        else raw.clear();
    }
    bool argb(const TextureKey& k, std::vector<uint32_t>& argb, bool& legacy) override {
        legacy = false;
        return g_fs && g_heap && km::convert_texture(*g_fs, *g_heap, k, argb);
    }
};

} // namespace

void account(const Frame& f, Stats& st) {
    st.draws += f.cl.draws.size(), st.vertices += f.cl.vertices.size(), st.sprites += f.sprites;
    st.frame_textures += f.textures.size();  // HH15.7c: o total, independente do comparador (o portao confere)
    st.modifier_params += f.modifier_params, st.modifier_volumes += f.modifier_volumes;
    st.skipped_params += f.skipped_params;
    for (const nr::Draw& d : f.cl.draws) {
        const size_t fog = size_t(d.state.fog);
        if (fog < st.fog_draws.size()) {
            ++st.fog_draws[fog];
            if (!st.fog_first_render[fog]) st.fog_first_render[fog] = st.renders;
        }
    }
    if (!f.cl.modifiers.empty() && !st.modifier_first_render) st.modifier_first_render = st.renders;
}

TextureSource& native_textures() {
    static NativeTextures src;
    return src;
}

void set_native_memory(const native::render::kamui::FrameState* fs, const native::render::kamui::TextureHeap* heap) {
    g_fs = fs, g_heap = heap;
}
const native::render::kamui::FrameState*  native_frame_state() { return g_fs; }
const native::render::kamui::TextureHeap* native_texture_heap() { return g_heap; }

void set_texture_replacements(nr::ITextureReplacements* provider) { g_replacements = provider; }

void  set_scale(float s) { g_scale = s > 0 ? s : 1.f; }
float scale() { return g_scale; }

void inject_fault(Fault f) { g_fault = f; }

// ---- Devices ----

Devices::Devices(Stats& s, TextureSource& src) : st(s), src_(src), soft_(std::make_unique<nr::soft::SoftwareDevice>()) {}

Devices::~Devices() {
    drop_vk(false);
    if (g_vkLast == this) g_vkLast = nullptr;
}

HiresImage Devices::hires_for(std::span<const uint32_t> shown, uint32_t w, uint32_t h) {
    if (hires_.empty()) return {};
    if (g_scale == 1.f) {
        hires_.clear();
        return {};
    }
    // o video pode mostrar so parte das linhas do render (entrelacado: 640 x 476 de um render de 640 x 480): procura a
    // linha do render em que a imagem exibida comeca (ate 8 de deslocamento)
    for (const Hires& e : hires_) {
        if (e.w != w || h > e.h || shown.size() != size_t(w) * h) continue;
        for (uint32_t dy = 0; dy <= std::min<uint32_t>(8, e.h - h); ++dy) {
            // amostra de 1 pixel em 4 nas duas direcoes; diferenca por canal <= 16 (565/555/4444 perdem ate 4 bits)
            uint32_t n = 0, bad = 0;
            for (uint32_t y = 1; y < h && bad * 1000 <= n + 1000; y += 4)
                for (uint32_t x = 1; x < w; x += 4, ++n) {
                    const uint32_t a = shown[size_t(y) * w + x], b = e.down[size_t(y + dy) * w + x];
                    for (int s = 0; s < 24; s += 8)
                        if (std::abs(int((a >> s) & 0xFF) - int((b >> s) & 0xFF)) > 16) {
                            ++bad;
                            break;
                        }
                }
            if (n && bad * 1000 <= n) {  // ate 0,1 % fora (bordas de recorte do framebuffer)
                ++st.hires_shown;
                HiresImage r{e.seq, e.iw, e.ih};
                // as linhas da imagem maior que correspondem as exibidas
                r.y0 = uint32_t(std::lround(double(dy) * e.ih / e.h)), r.rows = uint32_t(std::lround(double(h) * e.ih / e.h));
                r.rows = std::min(r.rows, e.ih - r.y0);
                return r;
            }
        }
    }
    ++st.hires_missed;
    return {};
}

bool Devices::read_hires(const HiresImage& h, std::vector<uint32_t>& argb, uint32_t& w) {
    return vk_ && h.seq && vk_->read_slot(h.seq, h.y0, h.rows, argb, w);
}

bool Devices::ensure_vk() {
    if (vk_) return true;
    if (vkTried_) return false;
    vkTried_ = true;
    vk_      = nr::vulkan_shared(&st.vk_error);
    if (!vk_) {
        std::fprintf(stderr, "[render] Vulkan indisponivel (%s): renderizador em software\n", st.vk_error.c_str());
        return false;
    }
    st.vk_device = vk_->stats().device;
    const auto& vs = vk_->stats();
    // garantido bit a bit com arredondamento ao par (RTE) e profundidade crua; os subnormais (abaixo de 1,2e-38) ficam
    // no padrao do dispositivo quando ele nao garante preserva-los
    std::fprintf(stderr,
                 "[render] Vulkan: %s (modo fiel bit a bit %s: arredondamento ao par %s, subnormais preservados %s, "
                 "profundidade crua %s)\n",
                 st.vk_device.c_str(), vs.float_controls && vs.raw_depth ? "garantido" : "NAO garantido",
                 vs.float_controls ? "sim" : "nao", vs.denorm_preserve ? "sim" : "padrao do dispositivo",
                 vs.raw_depth ? "sim" : "nao");
    return true;
}

// handle do dispositivo para a textura: refeita quando os bytes dela (ou a paleta) mudam
bool Devices::texture_handle(const TextureKey& k, nr::TextureHandle& out) {
    if (g_fault == Fault::Texture) {
        g_fault = Fault::None;
        return false;
    }
    src_.bytes(k, raw_);
    VkTex& e = vkTex_[{k.tcw, k.width, k.height, k.stride}];
    e.used   = st.renders;
    if (e.handle && e.raw == raw_ && e.provider == g_replacements) {
        out = e.handle;
        return true;
    }
    bool legacy = false;
    if (!src_.argb(k, argb_, legacy)) return false;
    st.tex_legacy += legacy;
    // substituicao HD pelo conteudo (nunca pelo endereco): compativel ou o original
    const std::vector<uint32_t>* texels = &argb_;
    uint32_t                     tw = k.width, th = k.height;
    if (g_replacements) {
        const nr::TextureId id = nr::texture_id(k.width, k.height, argb_);
        if (const nr::ReplacementTexture* r = g_replacements->find(id)) {
            if (nr::replacement_compatible(id, *r)) texels = &r->argb, tw = r->width, th = r->height, ++st.tex_replaced;
            else ++st.tex_replace_rejected;
        }
    }
    const auto bytes = std::as_bytes(std::span<const uint32_t>(*texels));
    if (e.handle && (e.w != tw || e.h != th)) vk_->destroy_texture(e.handle), e.handle = {};
    if (e.handle) {
        if (!vk_->update_texture(e.handle, bytes)) return false;
    } else {
        nr::TextureDesc d;
        d.width = tw, d.height = th;
        auto r = vk_->create_texture(d, bytes);
        if (!r) return false;
        e.handle = r.value();
    }
    e.w = tw, e.h = th, e.provider = g_replacements;
    e.raw = raw_;
    ++st.tex_uploads;
    out = e.handle;
    return true;
}

void Devices::evict() {  // texturas sem uso ha 600 renders saem da GPU
    for (auto it = vkTex_.begin(); it != vkTex_.end();)
        if (st.renders - it->second.used > 600) {
            if (it->second.handle) vk_->destroy_texture(it->second.handle);
            it = vkTex_.erase(it), ++st.tex_evicted;
        } else {
            ++it;
        }
}

// Perda do dispositivo (VK_ERROR_DEVICE_LOST e falhas do quadro): o dispositivo e recriado a partir dos dados da
// CPU (as texturas voltam do cache) e o MESMO render e refeito; ate kRecoveries por sessao. Falhou de novo:
// software ate o fim da sessao (docs/N7-PLANO.md secao 3).
bool Devices::vk_render(Frame& f, float sc, uint32_t w, uint32_t h, std::vector<uint32_t>& out) {
    std::string why;
    if (vk_attempt(f, sc, w, h, out, &why)) return true;
    if (!vk_ && vkTried_ && why.empty()) return false;  // sem Vulkan desde o inicio
    ++st.vk_failures;
    if (why == "textura") return false;  // so este render no software (o dispositivo continua)
    if (st.vk_recoveries < kRecoveries) {
        ++st.vk_recoveries;
        std::fprintf(stderr, "[render] Vulkan: falha no render %llu (%s); recriando o dispositivo (%u de %u)\n",
                     static_cast<unsigned long long>(st.renders + 1), why.c_str(), unsigned(st.vk_recoveries), kRecoveries);
        drop_vk(true);
        vkTried_ = false;  // ensure_vk cria de novo
        std::string again;
        if (vk_attempt(f, sc, w, h, out, &again)) return true;
        ++st.vk_failures;
        why = again.empty() ? "recriacao" : again;
    }
    st.vk_error = "falha do dispositivo Vulkan no render " + std::to_string(st.renders + 1) + " (" + why +
                  "): renderizador em software ate o fim da sessao";
    std::fprintf(stderr, "[render] %s\n", st.vk_error.c_str());
    drop_vk(true);
    vkTried_ = true;  // nao tenta mais
    return false;
}

// lost = o dispositivo se perdeu: o compartilhado e descartado (o proximo e outro); senao as texturas deste uso saem
// dele (a apresentacao pode continuar com ele)
void Devices::drop_vk(bool lost) {
    if (vk_ && !lost) {
        vk_->set_scale(1.f);
        for (auto& [k, t] : vkTex_)
            if (t.handle) vk_->destroy_texture(t.handle);
    }
    if (vk_ && lost) nr::vulkan_shared_lost(vk_.get());
    vkTex_.clear();
    hires_.clear();
    vk_.reset();
}

bool Devices::vk_attempt(Frame& f, float sc, uint32_t w, uint32_t h, std::vector<uint32_t>& out, std::string* why) {
    if (!ensure_vk()) return false;
    if (st.renders % 300 == 0) evict();
    std::vector<nr::TextureHandle> handles(f.textures.size());
    for (size_t i = 0; i < f.textures.size(); ++i)
        if (!texture_handle(f.textures[i], handles[i])) {
            // sem a textura o quadro sairia errado: este render fica no software (nunca um desenho sem textura)
            if (st.vk_error.empty()) st.vk_error = "textura nao enviada ao Vulkan (render no software)";
            *why = "textura";
            return false;
        }
    const std::vector<nr::Draw> saved = f.cl.draws;  // a IR do quadro fica com os handles do quadro
    for (nr::Draw& d : f.cl.draws)
        if (d.state.texture) d.state.texture = handles[d.state.texture.value - 1];
    uint32_t rw = 0, rh = 0;
    // N7c: a escala so no nativo (o modo comparar compara na resolucao do jogo)
    vk_->set_scale(sc);
    if (sc == 1.f) hires_.clear();
    // testes: Device falha esta tentativa e a da recuperacao; DeviceOnce so esta
    const bool injected = g_fault == Fault::Device || g_fault == Fault::DeviceOnce || g_fault == Fault::DeviceAgain;
    if (g_fault == Fault::Device) g_fault = Fault::DeviceAgain;
    else if (injected) g_fault = Fault::None;
    const bool submitted = !injected && vk_->submit(f.cl).ok();
    const auto t1        = std::chrono::steady_clock::now();
    // a leitura de volta e sempre na resolucao do jogo (com a escala, a reducao feita na GPU)
    const bool ok = submitted && vk_->read_frame(out, rw, rh).ok() && rw == w && rh == h;
    f.cl.draws = saved;
    uint32_t       iw = 0, ih = 0;
    const uint64_t seq = ok && sc != 1.f ? vk_->last_slot(iw, ih) : 0;
    if (ok && sc != 1.f) {
        ++st.hires_renders, st.hires_w = iw, st.hires_h = ih;
        st.hires_cpu_ms_total += std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - t1).count();
    }
    if (seq) {  // a imagem maior ficou num slot da GPU (para a apresentacao direta)
        Hires e;
        e.down = out, e.seq = seq, e.w = w, e.h = h, e.iw = iw, e.ih = ih;
        hires_.push_front(std::move(e));
        if (hires_.size() > 3) hires_.pop_back();
    }
    if (ok) {
        g_vkLast = this;
        ++st.vk_frames;
        st.vk_fragment_retries = vk_->stats().fragment_retries, st.vk_negative_depth = vk_->stats().negative_depth_frames;
        st.vk_gpu_ms_total = vk_->stats().gpu_ms_total, st.vk_gpu_ms_max = vk_->stats().gpu_ms_max;
        st.vk_max_layers   = vk_->stats().max_layers;
        return true;
    }
    *why = injected                           ? std::string("falha injetada")
           : !vk_->stats().last_error.empty() ? vk_->stats().last_error
           : (rw != w || rh != h)             ? "tamanho " + std::to_string(rw) + "x" + std::to_string(rh) + " em vez de " +
                                                    std::to_string(w) + "x" + std::to_string(h)
                                              : std::string("leitura do quadro");
    return false;
}

// a textura no dispositivo em software, refeita quando os bytes dela (ou a paleta) mudam
bool Devices::soft_texture(const TextureKey& k, nr::TextureHandle& out) {
    src_.bytes(k, raw_);
    SoftTex& e = softTex_[{k.tcw, k.width, k.height, k.stride}];
    e.used     = st.renders;
    if (e.handle && e.raw == raw_) {
        out = e.handle;
        return true;
    }
    bool legacy = false;
    if (!src_.argb(k, argb_, legacy)) return false;
    st.soft_tex_legacy += legacy;
    // negativo do comparador (HH_RENDER_SOFT_MUTACAO=textura): o primeiro texel de cada textura enviada muda
    static const bool mutate = [] {
        const char* m = std::getenv("HH_RENDER_SOFT_MUTACAO");
        return m && std::strcmp(m, "textura") == 0;
    }();
    if (mutate && !argb_.empty()) argb_[0] ^= 0x00FFFFFF;
    const auto bytes = std::as_bytes(std::span<const uint32_t>(argb_));
    if (e.handle) {
        if (!soft_->update_texture(e.handle, bytes)) return false;
    } else {
        nr::TextureDesc d;
        d.width = k.width, d.height = k.height;
        auto r = soft_->create_texture(d, bytes);
        if (!r) return false;
        e.handle = r.value();
    }
    e.raw = raw_;
    ++st.soft_tex_uploads;
    out = e.handle;
    return true;
}

// a IR do render no software; false (com o motivo) deixa o render para o chamador
bool Devices::soft_render(Frame& f, uint32_t w, uint32_t h, std::vector<uint32_t>& out) {
    if (st.renders % 300 == 0)  // texturas sem uso ha 600 renders saem do dispositivo
        for (auto it = softTex_.begin(); it != softTex_.end();)
            if (st.renders - it->second.used > 600) {
                if (it->second.handle) soft_->destroy_texture(it->second.handle);
                it = softTex_.erase(it), ++st.soft_tex_evicted;
            } else {
                ++it;
            }
    auto fail = [&](const std::string& why) {
        ++st.soft_failures;
        if (st.soft_error.empty()) {
            st.soft_error = "render " + std::to_string(st.renders + 1) + ": " + why;
            std::fprintf(stderr, "[render] software sobre a IR falhou (%s)\n", st.soft_error.c_str());
        }
        return false;
    };
    if (g_fault == Fault::Soft) {
        g_fault = Fault::None;
        return fail("falha injetada");
    }
    std::vector<nr::TextureHandle> handles(f.textures.size());
    for (size_t i = 0; i < f.textures.size(); ++i)
        if (!soft_texture(f.textures[i], handles[i])) return fail("textura sem conversao");
    const std::vector<nr::Draw> saved = f.cl.draws;  // a IR do quadro fica com os handles do quadro
    for (nr::Draw& d : f.cl.draws)
        if (d.state.texture) d.state.texture = handles[d.state.texture.value - 1];
    const auto r = soft_->submit(f.cl);
    f.cl.draws = saved;
    if (!r) return fail(std::string("submit: ") + error_name(r.error()));
    uint32_t rw = 0, rh = 0;
    if (!soft_->read_frame(out, rw, rh) || rw != w || rh != h) return fail("leitura do quadro");
    ++st.soft_frames;
    return true;
}

HiresImage hires_for(std::span<const uint32_t> shown, uint32_t width, uint32_t height) {
    return g_vkLast ? g_vkLast->hires_for(shown, width, height) : HiresImage{};
}

bool read_hires(const HiresImage& h, std::vector<uint32_t>& argb) {
    uint32_t w = 0;
    return g_vkLast && g_vkLast->read_hires(h, argb, w) && w == h.width;
}

// ---- o arquivo da IR ----

namespace {
constexpr char kDumpMagic[8] = {'B', 'S', 'I', 'R', '1', 0, 0, 0};
template <class T> void put(std::ofstream& o, const T& v) { o.write(reinterpret_cast<const char*>(&v), sizeof v); }
template <class T> void put_vec(std::ofstream& o, const std::vector<T>& v) {
    put(o, uint32_t(v.size()));
    if (!v.empty()) o.write(reinterpret_cast<const char*>(v.data()), std::streamsize(v.size() * sizeof(T)));
}
template <class T> bool get(std::ifstream& i, T& v) { return bool(i.read(reinterpret_cast<char*>(&v), sizeof v)); }
template <class T> bool get_vec(std::ifstream& i, std::vector<T>& v) {
    uint32_t n = 0;
    if (!get(i, n) || n > (1u << 28)) return false;
    v.resize(n);
    return n == 0 || bool(i.read(reinterpret_cast<char*>(v.data()), std::streamsize(size_t(n) * sizeof(T))));
}
} // namespace

bool save_frame_dump(const std::string& path, const Frame& f, TextureSource& src) {
    std::ofstream o(std::filesystem::path(path), std::ios::binary | std::ios::trunc);
    if (!o) return false;
    o.write(kDumpMagic, 8);
    put(o, f.cl.frame);
    put_vec(o, f.cl.vertices), put_vec(o, f.cl.draws), put_vec(o, f.cl.modifiers);
    put(o, uint32_t(f.textures.size()));
    for (const TextureKey& k : f.textures) {
        std::vector<uint32_t> argb;
        bool                  legacy = false;
        if (!src.argb(k, argb, legacy)) argb.assign(size_t(k.width) * k.height, 0xFFFF00FFu);
        put(o, k.width), put(o, k.height), put_vec(o, argb);
    }
    return bool(o);
}

bool load_frame_dump(const std::string& path, nr::CommandList& cl, std::vector<DumpTexture>& textures) {
    std::ifstream i(std::filesystem::path(path), std::ios::binary);
    char          magic[8];
    if (!i.read(magic, 8) || std::memcmp(magic, kDumpMagic, 8) != 0) return false;
    if (!get(i, cl.frame) || !get_vec(i, cl.vertices) || !get_vec(i, cl.draws) || !get_vec(i, cl.modifiers)) return false;
    uint32_t n = 0;
    if (!get(i, n) || n > 65536) return false;
    textures.resize(n);
    for (DumpTexture& t : textures) {
        if (!get(i, t.desc.width) || !get(i, t.desc.height) || !get_vec(i, t.argb)) return false;
        t.desc.format = nr::PixelFormat::ARGB8888;
    }
    return true;
}

void maybe_save_frame_dump(uint64_t render, const Frame& f, TextureSource& src) {
    static const std::pair<uint64_t, std::string> dumpAt = [] {
        const char* v = std::getenv("HH_RENDER_IR_GUARDAR");
        if (!v || !std::strchr(v, ':')) return std::pair<uint64_t, std::string>{0, {}};
        return std::pair<uint64_t, std::string>{std::strtoull(v, nullptr, 10), std::strchr(v, ':') + 1};
    }();
    if (dumpAt.first && render == dumpAt.first)
        std::fprintf(stderr, "[render] IR do render %llu em %s: %s\n", static_cast<unsigned long long>(dumpAt.first),
                     dumpAt.second.c_str(), save_frame_dump(dumpAt.second, f, src) ? "ok" : "FALHOU");
}

// ---- o resumo ----

std::string format_summary(const Stats& s) {
    char b[1400];
    std::snprintf(b, sizeof b,
                  "[render-ir] renders %llu (conferidos %llu): desenhos %llu, vertices %llu, triangulos %llu, sprites %llu; "
                  "IR invalida %llu, triangulos divergentes %llu, imagens divergentes %llu; texturas conferidas %llu "
                  "(divergentes %llu, sem conversao nativa %llu; chaves distintas %llu, conteudos distintos %llu); quadros "
                  "divergentes %llu; volumes modificadores %llu (%llu triangulos, sem fechamento %llu), parametros "
                  "omitidos %llu; fog desenhos tabela/vertice/nenhum/tabela2 %llu/%llu/%llu/%llu (primeiro render "
                  "%llu/%llu/%llu/%llu), primeiro volume render %llu%s%s\n",
                  (unsigned long long)s.renders, (unsigned long long)s.checked, (unsigned long long)s.draws,
                  (unsigned long long)s.vertices, (unsigned long long)s.triangles, (unsigned long long)s.sprites,
                  (unsigned long long)s.invalid, (unsigned long long)s.triangle_mismatch, (unsigned long long)s.image_mismatch,
                  (unsigned long long)s.textures_checked, (unsigned long long)s.texture_mismatch,
                  (unsigned long long)s.textures_without_native, (unsigned long long)s.distinct_texture_keys,
                  (unsigned long long)s.distinct_texture_ids, (unsigned long long)s.frame_mismatch,
                  (unsigned long long)s.modifier_volumes, (unsigned long long)s.modifier_params,
                  (unsigned long long)s.modifier_unclosed, (unsigned long long)s.skipped_params,
                  (unsigned long long)s.fog_draws[0], (unsigned long long)s.fog_draws[1],
                  (unsigned long long)s.fog_draws[2], (unsigned long long)s.fog_draws[3],
                  (unsigned long long)s.fog_first_render[0], (unsigned long long)s.fog_first_render[1],
                  (unsigned long long)s.fog_first_render[2], (unsigned long long)s.fog_first_render[3],
                  (unsigned long long)s.modifier_first_render,
                  s.first_error.empty() ? "" : "; primeira: ", s.first_error.c_str());
    std::string out = b;
    if (!s.vk_device.empty() || !s.vk_error.empty()) {
        char v[900];
        std::snprintf(v, sizeof v,
                      "[render-vk] %s: quadros %llu, falhas %llu; texturas enviadas %llu (pelo decodificador do legado %llu, "
                      "descartadas do cache %llu, HD %llu, HD incompativeis %llu); dispositivo recriado %llu vez(es); comparados %llu: pixels diferentes %.4f%% (> 8 niveis %.4f%%), maior "
                      "diferenca %u, pior render %llu (%.3f%%); PSNR minimo %.1f dB, medio %.1f dB, quadros abaixo de 40 dB %llu; GPU por "
                      "quadro: media %.2f ms, pior %.1f ms; camadas translucidas por pixel ate %llu%s%s\n",
                      s.vk_device.empty() ? "indisponivel" : s.vk_device.c_str(), (unsigned long long)s.vk_frames,
                      (unsigned long long)s.vk_failures, (unsigned long long)s.tex_uploads, (unsigned long long)s.tex_legacy,
                      (unsigned long long)s.tex_evicted, (unsigned long long)s.tex_replaced,
                      (unsigned long long)s.tex_replace_rejected, (unsigned long long)s.vk_recoveries,
                      (unsigned long long)s.cmp_frames,
                      s.cmp_pixels ? 100.0 * double(s.cmp_diff) / double(s.cmp_pixels) : 0.0,
                      s.cmp_pixels ? 100.0 * double(s.cmp_diff8) / double(s.cmp_pixels) : 0.0, s.cmp_max,
                      (unsigned long long)s.cmp_worst_render, 100.0 * s.cmp_worst_fraction, s.cmp_frames ? s.cmp_min_psnr : 0.0,
                      s.cmp_frames ? s.cmp_psnr_sum / double(s.cmp_frames) : 0.0, (unsigned long long)s.cmp_below40,
                      s.vk_frames ? s.vk_gpu_ms_total / double(s.vk_frames) : 0.0, s.vk_gpu_ms_max,
                      (unsigned long long)s.vk_max_layers,
                      s.vk_error.empty() ? "" : "; ",
                      s.vk_error.c_str());
        out += v;
    }
    if (s.native_checked) {  // HH15.7c: o estado nativo contra o do runtime
        char r[400];
        std::snprintf(r, sizeof r,
                      "[render-nativo] quadros conferidos %llu (divergentes %llu); texturas conferidas %llu de %llu (divergentes %llu, "
                      "sem conversao nativa %llu)%s%s",
                      (unsigned long long)s.native_checked, (unsigned long long)s.native_frame_mismatch,
                      (unsigned long long)s.native_tex_checked, (unsigned long long)s.frame_textures,
                      (unsigned long long)s.native_tex_mismatch,
                      (unsigned long long)s.native_tex_missing, s.native_error.empty() ? "" : "; primeira: ",
                      s.native_error.c_str());
        out += r;
        out += '\n';
    }
    if (s.list_checked) {  // HH15.7b: o ListSink nativo contra a captura
        char r[300];
        std::snprintf(r, sizeof r, "[render-lista] ListSink conferido com a captura em %llu renders: divergentes %llu%s%s",
                      (unsigned long long)s.list_checked, (unsigned long long)s.list_mismatch,
                      s.list_error.empty() ? "" : "; primeira: ", s.list_error.c_str());
        out += r;
        out += '\n';
    }
    if (s.soft_frames || s.soft_failures || s.soft_mismatch) {  // HH15.7a: software sobre a IR
        char r[400];
        std::snprintf(r, sizeof r,
                      "[render-soft] quadros %llu, falhas %llu, conferidos com o caminho do TA %llu (imagens divergentes %llu); texturas enviadas %llu (pelo decodificador do legado %llu, "
                      "descartadas %llu)%s%s",
                      (unsigned long long)s.soft_frames, (unsigned long long)s.soft_failures,
                      (unsigned long long)s.soft_checked, (unsigned long long)s.soft_mismatch, (unsigned long long)s.soft_tex_uploads, (unsigned long long)s.soft_tex_legacy,
                      (unsigned long long)s.soft_tex_evicted, s.soft_error.empty() ? "" : "; primeira: ", s.soft_error.c_str());
        out += r;
        out += '\n';
    }
    if (s.hires_renders) {  // N7c: resolucao interna
        char r[300];
        std::snprintf(r, sizeof r,
                      "[render-escala] alvo %ux%u: renders %llu; VBlanks com a imagem maior %llu, sem %llu; leitura de volta "
                      "(so a reduzida, feita na GPU): media %.2f ms por render\n",
                      s.hires_w, s.hires_h, (unsigned long long)s.hires_renders, (unsigned long long)s.hires_shown,
                      (unsigned long long)s.hires_missed, s.hires_cpu_ms_total / double(s.hires_renders));
        out += r;
    }
    return out;
}

// ---- o renderizador do produto ----

namespace {

// HH15.7b (produto): a IR do ListSink (as listas do Kamui decodificadas direto) desenhada no Vulkan (com o software sobre
// a IR de recuo) ou no software; HH15.7c: os texels pela memoria nativa
class Renderer {
public:
    explicit Renderer(Mode m) : mode(m) {}
    Mode    mode;
    Frame   frame;
    Stats   st;
    Devices dev{st, native_textures()};

    bool render_list(const km::ListFrame& lf, uint32_t w, uint32_t h, std::vector<uint32_t>& image) {
        frame                   = {};
        frame.cl                = lf.cl;
        frame.textures          = lf.textures;
        frame.texture_stride    = lf.texture_stride;
        frame.modifier_params   = lf.modifier_params, frame.modifier_volumes = lf.modifier_volumes;
        frame.modifier_unclosed = lf.modifier_unclosed, frame.skipped_params = lf.skipped_params;
        frame.sprites           = lf.sprites;
        maybe_save_frame_dump(st.renders + 1, frame, native_textures());
        bool ok;
        if (mode == Mode::Software) ok = dev.soft_render(frame, w, h, image);
        else ok = dev.vk_render(frame, g_scale, w, h, image) || dev.soft_render(frame, w, h, image);  // HH15.7a: o recuo
        st.modifier_unclosed += frame.modifier_unclosed;
        ++st.renders;
        account(frame, st);
        return ok;
    }
};

std::unique_ptr<Renderer> g_renderer;

} // namespace

void install_renderer(Mode m) {
    g_renderer.reset();  // estatisticas, quadro e dispositivos recomecam (Off descarta)
    if (m == Mode::Native || m == Mode::Software) g_renderer = std::make_unique<Renderer>(m);
}

Mode renderer_mode() { return g_renderer ? g_renderer->mode : Mode::Off; }

const Stats& renderer_stats() {
    static const Stats empty;
    return g_renderer ? g_renderer->st : empty;
}

std::string renderer_summary() { return g_renderer ? format_summary(g_renderer->st) : std::string(); }

bool draws_lists() { return g_renderer != nullptr; }

bool draw_list(const native::render::kamui::ListFrame& lf, uint32_t width, uint32_t height, std::vector<uint32_t>& image) {
    return g_renderer && g_renderer->render_list(lf, width, height, image);
}

} // namespace hh::transitional::render_capture
