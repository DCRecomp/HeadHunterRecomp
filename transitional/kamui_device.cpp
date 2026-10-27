// SPDX-License-Identifier: MIT
// HH13.6: funis do SDK grafico (Kamui/Shinobi) do Head Hunter alimentando um estado nativo (modo sombra). Ver
// kamui_device.h e docs/planos/HH13-PLANO.md.
#include "kamui_device.h"

#include "cpu/sh4_mem.h"
#include "cpu/sh4_runtime.h"
#include "guest_abi.h"
#include "mods/mod_api.h"
#include "pvr/pvr_regs.h"
#include "pvr/ta.h"
#include "pvr/vram.h"

#include <array>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <stdexcept>
#include <vector>

namespace hh::transitional::kamui {

namespace {

constexpr uint32_t kRegBase = 0x005F8000, kRegCount = 0x400 / 4;
constexpr uint32_t kSoftReset = 0x008, kStartRender = 0x014, kListInit = 0x144;
constexpr uint32_t kBlock = 32;  // granularidade das marcas de memoria escrita

// O fluxo de parametros do TA, dividido como pvr/ta.cpp (polygonFifo): parametros de 32 ou 64 bytes por lista. So a
// divisao (a mesma regra), sem interrupcoes nem estado de hardware: e o formato.
struct Params {
    int                       list = -1;
    bool                      vertex64 = false, pendingHalf = false;
    std::array<uint32_t, 8>   half{};
    uint32_t                  bytes = 0;  // recebidos desde o TA_LIST_INIT
    std::vector<TaParam>      out;

