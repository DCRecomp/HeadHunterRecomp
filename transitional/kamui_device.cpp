// SPDX-License-Identifier: MIT
// HH13.6: funis do SDK grafico (Kamui/Shinobi) do Head Hunter alimentando um estado nativo (modo sombra). Ver
// kamui_device.h e docs/planos/HH13-PLANO.md.
#include "kamui_device.h"

#include "cpu/sh4_mem.h"
#include "cpu/sh4_runtime.h"
#include "guest_abi.h"
#include "mods/mod_api.h"
#include "pvr/holly_intc.h"
#include "pvr/pvr_regs.h"
#include "pvr/pvr_video.h"
#include "pvr/ta.h"
#include "pvr/vram.h"

#include <array>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <map>
#include <stdexcept>
#include <tuple>
#include <vector>

namespace hh::transitional::kamui {

namespace {

constexpr uint32_t kRegBase = 0x005F8000, kRegCount = 0x2000 / 4;  // o banco inteiro (o render le a paleta em 0x1000+)
constexpr uint32_t kSoftReset = 0x008, kStartRender = 0x014, kListInit = 0x144, kListCont = 0x160;
constexpr uint32_t kId = 0x000, kRevision = 0x004, kSpgStatus = 0x10C, kYuvCount = 0x150;
constexpr uint32_t kIspBase = 0x128, kNextOpb = 0x134, kItpCurrent = 0x138, kNextOpbInit = 0x164;
constexpr uint32_t kListEndBit[5] = {1u << 7, 1u << 8, 1u << 9, 1u << 10, 1u << 21};  // ISTNRM, como pvr/ta.cpp
constexpr uint32_t kBlock = 32;  // granularidade das marcas de memoria escrita

// O fluxo de parametros do TA, dividido como pvr/ta.cpp (polygonFifo): parametros de 32 ou 64 bytes por lista. So a
// divisao (a mesma regra), sem interrupcoes nem estado de hardware: e o formato.
struct Params {
    int                       list = -1;
    bool                      vertex64 = false, pendingHalf = false;
    std::array<uint32_t, 8>   half{};
    uint32_t                  bytes = 0;  // recebidos desde o TA_LIST_INIT
    int                       ended = -1;  // lista fechada pelo ultimo parametro (fim de lista), senao -1
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
        ended = -1;
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
        case 0: ended = list, push(d, 8), list = -1; return true;  // fim de lista
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

// diagnostico: HH_KAMUI_CUSTO=<tsv> agrega os ciclos guest de cada chamada original (funil, bytes, alinhamentos, ciclos)
struct CostKey {
    uint32_t fn, n, dst, src;
    uint64_t cyc;
    bool     operator<(const CostKey& o) const {
        return std::tie(fn, n, dst, src, cyc) < std::tie(o.fn, o.n, o.dst, o.src, o.cyc);
    }
};
std::map<CostKey, uint64_t>& costs() {
    static std::map<CostKey, uint64_t> m;
    return m;
}
const char* costPath() {
    static const char* v = std::getenv("HH_KAMUI_CUSTO");
    return v && *v ? v : nullptr;
}
void callMeasured(Sh4Context* c, uint32_t addr, uint32_t n, uint32_t dst, uint32_t src) {
    if (!costPath()) return callOriginal(c, addr);
    const uint64_t before = c->cycles;
    callOriginal(c, addr);
    ++costs()[CostKey{addr, n, dst & 31, src & 31, c->cycles - before}];
}
void writeCosts() {
    if (!costPath()) return;
    if (FILE* f = std::fopen(costPath(), "w")) {
        std::fputs("funil\tbytes\tdst31\tsrc31\tciclos\tchamadas\n", f);
        for (const auto& [k, v] : costs())
            std::fprintf(f, "%08X\t%u\t%u\t%u\t%llu\t%llu\n", k.fn, k.n, k.dst, k.src, (unsigned long long)k.cyc,
                         (unsigned long long)v);
        std::fclose(f);
    }
}

// Empresta o estado nativo ao render e ao video do runtime enquanto `fn` roda.
template <class F> void withNative(F&& fn) {
    State& s = st();
    struct Restore {
        ~Restore() {
            pvr_regs_set_override(nullptr);
            vram_set_override(nullptr);
            ta_set_override(nullptr, nullptr);
        }
    } restore;
    // tempo do video (SPG, 0x0C8-0x0E0): continua no runtime (HH15), escrito tambem fora do funil (0x8C169FE0)
    for (uint32_t off = 0x0C8; off <= 0x0E0; off += 4) s.regs[off / 4] = pvr_reg(kRegBase + off);
    pvr_regs_set_override(s.regs.data());
    vram_set_override(s.mem.data());
    ta_set_override(&s.params.out, &s.params.bytes);
    fn();
}

// ---- modo nativo ----

// Tempo guest: o slot nativo segue o tempo do codigo gerado do original (HH_CYCLES de cada bloco em generated/, conferidos
// com a medicao HH_KAMUI_CUSTO no roteiro da area 1). Os lacos de copia e zeragem escrevem na mesma unidade, gastam os
// mesmos ciclos por volta e atendem os eventos vencidos no mesmo ponto (HH_POLL no back-edge, com o pc de retomada do
// laco), entao interrupcoes e trocas de thread no meio de uma carga longa veem o mesmo progresso. Sem isso o jogo anda mais
// depressa que no hardware e o roteiro diverge; o modelo de tempo proprio sai no HH15.
constexpr uint64_t kRegWriteCycles = 5, kRegReadCycles = 3, kLoopEpilogue = 2;

// Forma do laco que o original escolhe pelo alinhamento: unidade, ciclos ate a primeira volta, ciclos por volta e o pc do
// inicio do laco (retomada no atendimento).
struct LoopShape {
    uint32_t unit;
    uint64_t prologue, perIter;
    uint32_t resume;
};

// 0x8C159820: 5 na entrada; laco de 8 bytes (fmov duplo, 3 de preparo) se dst|src|n multiplo de 8, senao de 4 (2 + 1),
// 2 (2 + 2 + 1) ou 1 byte (2 + 2); 5 por volta; 2 no rts.
LoopShape copyShape(uint32_t dst, uint32_t src, uint32_t n) {
    const uint32_t x = dst | src | n;
    if (!(x & 7)) return {8, 5 + 3, 5, 0x8C159830};
    if (!(x & 3)) return {4, 5 + 2 + 1, 5, 0x8C159844};
    if (!(x & 1)) return {2, 5 + 2 + 2 + 1, 5, 0x8C159858};
    return {1, 5 + 2 + 2, 5, 0x8C159866};
}

// 0x8C159874: 4 na entrada; laco de 8 (5 de preparo), 4 (2 + 2), 2 (2 + 2 + 2) ou 1 byte (2 + 2 + 1) pelo alinhamento
// de dst|n; 4 por volta; 2 no rts.
LoopShape zeroShape(uint32_t dst, uint32_t n) {
    const uint32_t x = dst | n;
    if (!(x & 7)) return {8, 4 + 5, 4, 0x8C159886};
    if (!(x & 3)) return {4, 4 + 2 + 2, 4, 0x8C15989A};
    if (!(x & 1)) return {2, 4 + 2 + 2 + 2, 4, 0x8C1598B4};
    return {1, 4 + 2 + 2 + 1, 4, 0x8C1598C6};
}

uint64_t loopCycles(const LoopShape& l, uint32_t n) { return l.prologue + l.perIter * (n / l.unit) + kLoopEpilogue; }

// O laco: `body(i)` escreve a unidade i; atendimento no back-edge como HH_POLL (o original nao olha c->redirect no laco).
template <class F> void runLoop(Sh4Context* c, const LoopShape& l, uint32_t n, F&& body) {
    const uint32_t iters = n / l.unit;
    if (iters == 0) sh4_fatal(c, "copia/zeragem de 0 bytes pelo provedor nativo do Kamui (o original nao termina)", c->pc, n);
    const uint32_t pc = c->pc;
    c->cycles += l.prologue;
    for (uint32_t i = 0; i < iters; ++i) {
        c->cycles += l.perIter;
        body(i);
        if (i + 1 < iters) {
            ++c->polls;
            if (c->cycles >= c->next_event || c->polls >= c->next_poll) {  // a mesma condicao do HH_POLL (HH15.1)
                c->pc = l.resume;
                sh4_poll_slow(c);
            }
        }
    }
    c->cycles += kLoopEpilogue;
    if (!c->redirect) c->pc = pc;
}

void fRegWriteNative(Sh4Context* c) {
    State&         s   = st();
    const uint32_t off = c->r[4], val = c->r[5];
    ++s.stats.regWrites;
    if (off >= 0x2000 || (off & 3)) return callOriginal(c, kRegWrite);  // fora do banco: nao acontece no jogo
    c->cycles += kRegWriteCycles;
    c->r[0] = 1;  // o original devolve 1 (mov #1,r0 no slot do rts)
    if (off >= 0x0C8 && off <= 0x0E0) pvr_reg_write(c, kRegBase + off, 4, val);  // tempo do video (SPG): HH15
    if (off == kStartRender) {
        ++s.stats.nativeRenders;
        withNative([&] { pvr_start_render(c); });
        return;
    }
    if (off == kListInit) {
        if (val & 0x80000000u) {
            s.params.reset();
            s.regs[kNextOpb / 4]    = s.regs[kNextOpbInit / 4];
            s.regs[kItpCurrent / 4] = s.regs[kIspBase / 4];
        }
        return;
    }
    if (off == kListCont) s.params.list = -1;  // continua as listas atuais (como pvr/ta.cpp)
    if (off == kSoftReset || off == kSpgStatus || off == kId || off == kRevision || off == kYuvCount) return;
    s.regs[off / 4] = val;
}

void fRegReadNative(Sh4Context* c) {
    State&         s   = st();
    const uint32_t off = c->r[4];
    ++s.stats.regReads;
    if (off >= 0x2000 || (off & 3)) return callOriginal(c, kRegRead);
    c->cycles += kRegReadCycles;  // antes da leitura, como no original (SPG_STATUS depende do instante)
    uint32_t v = 0;
    if (off == kId) v = 0x17FD11DB;
    else if (off == kRevision) v = 0x00000011;
    else if (off == kSpgStatus) pvr_reg_read(c, kRegBase + off, 4, v);  // tempo do video (SPG): HH15
    else if (off == kYuvCount) v = 0;
    else v = s.regs[off / 4];
    GuestCall(c).ret(v);
}

// Destino nativo da area 4 (ta_set_sink): o que o DMA do canal 2 entregaria ao TA.
bool areaFourSink(Sh4Context* c, uint32_t target, const uint32_t* d) {
    State&         s      = st();
    const uint32_t p      = target & 0x1FFFFFFF;
    const uint32_t region = p & 0x01800000;
    if (region == 0) {
        if (!s.params.feed(d)) sh4_fatal(c, "parametro do TA com tipo invalido (provedor nativo do Kamui)", c->pc, d[0]);
        if (s.params.ended >= 0 && s.params.ended <= 4) {
            holly_intc_raise(kListEndBit[s.params.ended]);  // fim de lista: entrega das interrupcoes ate o HH15
            ++s.stats.listEnds;
        }
        s.stats.taBytes += 32;
        return true;
    }
    if (p & 0x01000000) {
        textureWrite(target, d);
        s.stats.textureBytes += 32;
        return true;
    }
    sh4_fatal(c, "conversor YUV pelo provedor nativo do Kamui (o jogo nao o usa)", c->pc, target);
}

void fDmaNative(Sh4Context* c) {
    ++st().stats.dmaTa;
    callOriginal(c, kDma);  // DMAC e SB do Holly (HH15); os dados vao para areaFourSink
}

void fCopyNative(Sh4Context* c) {
    State&         s   = st();
    const uint32_t dst = c->r[4], src = c->r[5], n = c->r[6];
    if (vramOffset(dst) < 0) return callOriginal(c, kCopy);
    const LoopShape l = copyShape(dst, src, n);
    runLoop(c, l, n, [&](uint32_t i) {
        const uint32_t o = i * l.unit;
        for (uint32_t b = 0; b < l.unit; ++b) memWrite(dst + o + b, sh4_read8(c, src + o + b));
    });
    ++s.stats.copies, s.stats.copyBytes += n;
}

void fZeroNative(Sh4Context* c) {
    State&         s   = st();
    const uint32_t dst = c->r[4], n = c->r[5];
    if (vramOffset(dst) < 0) return callOriginal(c, kZero);
    const LoopShape l = zeroShape(dst, n);
    // o valor: 0 no laco de 8 (fldi0); nos outros, o literal do original (mov.l @(disp,pc),r0 e a parte baixa no mov.w/b)
    const uint32_t v = l.unit == 8 ? 0 : sh4_read32(c, l.unit == 4 ? 0x8C1598A8 : l.unit == 2 ? 0x8C1598C0 : 0x8C1598D4);
    runLoop(c, l, n, [&](uint32_t i) {
        for (uint32_t b = 0; b < l.unit; ++b) memWrite(dst + i * l.unit + b, uint8_t(v >> (8 * (b & 3))));
    });
    ++s.stats.zeros, s.stats.zeroBytes += n;
}

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
    callMeasured(c, kRegWrite, off, 0, 0);
    if ((off == kStartRender || off == kListInit) && ordemLeft())
        std::fprintf(stderr, "[kamui] ordem: %s (TA do runtime %u -> %u bytes; nativo %u)\n",
                     off == kListInit ? "TA_LIST_INIT" : "STARTRENDER", before, ta_bytes(), s.params.bytes);
}

void fRegRead(Sh4Context* c) {
    ++st().stats.regReads;
    callMeasured(c, kRegRead, c->r[4], 0, 0);
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
        return callMeasured(c, kCopy, n, dst, src);
    }
    callOriginal(c, kCopy);
}

void fZero(Sh4Context* c) {
    State&         s   = st();
    const uint32_t dst = c->r[4], n = c->r[5];
    if (vramOffset(dst) >= 0) {
        for (uint32_t i = 0; i < n; ++i) memWrite(dst + i, 0);
        ++s.stats.zeros, s.stats.zeroBytes += n;
        return callMeasured(c, kZero, n, dst, 0);
    }
    callOriginal(c, kZero);
}

const std::pair<uint32_t, Sh4Func> kFns[] = {
    {kRegWrite, &fRegWrite}, {kRegRead, &fRegRead}, {kDma, &fDma}, {kCopy, &fCopy}, {kZero, &fZero}};
const std::pair<uint32_t, Sh4Func> kNativeFns[] = {{kRegWrite, &fRegWriteNative},
                                                   {kRegRead, &fRegReadNative},
                                                   {kDma, &fDmaNative},
                                                   {kCopy, &fCopyNative},
                                                   {kZero, &fZeroNative}};

} // namespace

