// SPDX-License-Identifier: MIT
// Derivado de recomp-BStinger native/render/soft/soft_raster.cpp (MIT, Copyright (c) 2026 Blue Stinger
// Recompiled contributors; ver third_party/licenses/recomp-BStinger-LICENSE.txt).
// HH15.7a: o rasterizador de referencia (soft_raster.h), movido do pvr_render do runtime sem mudar as contas. Segue o
// modelo do hardware documentado (KallistiOS: pvr_regs, pvr_internal; Flycast so como referencia de comportamento,
// nenhum codigo copiado).
#include "render/soft/soft_raster.h"

#include <algorithm>
#include <atomic>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <execution>
#include <map>
#include <tuple>
#include <xmmintrin.h>

namespace hh::native::render::soft {
namespace {

// O PVR e outro hardware: o modo de ponto flutuante do guest (FPSCR: arredondamento para zero, FTZ/DAZ), que o runtime
// aplica na thread do jogo, nao vale no render. Toda conta do render roda com o MXCSR padrao (mais proximo, sem
// FTZ/DAZ), na thread que chama e em cada worker da resolucao paralela (cujo MXCSR o pool nao garante).
std::atomic<bool> g_fpViolation{false};  // algum worker calculou fora do ambiente fixo

Color unpack(uint32_t c) {
    return {((c >> 24) & 0xFF) / 255.f, ((c >> 16) & 0xFF) / 255.f, ((c >> 8) & 0xFF) / 255.f, (c & 0xFF) / 255.f};
}

} // namespace

FpEnv::FpEnv() : saved_(_mm_getcsr()) { _mm_setcsr(0x1F80); }
FpEnv::~FpEnv() { _mm_setcsr(saved_); }

namespace {

class Raster {
public:
    int                   w, h;
    Frame                 fr;     // registradores do quadro (lidos uma vez, ou vindos da IR)
    std::vector<uint32_t> color;  // ARGB8888
    std::vector<float>    depth;  // 1/w
    TexelSource*          src_;

    Raster(int w_, int h_, const Frame& f, TexelSource& s)
        : w(w_), h(h_), fr(f), color(size_t(w_) * h_, 0), depth(size_t(w_) * h_, 0.f), src_(&s) {}

    // Fragmento ordenado por pixel (translucido em autosort ou punch-through).
    struct Frag {
        uint32_t pixel, seq, tsp;
        float    z;
        Color    c;
        bool     zwrite;
    };
    std::vector<Frag>* frags = nullptr;  // nao nulo: a lista atual e ordenada por pixel
    uint32_t           seq   = 0;

    // Autosort translucido (ISP_FEED_CFG bit 0 = 0; manual do Dreamcast 3.4.2): em cada
    // pixel, os fragmentos que passaram (>=, compare do poligono ignorado) contra a
    // profundidade das listas opacas sao misturados do mais distante (menor 1/w) para o
    // mais proximo; empate: o primeiro submetido e o mais distante. Sem z-write.
    void resolveSorted(std::vector<Frag>& f) {
        forEachPixel(f, [](const Frag& p, const Frag& q) { return p.z != q.z ? p.z < q.z : p.seq < q.seq; },
                     [&](const Frag* g, size_t n) {
                         for (size_t k = 0; k < n; ++k) color[g[k].pixel] = blend(g[k].tsp, g[k].c, unpack(color[g[k].pixel]));
                     });
        f.clear();
    }

