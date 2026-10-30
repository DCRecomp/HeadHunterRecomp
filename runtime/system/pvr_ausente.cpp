// SPDX-License-Identifier: MIT
// HH15.7d4c: o lugar dos modelos graficos no produto (HeadHunter.exe). Compilado como fonte do proprio produto
// (app/CMakeLists.txt): as APIs dos modelos (pvr/pvr_regs.h, pvr/pvr_video.h, pvr/vram.h, pvr/ta.h) que o boot, a
// memoria, a store queue, o host e o provedor nativo do Kamui chamam, entao o linker nao puxa pvr_regs, pvr_render,
// pvr_video, pvr_texture, vram nem ta do hh_runtime (catraca hh15_produto_papeis). Contrato do guest medido (HH15.7,
// passo 0): so o SPG (system/spg_bus: leituras de SPG_STATUS e SPG_VBLANK, escritas 0x0C8-0x0E0). O resto do banco
// grafico, a VRAM (area 1) e a area 4 (TA e caminho direto de textura, pela store queue) sao erro explicito; os desenhos
// chegam pelo DMA do canal 2 nativo (system/dma_bus) ao provedor nativo do Kamui. O banco que o boot deixa
// (kBiosPvrState) fica guardado so para o provedor nativo do Kamui partir dele na instalacao (pvr_reg); sem VRAM, a
// memoria das texturas parte zerada (vram_data devolve nullptr), como a VRAM depois do reset.
#include "pvr/pvr_regs.h"
#include "pvr/pvr_video.h"
#include "pvr/ta.h"
#include "pvr/vram.h"

#include "cpu/sh4_runtime.h"
#include "system/render_bus.h"
#include "system/spg_bus.h"

#include <array>

namespace hh {
namespace {

constexpr uint32_t kBase = 0x005F8000;
constexpr uint32_t kSize = 0x2000;

std::array<uint32_t, kSize / 4> g_boot{};  // o banco deixado pelo boot (so para a instalacao do Kamui)

uint32_t canon(uint32_t a) { return a < 0xE0000000 ? (a & 0x1FFFFFFF) : 0; }
bool     inBank(uint32_t p) { return p >= kBase && p < kBase + kSize; }
bool     inVram(uint32_t a) { return a < 0xE0000000 && ((a & 0x1FFFFFFF) >> 26) == 1; }  // area 1

[[noreturn]] void outside(const char* what) { sh4_fatal(nullptr, what, 0, 0); }

} // namespace

// ---- registradores (pvr/pvr_regs.h) ----

void pvr_regs_reset() {
    g_boot.fill(0);
    render_bus_reset();  // nenhum fim de render pendente
    spg_bus_reset();
}

bool pvr_reg_read(Sh4Context* c, uint32_t a, unsigned size, uint32_t& v) {
    const uint32_t p = canon(a);
    if (!inBank(p) || size != 4) return false;
    const uint32_t off = p - kBase;
    if (off == kSpgStatusOff || spg_bus_holds(off)) {
        v = spg_bus_read(c, off);
        return true;
    }
    sh4_fatal(c, "registrador do PVR: leitura fora do contrato (so o SPG)", c->pc, a);
}

bool pvr_reg_write(Sh4Context* c, uint32_t a, unsigned size, uint32_t v) {
    const uint32_t p = canon(a);
    if (!inBank(p) || size != 4) return false;
    if (spg_bus_holds(p - kBase)) {
        spg_bus_write(c, p - kBase, v);
        return true;
    }
    if (p - kBase == kSpgStatusOff) return true;  // so leitura: a escrita nao guarda nada, como no modelo (revisao Codex)
    sh4_fatal(c, "registrador do PVR: escrita fora do contrato (so o SPG)", c->pc, a);
}

void pvr_regs_dump(Sh4Context* c, uint32_t* out) {
    for (uint32_t i = 0; i < kSize / 4; ++i) {
        const uint32_t off = 4 * i;
        out[i] = off == kSpgStatusOff ? spg_bus_read(c, off) : spg_bus_holds(off) ? spg_bus_reg(off) : g_boot[i];
    }
}

uint32_t pvr_reg(uint32_t a) {
    const uint32_t p = a & 0x1FFFFFFF;
    if (!inBank(p)) return 0;
    return spg_bus_holds(p - kBase) ? spg_bus_reg(p - kBase) : g_boot[(p - kBase) / 4];
}

uint32_t pvr_pal_entry(uint32_t) { outside("paleta do PVR pelo modelo: so na verificacao"); }

void pvr_regs_load(const uint32_t* words) {
    for (uint32_t i = 0; i < kSize / 4; ++i) g_boot[i] = words[i];
    spg_bus_load(words);  // a temporizacao e a fase do feixe deixadas pelo boot
}

void pvr_start_render(Sh4Context* c) {
    sh4_fatal(c, "STARTRENDER pelo caminho do TA: so na verificacao (o produto desenha pela IR do ListSink)", c->pc, 0);
}

void pvr_regs_set_override(const uint32_t* words) {
    if (words) outside("banco do PVR emprestado: so na verificacao");
}

// ---- saida de video do modelo (pvr/pvr_video.h) ----

void pvr_video_frame(std::vector<uint32_t>&, uint32_t&, uint32_t&) {
    outside("saida de video do modelo: so na verificacao (o produto exibe pelo servico nativo do Kamui)");
}

// ---- VRAM (pvr/vram.h) ----

uint8_t* vram_data() { return nullptr; }  // sem VRAM no produto

void vram_set_override(uint8_t* data) {
    if (data) outside("VRAM emprestada: so na verificacao");
}

void vram_reset() {}

bool vram_read(Sh4Context* c, uint32_t a, unsigned, uint32_t&) {
    if (!inVram(a)) return false;
    sh4_fatal(c, "VRAM (area 1): leitura fora do contrato", c->pc, a);
}

bool vram_write(Sh4Context* c, uint32_t a, unsigned, uint32_t) {
    if (!inVram(a)) return false;
    sh4_fatal(c, "VRAM (area 1): escrita fora do contrato", c->pc, a);
}

// ---- TA (pvr/ta.h) ----

void ta_reset() {}

const std::vector<TaParam>& ta_params() {
    static const std::vector<TaParam> none;
    return none;
}

uint32_t ta_bytes() { return 0; }

// area 4 pela store queue: fora do contrato (false: sh4_sq para com erro explicito)
bool ta_fifo_write(Sh4Context*, uint32_t, const uint32_t*) { return false; }

void ta_set_sink(TaSink) {}  // o provedor nativo do Kamui recebe pelo DMA do canal 2 (system/dma_bus)

void ta_set_override(const std::vector<TaParam>* params, const uint32_t*) {
    if (params) outside("parametros do TA emprestados: so na verificacao");
}

void ta_note_via(const char*) {}
void ta_note_bytes(Sh4Context*, const char*, uint32_t) {}

} // namespace hh