uint64_t copy_cycles(uint32_t dst, uint32_t src, uint32_t n) { return loopCycles(copyShape(dst, src, n), n); }
uint64_t zero_cycles(uint32_t dst, uint32_t n) { return loopCycles(zeroShape(dst, n), n); }

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
    // o estado nativo parte do atual (o que o BIOS deixou na VRAM e nos registradores)
    std::memcpy(s.mem.data(), vram_data(), kVramSize);
    for (uint32_t i = 0; i < kRegCount; ++i) s.regs[i] = pvr_reg(kRegBase + 4 * i);
    if (m == Mode::Native) ta_set_sink(&areaFourSink);
    const auto& fns = m == Mode::Native ? kNativeFns : kFns;
    for (const auto& [addr, fn] : fns) {
        registry.add(addr, dcmap::Subsystem::Render, "kamui");
        registry.set_native(addr, fn);
        if (registry.select(addr, Provider::Native) != SelectResult::Ok)
            throw std::runtime_error("provedor nativo do Kamui: troca de provedor falhou");
    }
}

void uninstall(BoundaryRegistry& registry) {
    for (const auto& [addr, fn] : kFns) registry.select(addr, Provider::Legacy);
    ta_set_sink(nullptr);
    writeCosts();
    st().mode = Mode::Off;
}