    // Agrupa os fragmentos por pixel (ordenacao por contagem, linear no numero de
    // fragmentos) e ordena cada grupo por `less`, uma ordem total: (z, ordem de submissao),
    // e um poligono nao gera dois fragmentos no mesmo pixel. Mesmo resultado de uma
    // ordenacao global, sem o custo dela com camadas translucidas de tela inteira.
    template <class Less, class Fn> void forEachPixel(const std::vector<Frag>& f, Less less, Fn fn) {
        if (f.empty()) return;
        const size_t          pixels = size_t(w) * h;
        std::vector<uint32_t> start(pixels + 1, 0);
        for (const Frag& g : f) ++start[g.pixel + 1];
        for (size_t i = 1; i <= pixels; ++i) start[i] += start[i - 1];
        std::vector<uint32_t> next(start.begin(), start.end() - 1);
        sorted_.resize(f.size());
        for (const Frag& g : f) sorted_[next[g.pixel]++] = g;
        // cada pixel e independente (fatia propria de sorted_, saida no proprio pixel): as
        // linhas sao resolvidas em paralelo, com o mesmo resultado da ordem sequencial
        std::vector<int> rows(static_cast<size_t>(h));
        for (int y = 0; y < h; ++y) rows[size_t(y)] = y;
        // diagnostico: HH_RENDER_SEQ=1 resolve as linhas em sequencia (mesmo resultado; isola efeitos das threads)
        static const bool seqOnly = std::getenv("HH_RENDER_SEQ") != nullptr;
        auto              body    = [&](int y) {
            const FpEnv fpEnv;  // MXCSR do worker e indefinido (revisao Codex 2026-10-14)
            if ((_mm_getcsr() & ~0x3Fu) != 0x1F80) g_fpViolation.store(true);  // ver take_fp_violation
            for (size_t p = size_t(y) * w, end = p + size_t(w); p < end; ++p) {
                const uint32_t b = start[p], e = start[p + 1];
                if (b == e) continue;
                if (e - b > 1) std::sort(sorted_.begin() + b, sorted_.begin() + e, less);
                fn(&sorted_[b], size_t(e - b));
            }
        };
        if (seqOnly) std::for_each(rows.begin(), rows.end(), body);
        else std::for_each(std::execution::par, rows.begin(), rows.end(), body);
    }
    std::vector<Frag> sorted_;

    // Punch-through (manual 3.4.3): ordenado por pixel da frente para tras, compare forcado
    // em >=; vale o fragmento mais proximo que passou no teste de alfa (os que falham ja
    // foram descartados e revelam a camada seguinte). Empate: o ultimo submetido, como no
    // >= de um z-buffer comum (a regra exata de empate nao foi verificada).
    void resolvePunchThrough(std::vector<Frag>& f) {
        forEachPixel(f, [](const Frag& p, const Frag& q) { return p.z != q.z ? p.z > q.z : p.seq > q.seq; },
                     [&](const Frag* g, size_t) {
                         color[g->pixel] = pack(g->c);
                         // z-write disable e ignorado no punch-through (evidencia de
                         // compatibilidade: DEmul, Bust-A-Move 4; revisao Codex 2026-10-14)
                         depth[g->pixel] = g->z;
                     });
        f.clear();
    }