    static bool global64(uint32_t pcw) {
        const uint32_t col = (pcw >> 4) & 3, tex = (pcw >> 3) & 1, off = (pcw >> 2) & 1, vol = (pcw >> 6) & 1;
        return vol ? col == 2 : (col == 2 && tex && off);
    }
    static bool vertexIs64(uint32_t pcw) {
        const uint32_t col = (pcw >> 4) & 3, tex = (pcw >> 3) & 1, vol = (pcw >> 6) & 1;
        if (!tex) return false;
        return vol ? true : col == 1;
    }
    void push(const uint32_t* w, uint32_t words) {
        TaParam p;
        p.list  = uint32_t(list < 0 ? 0 : list);
        p.words = words;
        std::memcpy(p.w, w, words * 4);
        out.push_back(p);
    }
    void reset() { *this = Params{}; }
    // 32 bytes; false se o tipo do parametro e invalido (o runtime para com erro fatal)
    bool feed(const uint32_t* d) {
        bytes += 32;
        if (pendingHalf) {
            uint32_t w[16];
            std::memcpy(w, half.data(), 32);
            std::memcpy(w + 8, d, 32);
            push(w, 16);
            pendingHalf = false;
            return true;
        }
        const uint32_t pcw = d[0], type = pcw >> 29;
        if ((type == 4 || type == 5) && list < 0) list = int((pcw >> 24) & 7);
        switch (type) {
        case 0: push(d, 8), list = -1; return true;  // fim de lista
        case 1:
        case 2: push(d, 8); return true;  // recorte do usuario, object list set
        case 4: {
            const bool modvol = list == 1 || list == 3;
            vertex64          = modvol || vertexIs64(pcw);
            if (!modvol && global64(pcw)) break;
            push(d, 8);
            return true;
        }
        case 5: vertex64 = true, push(d, 8); return true;
        case 7:
            if (vertex64) break;
            push(d, 8);
            return true;
        default: return false;
        }
        std::memcpy(half.data(), d, 32);
        pendingHalf = true;
        return true;
    }
};

struct State {
    Mode                                 mode = Mode::Off;
    std::array<uint32_t, kRegCount>      regs{};
    std::array<bool, kRegCount>          regSet{};
    std::vector<uint8_t>                 mem = std::vector<uint8_t>(kVramSize);
    std::vector<uint8_t>                 dirty = std::vector<uint8_t>(kVramSize / kBlock);  // blocos escritos pelos funis
    Params                               params;
    Stats                                stats;
};

State& st() {
    static State s;
    return s;
}

void mismatch(uint64_t& counter, const std::string& what) {
    ++counter;
    if (st().stats.first.empty()) st().stats.first = what;
}

// Offset no layout de 32 bits da VRAM (como pvr/vram.cpp, locate), ou -1 fora da area 1.
int64_t vramOffset(uint32_t a) {
    if (a >= 0xE0000000) return -1;
    const uint32_t phys = a & 0x1FFFFFFF;
    if ((phys >> 26) != 1) return -1;
    const uint32_t o = phys & (kVramSize - 1);
    return (phys & 0x01000000) ? o : vram_offset64(o);
}

void memWrite(uint32_t addr, uint8_t v) {
    const int64_t o = vramOffset(addr);
    if (o < 0) return;
    State& s           = st();
    s.mem[size_t(o)]   = v;
    s.dirty[size_t(o) / kBlock] = 1;
}

// Caminho direto de textura (0x11000000+): 32 bytes pelo caminho de 64 bits (pvr/ta.cpp, ta_fifo_write).
void textureWrite(uint32_t target, const uint32_t* d) {
    State&         s   = st();
    const uint32_t off = (target & 0x1FFFFFFF) & 0x00FFFFE0;
    for (int i = 0; i < 8; ++i) {
        const uint32_t o = vram_offset64(off + 4 * i);
        std::memcpy(&s.mem[o], &d[i], 4);
        s.dirty[o / kBlock] = 1;
    }
}

// Confere o estado nativo com o do runtime antes do render (STARTRENDER).
void compareAtStartRender() {
    State& s = st();
    ++s.stats.renders;
    for (uint32_t i = 0; i < kRegCount; ++i) {
        if (!s.regSet[i] || i * 4 == kSoftReset || i * 4 == kStartRender || i * 4 == kListInit) continue;  // disparos
        const uint32_t rt = pvr_reg(kRegBase + i * 4);
        if (rt != s.regs[i]) {
            char b[96];
            std::snprintf(b, sizeof b, "registrador %03X: nativo %08X, runtime %08X (render %llu)", i * 4, s.regs[i], rt,
                          (unsigned long long)s.stats.renders);
            mismatch(s.stats.regMismatch, b);
        }
    }
    const auto& rp = ta_params();
    s.stats.paramsChecked += rp.size();
    if (rp.size() != s.params.out.size()) {
        char b[96];
        std::snprintf(b, sizeof b, "parametros: nativo %zu (%u bytes), runtime %zu (%u bytes) (render %llu)",
                      s.params.out.size(), s.params.bytes, rp.size(), ta_bytes(), (unsigned long long)s.stats.renders);
        if (std::getenv("HH_KAMUI_LOG") && s.stats.paramMismatch == 0) {  // diagnostico: as duas sequencias
            for (const TaParam& p : s.params.out) std::fprintf(stderr, "[kamui] nativo  lista %u PCW %08X\n", p.list, p.w[0]);
            for (const TaParam& p : rp) std::fprintf(stderr, "[kamui] runtime lista %u PCW %08X\n", p.list, p.w[0]);
        }
        mismatch(s.stats.paramMismatch, b);
    } else {
        for (size_t i = 0; i < rp.size(); ++i) {
            const TaParam &a = s.params.out[i], &b = rp[i];
            if (a.list != b.list || a.words != b.words || std::memcmp(a.w, b.w, a.words * 4) != 0) {
                char m[160];
                std::snprintf(m, sizeof m, "parametro %zu de %zu diferente (render %llu): nativo lista %u %u palavras PCW %08X, "
                              "runtime lista %u %u palavras PCW %08X", i, rp.size(), (unsigned long long)s.stats.renders,
                              a.list, a.words, a.w[0], b.list, b.words, b.w[0]);
                mismatch(s.stats.paramMismatch, m);
                break;
            }
        }
    }
    const uint8_t* v = vram_data();
    for (size_t blk = 0; blk < s.dirty.size(); ++blk) {
        if (!s.dirty[blk]) continue;
        s.dirty[blk] = 0;
        ++s.stats.memBlocksChecked;
        if (std::memcmp(&s.mem[blk * kBlock], v + blk * kBlock, kBlock) != 0) {
            char b[96];
            std::snprintf(b, sizeof b, "memoria: bloco %06zX diferente (render %llu)", blk * kBlock,
                          (unsigned long long)s.stats.renders);
            mismatch(s.stats.memMismatch, b);
        }
    }
}

void callOriginal(Sh4Context* c, uint32_t addr) { sh4_lookup_generated(addr)(c); }

// diagnostico: HH_KAMUI_ORDEM=<n> registra os n primeiros eventos (TA_LIST_INIT, STARTRENDER, pedidos de DMA) com os
// bytes que o TA do runtime ja recebeu antes e depois de cada um
int ordemLeft() {
    static int left = [] { const char* v = std::getenv("HH_KAMUI_ORDEM"); return v ? std::atoi(v) : 0; }();
    return left > 0 ? left-- : 0;
}

void fRegWrite(Sh4Context* c) {
    State&         s   = st();
    const uint32_t off = c->r[4], val = c->r[5];
    ++s.stats.regWrites;
    if (off < 0x400 && !(off & 3)) {
        if (off == kStartRender) compareAtStartRender();
        if (off == kListInit) s.params.reset();
        s.regs[off / 4] = val, s.regSet[off / 4] = true;
    }
    const uint32_t before = ta_bytes();
    callOriginal(c, kRegWrite);
    if ((off == kStartRender || off == kListInit) && ordemLeft())
        std::fprintf(stderr, "[kamui] ordem: %s (TA do runtime %u -> %u bytes; nativo %u)\n",
                     off == kListInit ? "TA_LIST_INIT" : "STARTRENDER", before, ta_bytes(), s.params.bytes);
}

void fRegRead(Sh4Context* c) {
    ++st().stats.regReads;
    callOriginal(c, kRegRead);
}

void fDma(Sh4Context* c) {
    State&      s = st();
    GuestMemory mem(c);
    // o descritor e lido antes (o original pode altera-lo) e os dados sao espelhados DEPOIS do original: o pedido de tipo 1
    // escreve TA_LIST_INIT pelo funil de registradores antes de transferir (0x8C17D200), e o TA so recebe os dados depois
    const uint32_t desc = c->r[4];
    const auto     type = mem.load<uint32_t>(desc + 8), len = mem.load<uint32_t>(desc + 12),
               src = mem.load<uint32_t>(desc + 16), dst = mem.load<uint32_t>(desc + 20), flags = mem.load<uint32_t>(desc + 4);
    // diagnostico: HH_KAMUI_LOG=<n> registra os n primeiros pedidos de DMA (tipo, flags, bytes, origem, destino)
    static const int logMax = [] { const char* v = std::getenv("HH_KAMUI_LOG"); return v ? std::atoi(v) : 0; }();
    static int       logged = 0;
    if (logged < logMax) {
        ++logged;
        std::fprintf(stderr, "[kamui] DMA tipo %d flags %08X bytes %u origem %08X destino %08X pr %08X\n", type ? int(*type) : -1,
                     flags ? *flags : 0, len ? *len : 0, src ? *src : 0, dst ? *dst : 0, c->pr);
    }
    const uint32_t before = ta_bytes();
    callOriginal(c, kDma);
    // canal 2 para a area 4 (FIFO de poligonos ou caminho direto de textura): os tipos 0, 1 e 2 do pedido vao para la
    // (caminhos diferentes do mesmo funil); vale o destino do descritor, nao o tipo (log HH_KAMUI_LOG)
    const bool ch2 = type && *type <= 4 && dst && ((*dst & 0x1FFFFFFF) >> 26) == 4;
    if (ch2 && len && src) {
        const bool            fifo = ((*dst & 0x1FFFFFFF) & 0x01800000) == 0;
        std::vector<uint32_t> blk(8);
        for (uint32_t off = 0; off + 32 <= *len; off += 32) {
            for (int i = 0; i < 8; ++i) blk[i] = sh4_read32(c, *src + off + 4 * uint32_t(i));
            if (fifo) {
                if (!s.params.feed(blk.data())) mismatch(s.stats.paramMismatch, "parametro do TA com tipo invalido");
            } else {
                textureWrite(*dst + off, blk.data());
            }
        }
        (fifo ? s.stats.dmaTa : s.stats.dmaTexture)++;
        (fifo ? s.stats.taBytes : s.stats.textureBytes) += *len;
    } else {
        ++s.stats.dmaOther;
    }
    if (ordemLeft())
        std::fprintf(stderr, "[kamui] ordem: DMA tipo %d %u bytes destino %08X (TA do runtime %u -> %u bytes; nativo %u)\n",
                     type ? int(*type) : -1, len ? *len : 0, dst ? *dst : 0, before, ta_bytes(), s.params.bytes);
}

void fCopy(Sh4Context* c) {
    State&         s   = st();
    const uint32_t dst = c->r[4], src = c->r[5], n = c->r[6];
    if (vramOffset(dst) >= 0) {
        for (uint32_t i = 0; i < n; ++i) memWrite(dst + i, sh4_read8(c, src + i));
        ++s.stats.copies, s.stats.copyBytes += n;
    }
    callOriginal(c, kCopy);
}

void fZero(Sh4Context* c) {
    State&         s   = st();
    const uint32_t dst = c->r[4], n = c->r[5];
    if (vramOffset(dst) >= 0) {
        for (uint32_t i = 0; i < n; ++i) memWrite(dst + i, 0);
        ++s.stats.zeros, s.stats.zeroBytes += n;
    }
    callOriginal(c, kZero);
}

const std::pair<uint32_t, Sh4Func> kFns[] = {
    {kRegWrite, &fRegWrite}, {kRegRead, &fRegRead}, {kDma, &fDma}, {kCopy, &fCopy}, {kZero, &fZero}};

} // namespace

void install(BoundaryRegistry& registry, Mode m) {
    for (const auto& [addr, fn] : kFns)
        if (!sh4_lookup_generated(addr) || !mods::has_slot(addr)) {
            char hexa[16];
            std::snprintf(hexa, sizeof hexa, "%08X", addr);
            throw std::runtime_error(std::string("provedor nativo do Kamui: o funil ") + hexa +
                                     " nao tem slot (gere o codigo de novo com [native] slots)");
        }
    State& s = st();
    s        = State{};
    s.mode   = m;
    std::memcpy(s.mem.data(), vram_data(), kVramSize);  // a sombra parte do estado atual
    for (const auto& [addr, fn] : kFns) {
        registry.add(addr, dcmap::Subsystem::Render, "kamui");
        registry.set_native(addr, fn);
        if (registry.select(addr, Provider::Native) != SelectResult::Ok)
            throw std::runtime_error("provedor nativo do Kamui: troca de provedor falhou");
    }
}

void uninstall(BoundaryRegistry& registry) {
    for (const auto& [addr, fn] : kFns) registry.select(addr, Provider::Legacy);
    st().mode = Mode::Off;
}

bool installed() { return st().mode != Mode::Off; }

Stats stats() { return st().stats; }

std::string summary() {
    const Stats& s = st().stats;
    char         b[640];
    std::snprintf(b, sizeof b,
                  "[kamui] sombra: renders %llu; registradores escritos %llu (divergentes %llu), lidos %llu; DMA TA %llu "
                  "(%llu bytes), textura %llu (%llu bytes), outros %llu; copias %llu (%llu bytes), zeragens %llu (%llu "
                  "bytes); parametros conferidos %llu (divergencias %llu); blocos de memoria conferidos %llu (divergentes "
                  "%llu)%s%s\n",
                  (unsigned long long)s.renders, (unsigned long long)s.regWrites, (unsigned long long)s.regMismatch,
                  (unsigned long long)s.regReads, (unsigned long long)s.dmaTa, (unsigned long long)s.taBytes,
                  (unsigned long long)s.dmaTexture, (unsigned long long)s.textureBytes, (unsigned long long)s.dmaOther,
                  (unsigned long long)s.copies, (unsigned long long)s.copyBytes, (unsigned long long)s.zeros,
                  (unsigned long long)s.zeroBytes, (unsigned long long)s.paramsChecked,
                  (unsigned long long)s.paramMismatch, (unsigned long long)s.memBlocksChecked,
                  (unsigned long long)s.memMismatch, s.first.empty() ? "" : "; primeira: ", s.first.c_str());
    return b;
}

} // namespace hh::transitional::kamui