bool installed() { return st().mode != Mode::Off; }
Mode mode() { return st().mode; }

void video_frame(std::vector<uint32_t>& argb, uint32_t& width, uint32_t& height) {
    withNative([&] { pvr_video_frame(argb, width, height); });
}

Stats stats() { return st().stats; }

std::string summary() {
    const Stats& s = st().stats;
    char         b[768];
    std::snprintf(b, sizeof b,
                  "[kamui] %s: renders %llu (nativos %llu, fins de lista %llu); registradores escritos %llu (divergentes %llu), lidos %llu; DMA TA %llu "
                  "(%llu bytes), textura %llu (%llu bytes), outros %llu; copias %llu (%llu bytes), zeragens %llu (%llu "
                  "bytes); parametros conferidos %llu (divergencias %llu); blocos de memoria conferidos %llu (divergentes "
                  "%llu)%s%s\n",
                  st().mode == Mode::Native ? "nativo" : "sombra", (unsigned long long)s.renders,
                  (unsigned long long)s.nativeRenders, (unsigned long long)s.listEnds, (unsigned long long)s.regWrites, (unsigned long long)s.regMismatch,
                  (unsigned long long)s.regReads, (unsigned long long)s.dmaTa, (unsigned long long)s.taBytes,
                  (unsigned long long)s.dmaTexture, (unsigned long long)s.textureBytes, (unsigned long long)s.dmaOther,
                  (unsigned long long)s.copies, (unsigned long long)s.copyBytes, (unsigned long long)s.zeros,
                  (unsigned long long)s.zeroBytes, (unsigned long long)s.paramsChecked,
                  (unsigned long long)s.paramMismatch, (unsigned long long)s.memBlocksChecked,
                  (unsigned long long)s.memMismatch, s.first.empty() ? "" : "; primeira: ", s.first.c_str());
    return b;
}

} // namespace hh::transitional::kamui