    void draw(const Tri& t, uint32_t list) {
        ++seq;
        lastTex_ = nullptr;  // o TexRef local de cada draw pode reutilizar o mesmo endereco
        const Vertex &a = t.v[0], &b = t.v[1], &c = t.v[2];
        const float area = (b.x - a.x) * (c.y - a.y) - (c.x - a.x) * (b.y - a.y);
        if (area == 0 || !std::isfinite(area)) return;
        const uint32_t cull = (t.isp >> 27) & 3;
        if (cull == 1 && std::fabs(area) < 2 * fr.cull_val) return;  // FPU_CULL_VAL
        if (cull == 2 && area < 0) return;
        if (cull == 3 && area > 0) return;
        int x0 = std::max(0, int(std::floor(std::min({a.x, b.x, c.x}))));
        int x1 = std::min(w - 1, int(std::ceil(std::max({a.x, b.x, c.x}))));
        int y0 = std::max(0, int(std::floor(std::min({a.y, b.y, c.y}))));
        int y1 = std::min(h - 1, int(std::ceil(std::max({a.y, b.y, c.y}))));
        if (t.clipMode == 2) {  // so dentro do recorte
            x0 = std::max(x0, t.cx0), x1 = std::min(x1, t.cx1);
            y0 = std::max(y0, t.cy0), y1 = std::min(y1, t.cy1);
        }
        if (x0 > x1 || y0 > y1) return;
        const float inv = 1.f / area;
        TexRef     tex;
        if (t.tex) {
            tex.tcw    = t.tcw;
            tex.width  = 8u << ((t.tsp >> 3) & 7);
            tex.height = 8u << (t.tsp & 7);
            tex.stride = fr.tex_stride;  // TEXT_CONTROL
            tex.texture = t.texture;
        }
        // listas ordenadas por pixel (translucida em autosort, punch-through): compare
        // forcado em >= (manual 3.4.2 e 3.4.3; revisao Codex 2026-10-14)
        const uint32_t cmp    = frags ? 6u : (t.isp >> 29);
        const bool     zwrite = !((t.isp >> 26) & 1);
        const uint8_t  ptRef  = uint8_t(fr.pt_ref & 0xFF);
        // HALF_OFFSET (revisao Codex 2026-10-14): bit 0 amostra do FPU (cobertura e
        // profundidade), bit 1 amostra do TSP (atributos), bit 2 meio texel; 1 = centro do
        // pixel/texel, 0 = canto superior esquerdo. O jogo escreve 7.
        const uint32_t half   = fr.half_offset;
        const float    fpuOff = (half & 1) ? 0.5f : 0.f, tspOff = (half & 2) ? 0.5f : 0.f;
        const bool     texelHalf = half & 4;
        // color clamp do TSP (bit 21): limites ARGB de FPU_CLAMP_MAX (0x0BC) e MIN (0x0C0)
        const bool  clampOn = (t.tsp >> 21) & 1;
        const Color cmax = unpack(fr.clamp_max), cmin = unpack(fr.clamp_min);
        for (int y = y0; y <= y1; ++y)
            for (int x = x0; x <= x1; ++x) {
                if (t.clipMode == 3 && x >= t.cx0 && x <= t.cx1 && y >= t.cy0 && y <= t.cy1) continue;  // so fora
                const float px = x + fpuOff, py = y + fpuOff;
                float       w0 = (b.x - px) * (c.y - py) - (c.x - px) * (b.y - py);
                float       w1 = (c.x - px) * (a.y - py) - (a.x - px) * (c.y - py);
                float       w2 = (a.x - px) * (b.y - py) - (b.x - px) * (a.y - py);
                if (area < 0) w0 = -w0, w1 = -w1, w2 = -w2;
                if (w0 < 0 || w1 < 0 || w2 < 0) continue;
                if ((w0 == 0 && !topLeft(b, c, area)) || (w1 == 0 && !topLeft(c, a, area)) ||
                    (w2 == 0 && !topLeft(a, b, area)))
                    continue;
                const float  l0 = w0 * std::fabs(inv), l1 = w1 * std::fabs(inv), l2 = w2 * std::fabs(inv);
                const float  z  = l0 * a.z + l1 * b.z + l2 * c.z;
                const size_t i  = size_t(y) * w + x;
                if (!depthPass(cmp, z, depth[i])) continue;
                // atributos na amostra do TSP (igual a do FPU quando os bits 0 e 1 coincidem)
                float m0 = l0, m1 = l1, m2 = l2;
                if (tspOff != fpuOff) {
                    const float qx = x + tspOff, qy = y + tspOff;
                    m0 = ((b.x - qx) * (c.y - qy) - (c.x - qx) * (b.y - qy)) * inv;
                    m1 = ((c.x - qx) * (a.y - qy) - (a.x - qx) * (c.y - qy)) * inv;
                    m2 = 1.f - m0 - m1;
                }
                const float zt = m0 * a.z + m1 * b.z + m2 * c.z;
                // correcao de perspectiva: atributos * (1/w) interpolados, / (1/w)
                const float p0 = m0 * a.z / zt, p1 = m1 * b.z / zt, p2 = m2 * c.z / zt;
                auto        lerp = [&](float va, float vb, float vc) { return p0 * va + p1 * vb + p2 * vc; };
                const Vertex&  fl   = c;  // sombreamento plano: cor do ultimo vertice
                Color col = t.gouraud ? Color{lerp(a.col.a, b.col.a, c.col.a), lerp(a.col.r, b.col.r, c.col.r),
                                              lerp(a.col.g, b.col.g, c.col.g), lerp(a.col.b, b.col.b, c.col.b)}
                                      : fl.col;
                Color off = t.gouraud ? Color{0, lerp(a.off.r, b.off.r, c.off.r), lerp(a.off.g, b.off.g, c.off.g),
                                              lerp(a.off.b, b.off.b, c.off.b)}
                                      : fl.off;
                if (!((t.tsp >> 20) & 1)) col.a = 1.f;  // use alpha = 0
                Color out = col;
                if (t.tex) out = shade(t, tex, lerp(a.u, b.u, c.u), lerp(a.v, b.v, c.v), col, off, texelHalf);
                // Diagnostico: HH_RENDER_PROBE=x,y registra cada primitiva que pinta o pixel
                static const char* probe = std::getenv("HH_RENDER_PROBE");
                if (probe) {
                    int qx = -1, qy = -1;
                    std::sscanf(probe, "%d,%d", &qx, &qy);
                    if (qx == x && qy == y)
                        std::fprintf(stderr,
                                     "[probe] (%d,%d) lista %u tsp %08X tcw %08X uv %.7f %.7f (texel %.5f %.5f) "
                                     "col %.3f %.3f %.3f %.3f -> %.3f %.3f %.3f %.3f\n",
                                     x, y, list, t.tsp, t.tcw, lerp(a.u, b.u, c.u), lerp(a.v, b.v, c.v),
                                     lerp(a.u, b.u, c.u) * tex.width, lerp(a.v, b.v, c.v) * tex.height, col.a, col.r,
                                     col.g, col.b, out.a, out.r, out.g, out.b);
                }
                if (clampOn) {  // min(max()) e nao std::clamp: o jogo pode programar min > max
                    auto cl = [](float v, float lo, float hi) { return std::min(std::max(v, lo), hi); };
                    out = {cl(out.a, cmin.a, cmax.a), cl(out.r, cmin.r, cmax.r), cl(out.g, cmin.g, cmax.g),
                           cl(out.b, cmin.b, cmax.b)};
                }
                if (list == 4 && uint8_t(std::lround(clamp01(out.a) * 255)) < ptRef) continue;
                if (frags) {  // lista ordenada por pixel: resolvida depois de todos os poligonos
                    frags->push_back({uint32_t(i), seq, t.tsp, z, out, zwrite});
                    continue;
                }
                const uint32_t dst = color[i];
                color[i]           = list == 2 ? blend(t.tsp, out, unpack(dst)) : pack(out);
                if (probe) {
                    int qx = -1, qy = -1;
                    std::sscanf(probe, "%d,%d", &qx, &qy);
                    if (qx == x && qy == y) std::fprintf(stderr, "[probe]   destino %08X -> %08X\n", dst, color[i]);
                }
                if (zwrite) depth[i] = z;
            }
    }

    void fillBackground(uint32_t argb, float z) {
        std::fill(color.begin(), color.end(), argb);
        std::fill(depth.begin(), depth.end(), z);
    }

private:
    static float clamp01(float v) { return v < 0 ? 0 : (v > 1 ? 1 : v); }

    static uint32_t pack(const Color& c) {
        auto q = [](float v) { return uint32_t(std::lround(clamp01(v) * 255)); };
        return (q(c.a) << 24) | (q(c.r) << 16) | (q(c.g) << 8) | q(c.b);
    }

    // aresta de cima ou da esquerda (regra de preenchimento)
    static bool topLeft(const Vertex& p, const Vertex& q, float area) {
        // y para baixo: com area > 0 as arestas vao no sentido horario na tela; a
        // aresta de cima vai para a direita (dx > 0) e as da esquerda sobem (dy < 0).
        float dx = q.x - p.x, dy = q.y - p.y;
        if (area > 0) dx = -dx, dy = -dy;
        return (dy == 0 && dx < 0) || dy > 0;
    }

    static bool depthPass(uint32_t cmp, float z, float old) {
        switch (cmp) {
        case 0: return false;
        case 1: return z < old;
        case 2: return z == old;
        case 3: return z <= old;
        case 4: return z > old;
        case 5: return z != old;
        case 6: return z >= old;
        default: return true;
        }
    }

    // Texels ja lidos neste render (a memoria das texturas nao muda durante um render): preenchidos sob
    // demanda, so os texels usados.
    struct TexCache {
        std::vector<uint32_t> argb;
        std::vector<uint8_t>  have;
    };
    std::map<std::tuple<uint32_t, uint32_t, uint32_t, uint32_t, uint32_t>, TexCache> texCache_;
    const TexRef* lastTex_ = nullptr;
    TexCache*      lastCache_ = nullptr;

    Color texel(const TexRef& t, int x, int y, uint32_t tsp) {
        const int  W = int(t.width), H = int(t.height);
        const bool clampU = (tsp >> 16) & 1, clampV = (tsp >> 15) & 1;
        const bool flipU = (tsp >> 18) & 1, flipV = (tsp >> 17) & 1;
        auto       wrap  = [](int v, int n, bool cl, bool fl) {
            if (cl) return v < 0 ? 0 : (v >= n ? n - 1 : v);
            if (fl) {
                const int m = ((v % (2 * n)) + 2 * n) % (2 * n);
                return m < n ? m : 2 * n - 1 - m;
            }
            return ((v % n) + n) % n;
        };
        if (lastTex_ != &t) {
            TexCache& e = texCache_[{t.tcw, t.width, t.height, t.stride, t.texture}];
            if (e.argb.empty()) {
                e.argb.resize(size_t(t.width) * t.height);
                e.have.assign(size_t(t.width) * t.height, 0);
            }
            lastTex_ = &t, lastCache_ = &e;
        }
        const uint32_t tx = uint32_t(wrap(x, W, clampU, flipU)), ty = uint32_t(wrap(y, H, clampV, flipV));
        const size_t   i  = size_t(ty) * t.width + tx;
        if (!lastCache_->have[i]) {
            lastCache_->argb[i] = src_->texel(t, tx, ty);
            lastCache_->have[i] = 1;
        }
        return unpack(lastCache_->argb[i]);
    }

    Color shade(const Tri& t, const TexRef& tex, float u, float v, const Color& col, const Color& off,
                bool texelHalf) {
        const float half = texelHalf ? 0.5f : 0.f;  // HALF_OFFSET bit 2: centro do texel
        const float fu = u * tex.width - half, fv = v * tex.height - half;
        Color       tx;
        if (((t.tsp >> 13) & 3) == 0) {  // ponto
            // Amostra exatamente na fronteira de texels (o texto do jogo cai assim): o texel
            // escolhido depende da precisao numerica; nao ha referencia de hardware ainda
            // (STATUS.md, comparacao com o Flycast).
            tx = texel(tex, int(std::floor(fu + 0.5f)), int(std::floor(fv + 0.5f)), t.tsp);
        } else {  // bilinear
            const int   ix = int(std::floor(fu)), iy = int(std::floor(fv));
            const float ax = fu - ix, ay = fv - iy;
            const Color c00 = texel(tex, ix, iy, t.tsp), c10 = texel(tex, ix + 1, iy, t.tsp);
            const Color c01 = texel(tex, ix, iy + 1, t.tsp), c11 = texel(tex, ix + 1, iy + 1, t.tsp);
            auto        mix = [&](float p, float q, float r, float s) {
                return (p * (1 - ax) + q * ax) * (1 - ay) + (r * (1 - ax) + s * ax) * ay;
            };
            tx = {mix(c00.a, c10.a, c01.a, c11.a), mix(c00.r, c10.r, c01.r, c11.r), mix(c00.g, c10.g, c01.g, c11.g),
                  mix(c00.b, c10.b, c01.b, c11.b)};
        }
        if ((t.tsp >> 19) & 1) tx.a = 1.f;  // ignore texture alpha
        Color o;
        switch ((t.tsp >> 6) & 3) {
        case 0: o = tx; break;                                                                // decal
        case 1: o = {tx.a, tx.r * col.r, tx.g * col.g, tx.b * col.b}; break;                   // modulate
        case 2: o = {col.a, tx.r * tx.a + col.r * (1 - tx.a), tx.g * tx.a + col.g * (1 - tx.a),
                     tx.b * tx.a + col.b * (1 - tx.a)}; break;                                 // decal alpha
        default: o = {tx.a * col.a, tx.r * col.r, tx.g * col.g, tx.b * col.b}; break;         // modulate alpha
        }
        if (t.offset) o.r += off.r, o.g += off.g, o.b += off.b;
        return o;
    }

    static float factor(uint32_t f, bool isSrc, const Color& s, const Color& d, int ch) {
        const float sc[4] = {s.a, s.r, s.g, s.b}, dc[4] = {d.a, d.r, d.g, d.b};
        switch (f) {
        case 0: return 0;
        case 1: return 1;
        case 2: return isSrc ? dc[ch] : sc[ch];  // "outra" cor
        case 3: return 1 - (isSrc ? dc[ch] : sc[ch]);
        case 4: return s.a;
        case 5: return 1 - s.a;
        case 6: return d.a;
        default: return 1 - d.a;
        }
    }

    static uint32_t blend(uint32_t tsp, const Color& s, const Color& d) {
        const uint32_t sf = tsp >> 29, df = (tsp >> 26) & 7;
        const float    sc[4] = {s.a, s.r, s.g, s.b}, dc[4] = {d.a, d.r, d.g, d.b};
        float          o[4];
        for (int ch = 0; ch < 4; ++ch) o[ch] = sc[ch] * factor(sf, true, s, d, ch) + dc[ch] * factor(df, false, s, d, ch);
        return pack({o[0], o[1], o[2], o[3]});
    }
};

// As listas na ordem do hardware (opaca, punch-through, translucida) e a resolucao das ordenadas por pixel; `lap`
// mede as fases (diagnostico). Devolve o numero de fragmentos translucidos ordenados.
template <class Lap> size_t drawLists(std::span<const Tri> prims, Raster& r, Lap lap) {
    // ISP_FEED_CFG (0x098) bit 0: 1 = pre-sort (translucidos na ordem de submissao),
    // 0 = autosort por pixel (o jogo usa 0x00800408)
    std::vector<Raster::Frag> frags;
    const bool                autosort = r.fr.autosort;
    for (uint32_t list : {0u, 4u, 2u}) {
        r.frags = (list == 4 || (list == 2 && autosort)) ? &frags : nullptr;
        for (const Tri& t : prims)
            if (t.list == list) r.draw(t, list);
        if (list == 4) r.resolvePunchThrough(frags);
        lap(list == 0 ? 1 : (list == 4 ? 2 : 3));
    }
    const size_t nfrags = frags.size();
    if (autosort) r.resolveSorted(frags);
    lap(4);
    return nfrags;
}

struct PhaseLap {
    PhaseFn fn;
    void*   ctx;
    void    operator()(int phase) const {
        if (fn) fn(ctx, phase);
    }
};

} // namespace

size_t rasterize(std::span<const Tri> tris, const Frame& frame, TexelSource& texels, Image& image, PhaseFn phase,
                 void* phase_ctx) {
    const FpEnv fpEnv;  // ambiente fixo nesta thread (os workers instalam o seu)
    Raster      r(int(image.width), int(image.height), frame, texels);
    r.fillBackground(0xFF000000 | frame.background, frame.background_depth);  // ISP_BACKGND_T/D
    const size_t n = drawLists(tris, r, PhaseLap{phase, phase_ctx});
    image.color    = std::move(r.color);
    image.depth    = std::move(r.depth);
    return n;
}

bool take_fp_violation() { return g_fpViolation.exchange(false); }

} // namespace hh::native::render::soft
