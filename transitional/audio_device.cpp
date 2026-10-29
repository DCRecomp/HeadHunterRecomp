// SPDX-License-Identifier: MIT
#include "audio_device.h"

#include "audio/aica_channels.h"
#include "audio/aica_dsp.h"
#include "audio/audio.h"
#include "cpu/sh4_mem.h"
#include "cpu/sh4_runtime.h"
#include "cpu/time_hooks.h"
#include "mods/mod_api.h"
#include "pvr/holly_intc.h"

#include <algorithm>
#include <array>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <map>
#include <memory>
#include <optional>
#include <stdexcept>
#include <vector>

namespace hh::transitional::audio {

namespace {

namespace na = native::audio;

constexpr uint32_t kRamBase = 0x00800000, kRamSize = 0x00200000;  // RAM de som (2 MB, espelhada ate 0x00FFFFFF)
constexpr uint32_t kRegBase = 0x00700000, kRegSize = 0x00010000;  // registradores da AICA (canais, DSP, controle)
constexpr uint32_t kMcieb = 0x28B4, kMcipd = 0x28B8, kMcire = 0x28BC, kArmReset = 0x2C00;
// tabelas do driver na RAM de som (enderecos fixos da imagem do AUDIO64.DRV: palavras 0xC0-0xEC do cabecalho)
constexpr uint32_t kPosTable = 0xB34, kPrevTable = 0xC34, kEventTable = 0xD34, kRingPtr = 0xE4, kRingStart = 0xAEC,
                   kRingLast = 0xAEC + 0x3F, kVersionWord = 0xEC, kVersionCopy = 0xB408;
constexpr uint32_t kDriverVersion = 0x00030040;  // o que o boot do driver escreve em 0xEC e 0xB408 (0x144-0x154)
constexpr uint32_t kDefaultDspProgram = 0x237C, kDefaultDspProgramSize = 0x2FDC, kDefaultDspOutputs = 0xA64,
                   kDefaultDspOutputsSize = 0xAA4;
constexpr uint32_t kPosTablePtr = 0xE8;           // a palavra da imagem do driver com o endereco da tabela de posicoes
constexpr uint32_t kSystemInstalled = 0x8C154DB4;  // acSystemInstallDriver grava 1 no fim
constexpr uint32_t kSlotCounter = 0x8C154D7C;  // comandos enviados (o original incrementa a cada envio)
constexpr uint32_t kIstDmaEnd = 1u << 15;  // ISTNRM: fim do DMA do G2 para a AICA
constexpr uint32_t kQueueState  = 0x8C30F860;  // +20: indice de escrita na fila (o original avanca, modulo 32)
constexpr size_t   kPollSamples = 32;           // passada do driver: posicoes e eventos a cada 32 amostras (0,7 ms)

struct Port {
    na::AicaChannelParams shadow;  // a sombra do canal (0x117C + porta * 0x48 no driver)
    bool                  active = false, eventArmed = false;
    bool                  liveLoop = false;  // LPCTL do canal (o stop o limpa no canal, nao na sombra)
    uint32_t              eventTarget = 0;
};

na::AicaChannelParams channel_template() {
    na::AicaChannelParams p;  // modelo do driver (0x1134): DISDL 15, DIPAN 0x10, TL 0, sem envio
    p.disdl = 15, p.dipan = 0x10;
    return p;
}

struct State {
    bool                              on = false;
    std::vector<uint8_t>              ram = std::vector<uint8_t>(kRamSize);
    std::vector<uint32_t>             regs = std::vector<uint32_t>(kRegSize / 4);
    std::unique_ptr<na::AicaChannels> engine;
    std::array<Port, 64>              ports{};
    uint64_t                          rendered = 0;  // amostras ja produzidas (relogio do audio)
    bool                              clockSet = false;
    std::vector<int16_t>              pending;       // produzido e ainda nao entregue a saida
    std::optional<std::vector<uint8_t>> dspProgram, dspOutputs;
    Options                           opt;
    Stats                             stats;
    std::FILE*                        wav = nullptr;
    uint64_t                          wavFrames = 0;
    bool                              dmaBusy = false;  // DMA do G2 em andamento (o ADST do original)
    std::map<uint32_t, uint64_t>      readsByAddr;  // diagnostico: leituras do SH-4 na RAM de som
    std::map<uint32_t, uint64_t>      writesByRegion;  // diagnostico (HH_AUDIO_LOG): bytes escritos por regiao de 64 KB
};

State& st() {
    static State s;
    return s;
}

uint32_t le32(const std::vector<uint8_t>& m, uint32_t o) {
    o &= kRamSize - 1;
    return uint32_t(m[o]) | uint32_t(m[(o + 1) & (kRamSize - 1)]) << 8 | uint32_t(m[(o + 2) & (kRamSize - 1)]) << 16 |
           uint32_t(m[(o + 3) & (kRamSize - 1)]) << 24;
}
void put32(std::vector<uint8_t>& m, uint32_t o, uint32_t v) {
    for (int i = 0; i < 4; ++i) m[(o + uint32_t(i)) & (kRamSize - 1)] = uint8_t(v >> (8 * i));
}

// enderecos do SH-4 (P1/P2/fisico) -> RAM de som ou registradores
bool ram_offset(uint32_t a, uint32_t& off) {
    const uint32_t p = a & 0x1FFFFFFF;
    if (p < kRamBase || p >= 0x01000000) return false;
    off = (p - kRamBase) & (kRamSize - 1);
    return true;
}
bool reg_offset(uint32_t a, uint32_t& off) {
    const uint32_t p = a & 0x1FFFFFFF;
    if (p < kRegBase || p >= kRegBase + kRegSize) return false;
    off = p - kRegBase;
    return true;
}

void callOriginal(Sh4Context* c, uint32_t addr) { sh4_lookup_generated(addr)(c); }

// O outro lado de uma copia pode ser a propria RAM de som (o jogo transmite amostras longas residentes na RAM de som
// para um anel, de RAM de som para RAM de som: acG2Write(0xA0834040, 0xA0895C48, ...)): a RAM nativa, nunca a do runtime.
uint32_t guest_read32(Sh4Context* c, uint32_t a) {
    uint32_t off = 0;
    return ram_offset(a, off) ? le32(st().ram, off) : sh4_read32(c, a);
}
void guest_write32(Sh4Context* c, uint32_t a, uint32_t v) {
    uint32_t off = 0;
    if (ram_offset(a, off)) put32(st().ram, off, v);
    else sh4_write32(c, a, v);
}

void update_irq() {
    State& s = st();
    holly_intc_set_external(kIstExtAica, (s.regs[kMcipd / 4] & s.regs[kMcieb / 4] & 0x7FF) != 0);
}

// ---- motor e driver ----

void apply_live(int port) {
    State& s = st();
    s.engine->update(port, s.ports[size_t(port)].shadow);
}

void key_on(int port) {
    State& s = st();
    Port&  p = s.ports[size_t(port)];
    p.active   = true;
    p.liveLoop = p.shadow.loop;
    s.engine->key_on(port, p.shadow);
    // diagnostico: HH_AUDIO_LOG=<n> registra os n primeiros key-on (parametros do canal)
    static const int logMax = [] { const char* v = std::getenv("HH_AUDIO_LOG"); return v ? std::atoi(v) : 0; }();
    if (int(s.stats.keyOns) < logMax)
        std::fprintf(stderr,
                     "[audio] key-on (amostra %llu) porta %d: SA %06X PCMS %u laco %d LSA %u LEA %u OCT %d FNS %u TL %u "
                     "DISDL %u DIPAN %02X ISEL %u IMXL %u\n",
                     (unsigned long long)s.rendered, port, p.shadow.sa, p.shadow.pcms, p.shadow.loop, p.shadow.lsa, p.shadow.lea, p.shadow.oct, p.shadow.fns,
                     p.shadow.tl, p.shadow.disdl, p.shadow.dipan, p.shadow.isel, p.shadow.imxl);
    if (int(s.stats.keyOns) < logMax) {
        std::fprintf(stderr, "[audio]   RAM no SA:");
        for (uint32_t i = 0; i < 24; ++i) std::fprintf(stderr, " %02X", s.ram[(p.shadow.sa + i) & (kRamSize - 1)]);
        std::fputc('\n', stderr);
    }
    put32(s.ram, kPrevTable + 4 * uint32_t(port), 0xFFFFFFFFu);  // como o driver no key-on (0x3C0C-0x3C24)
    put32(s.ram, kPosTable + 4 * uint32_t(port), 0);
    ++s.stats.keyOns;
}

void push_event(int port) {
    State&   s   = st();
    uint32_t ptr = le32(s.ram, kRingPtr);
    s.ram[ptr & (kRamSize - 1)] = uint8_t(port);
    ptr = ptr + 1 > kRingLast ? kRingStart : ptr + 1;
    put32(s.ram, kRingPtr, ptr);
    s.regs[kMcipd / 4] |= 0x20;  // interrupcao ao SH-4 (0x4490-0x44C0)
    ++s.stats.events;
    update_irq();
}

// a passada do laco principal do driver (0x43BC): posicoes das portas ativas, fim sem laco, eventos
void driver_poll() {
    State& s = st();
    for (int port = 0; port < 64; ++port) {
        Port& p = s.ports[size_t(port)];
        if (!p.active) continue;
        const uint32_t cur  = s.engine->position(port);
        const uint32_t prev = le32(s.ram, kPosTable + 4 * uint32_t(port));
        put32(s.ram, kPrevTable + 4 * uint32_t(port), prev);
        if (cur != prev) put32(s.ram, kPosTable + 4 * uint32_t(port), cur);
        else if (!p.liveLoop) p.active = false;  // posicao parada sem LPCTL no canal (0x4598): key-off e porta inativa
    }
    // eventos: portas 31..0 e depois 63..32, como a varredura do driver (0x4410-0x44EC)
    for (int bank : {0, 32})
        for (int i = 31; i >= 0; --i) {
            const int port = bank + i;
            Port&     p    = s.ports[size_t(port)];
            if (!p.eventArmed) continue;
            const int32_t prev = int32_t(le32(s.ram, kPrevTable + 4 * uint32_t(port)));
            const int32_t cur  = int32_t(le32(s.ram, kPosTable + 4 * uint32_t(port)));
            const int32_t t    = int32_t(p.eventTarget);
            const bool    fire = prev < cur ? (t > prev && t <= cur) : (t < cur || t > prev);
            if (!fire) continue;
            p.eventArmed = false;
            push_event(port);
        }
}

void write_wav_header(std::FILE* f, uint64_t frames) {
    const uint32_t data = uint32_t(frames * 4), riff = 36 + data, fmt = 16, rate = 44100, bps = 44100 * 4;
    const uint16_t pcm = 1, ch = 2, align = 4, bits = 16;
    std::fseek(f, 0, SEEK_SET);
    std::fwrite("RIFF", 1, 4, f), std::fwrite(&riff, 4, 1, f), std::fwrite("WAVEfmt ", 1, 8, f);
    std::fwrite(&fmt, 4, 1, f), std::fwrite(&pcm, 2, 1, f), std::fwrite(&ch, 2, 1, f), std::fwrite(&rate, 4, 1, f);
    std::fwrite(&bps, 4, 1, f), std::fwrite(&align, 2, 1, f), std::fwrite(&bits, 2, 1, f);
    std::fwrite("data", 1, 4, f), std::fwrite(&data, 4, 1, f);
    std::fseek(f, 0, SEEK_END);
}

// avanca o motor ate o instante logico atual, com as passadas do driver no caminho
void advance(Sh4Context* c) {
    State&         s      = st();
    const uint64_t target = time_now(c) * na::AicaChannels::kRate / kCompatTicksPerSecond;
    if (!s.clockSet) s.rendered = target, s.clockSet = true;
    std::array<int16_t, 2 * kPollSamples> buf;
    while (s.rendered < target) {
        const size_t n = size_t(std::min<uint64_t>(kPollSamples - s.rendered % kPollSamples, target - s.rendered));
        s.engine->render(n, buf.data());
        s.rendered += n, s.stats.samples += n;
        if (s.wav) std::fwrite(buf.data(), sizeof(int16_t), 2 * n, s.wav), s.wavFrames += n;
        if (s.opt.output) {
            if (s.pending.size() < 2 * 44100 * 2) s.pending.insert(s.pending.end(), buf.begin(), buf.begin() + 2 * n);
            else s.stats.dropped += n;
        }
        if (s.rendered % kPollSamples == 0) driver_poll();
        // diagnostico: HH_AUDIO_RAM_EM=<amostra> grava a RAM de som (HH_AUDIO_RAM) nesse instante
        static const uint64_t dumpAt = [] { const char* v = std::getenv("HH_AUDIO_RAM_EM"); return v ? std::strtoull(v, nullptr, 10) : 0; }();
        if (dumpAt && s.rendered >= dumpAt && s.rendered - n < dumpAt)
            if (const char* d = std::getenv("HH_AUDIO_RAM"); d && *d)
                if (std::FILE* f = std::fopen(d, "wb")) std::fwrite(s.ram.data(), 1, s.ram.size(), f), std::fclose(f);
    }
}

void load_dsp() {
    State& s = st();
    static const bool noDsp = std::getenv("HH_AUDIO_SEM_DSP") != nullptr;  // diagnostico: sem o DSP de efeitos
    if (!s.dspProgram || !s.dspOutputs || noDsp) return;
    std::string err;
    auto        prog = na::parse_aica_dsp(*s.dspProgram, *s.dspOutputs, &err);
    if (!prog) {
        if (s.stats.first.empty()) s.stats.first = "programa do DSP recusado: " + err;
        return;
    }
    s.engine->set_dsp(std::make_shared<na::AicaDspProgram>(*prog));
}

std::vector<uint8_t> ram_bytes(uint32_t off, uint32_t n) {
    State&               s = st();
    std::vector<uint8_t> v(n);
    for (uint32_t i = 0; i < n; ++i) v[i] = s.ram[(off + i) & (kRamSize - 1)];
    return v;
}

int8_t sign4(uint32_t v) { return int8_t(int32_t(v << 28) >> 28); }

// pitch (0x3FA8): FNS somado com vai-um para o OCT; relativo a sombra ou absoluto
void pitch(int port, uint32_t dFns, int32_t dOct, bool absolute, bool store) {
    State&                s  = st();
    na::AicaChannelParams p  = s.ports[size_t(port)].shadow;
    uint32_t              fns = (absolute ? 0 : p.fns) + (dFns & 0x3FF);
    int32_t               oct = (absolute ? 0 : (p.oct & 15)) + dOct;
    if (fns >= 0x400) fns -= 0x400, ++oct;
    p.fns = uint16_t(fns & 0x3FF), p.oct = sign4(uint32_t(oct) & 15);
    if (store) s.ports[size_t(port)].shadow = p;
    s.engine->update(port, p);
}

uint8_t pan_code(uint8_t v) {  // 0x3F24: o pan do pacote para o DIPAN
    uint32_t r = v & 0x1F;
    if (r < 0x10) r = r == 0 ? 0xF : ((0u - r) & 0xF);
    return uint8_t(r);
}

void command(Sh4Context* c, const uint8_t* k) {
    State&         s    = st();
    const uint8_t  op   = k[0];
    const int      port = k[2] & 63;
    auto           u32  = [&](int o) { return uint32_t(k[o]) | uint32_t(k[o + 1]) << 8 | uint32_t(k[o + 2]) << 16 | uint32_t(k[o + 3]) << 24; };
    Port&          p    = s.ports[size_t(port)];
    // diagnostico: HH_AUDIO_RAM_CMD=<opcode hexa> grava a RAM de som (HH_AUDIO_RAM) antes do primeiro comando desse opcode
    static const int dumpOp = [] { const char* v = std::getenv("HH_AUDIO_RAM_CMD"); return v ? int(std::strtol(v, nullptr, 16)) : -1; }();
    if (op == dumpOp && s.stats.byOpcode[op] == 0)
        if (const char* d = std::getenv("HH_AUDIO_RAM"); d && *d)
            if (std::FILE* f = std::fopen(d, "wb")) std::fwrite(s.ram.data(), 1, s.ram.size(), f), std::fclose(f);
    ++s.stats.commands, ++s.stats.byOpcode[op];
    // diagnostico: HH_AUDIO_LOG_PORTA=<n> registra os pacotes da porta n (e os de MultiPlay/MultiStop)
    static const int logPort = [] { const char* v = std::getenv("HH_AUDIO_LOG_PORTA"); return v ? std::atoi(v) : -1; }();
    if (logPort >= 0 && (port == logPort || op == 0x9C || op == 0x9D)) {
        std::fprintf(stderr, "[audio] cmd %02X porta %d (amostra %llu) pr %08X:", op, port, (unsigned long long)s.rendered, c->pr);
        for (int i = 0; i < 16; ++i) std::fprintf(stderr, " %02X", k[i]);
        std::fputc('\n', stderr);
    }
    switch (op) {
    case 0x90: {  // acDigiOpen (0x3A90): SA, formato, fim, pitch
        p.shadow            = channel_template();
        const uint32_t size = u32(8), fmt = k[12] & 3;
        p.shadow.sa         = u32(4) & 0x7FFFFF;
        p.shadow.pcms       = uint8_t(fmt);
        p.shadow.lsa        = 0;
        p.shadow.lea        = (fmt == 0 ? size >> 1 : fmt == 1 ? size : size << 1) - 1;
        const uint32_t reg  = uint32_t(k[13]) << 8 | k[14];
        p.shadow.oct = sign4(reg >> 11), p.shadow.fns = uint16_t(reg & 0x3FF);
        break;
    }
    case 0x30: p.shadow.lsa = u32(4) & 0xFFFF, p.shadow.lea = u32(8) & 0xFFFF; break;  // acDigiSetLoopPoints (so a sombra)
    case 0x91: p.shadow = channel_template(); break;                        // acDigiClose
    case 0x92: p.eventTarget = u32(4), p.eventArmed = true; break;          // acDigiRequestEvent
    case 0x93: {                                                            // acDigiPlay
        p.shadow.loop = u32(8) & 1;
        if (const uint32_t lp = u32(12)) p.shadow.lea = lp >> 16, p.shadow.lsa = lp & 0xFFFF;
        key_on(port);
        break;
    }
    case 0x94: s.engine->key_off(port), p.liveLoop = false, p.eventArmed = false; break;  // acDigiStop (0x3E24)
    case 0x95:                                                              // acDspSetMixerChannel
        p.shadow.isel = k[3] & 15, p.shadow.imxl = k[4] & 15, p.shadow.disdl = k[5] & 15;
        apply_live(port);
        break;
    case 0x96: p.shadow.tl = k[3], apply_live(port); break;                 // acDigiSetVolume
    case 0x97: p.shadow.dipan = pan_code(k[3]), apply_live(port); break;    // acDigiSetPan
    case 0x98: pitch(port, u32(4), int32_t(u32(8)), false, false); break;   // acDigiSetCurrentPitch: so o canal
    case 0xAC: pitch(port, u32(4), int32_t(u32(8)), true, true); break;     // acDigiSetSampleRate
    case 0xAD: {                                                            // acDigiPlayWithParameters (0x4024)
        p.shadow.tl    = k[3];
        p.shadow.dipan = pan_code(k[4]);
        p.shadow.isel = k[5] & 15, p.shadow.imxl = (k[5] >> 4) & 15, p.shadow.disdl = k[10] & 15;
        if (const uint32_t ev = u32(8) & 0xFFFF) p.eventTarget = ev, p.eventArmed = true;
        const uint32_t lp = u32(12);
        p.shadow.loop     = lp != 0;
        if (lp) p.shadow.lea = lp >> 16, p.shadow.lsa = lp & 0xFFFF;
        key_on(port);
        if (const uint32_t pr = uint32_t(k[6]) << 8 | k[7])
            pitch(port, pr & 0x3FF, sign4(k[6] >> 3), (pr & 0x400) != 0, false);
        break;
    }
    case 0x9C:  // acDigiMultiPlay: as portas das duas mascaras de uma vez
    case 0x9D:  // acDigiMultiStop
        // a palavra em +4 cobre as portas 63..32 e a em +8 as 31..0 (bit i -> porta base + i; 0x3CD4 conta de 63 a 0)
        for (int w = 0; w < 2; ++w) {
            const uint32_t m = u32(4 + 4 * w);
            for (int i = 0; i < 32; ++i) {
                if (!(m >> i & 1)) continue;
                const int q = (w == 0 ? 32 : 0) + i;
                if (op == 0x9C) s.ports[size_t(q)].shadow.loop = k[2] & 1, key_on(q);
                else s.engine->key_off(q), s.ports[size_t(q)].liveLoop = false, s.ports[size_t(q)].eventArmed = false;
            }
        }
        break;
    case 0x81:  // acSystemSetMasterVolume (MVOL)
        s.regs[0x2800 / 4] = (s.regs[0x2800 / 4] & ~0xFu) | (k[2] & 15u);
        s.engine->set_master(uint8_t(k[2] & 15), (s.regs[0x2800 / 4] & 0x8000) != 0);
        break;
    case 0x8A:  // acSystemSetStereoOrMono (MONO)
        s.regs[0x2800 / 4] = (s.regs[0x2800 / 4] & ~0x8000u) | (k[2] ? 0x8000u : 0u);
        s.engine->set_master(uint8_t(s.regs[0x2800 / 4] & 15), k[2] != 0);
        break;
    case 0x9A:  // acDspInstallProgram: SFPB na RAM de som (0 = o padrao do driver)
    case 0x9B: {  // acDspInstallOutputMixer: SFOB
        uint32_t addr = u32(4), size = u32(8);
        if (!addr)
            addr = op == 0x9A ? kDefaultDspProgram : kDefaultDspOutputs,
            size = le32(s.ram, op == 0x9A ? kDefaultDspProgramSize : kDefaultDspOutputsSize);
        uint32_t off = 0;
        if (!ram_offset(addr, off)) off = addr & (kRamSize - 1);
        (op == 0x9A ? s.dspProgram : s.dspOutputs) = ram_bytes(off, std::min<uint32_t>(size, 0x4000));
        load_dsp();
        break;
    }
    default:
        ++s.stats.unknownCommands;
        if (s.stats.first.empty()) {
            char b[96];
            std::snprintf(b, sizeof b, "comando 0x%02X (porta %d) sem reconstrucao, pr %08X", op, port, c->pr);
            s.stats.first = b;
        }
        break;
    }
}

// ---- slots ----

void fHostCommand(Sh4Context* c) {
    State& s = st();
    advance(c);
    uint8_t k[16];
    for (uint32_t i = 0; i < 16; ++i) k[i] = sh4_read8(c, c->r[4] + i);
    // diagnostico (HH15.1, promocao do tempo logico): HH_AUDIO_CMDS=<arquivo> grava a sequencia de comandos ao driver
    // (VBlank e os 16 bytes do pacote), para comparar a ordem entre modos de tempo
    static std::FILE* cmdLog = [] { const char* v = std::getenv("HH_AUDIO_CMDS"); return v && *v ? std::fopen(v, "w") : nullptr; }();
    if (cmdLog) {
        std::fprintf(cmdLog, "%llu\t", static_cast<unsigned long long>(holly_vblank_count()));
        for (uint8_t b : k) std::fprintf(cmdLog, "%02X", b);
        std::fputc('\n', cmdLog);
        std::fflush(cmdLog);
    }
    command(c, k);
    // efeitos do original na RAM do SH-4: contador de comandos e indice de escrita da fila (modulo 32)
    sh4_write32(c, kSlotCounter, sh4_read32(c, kSlotCounter) + 1);
    sh4_write16(c, kQueueState + 20, uint16_t((sh4_read16(c, kQueueState + 20) + 1) & 31));
    c->r[0] = 1;
}

bool valid_block(uint32_t a, uint32_t b, uint32_t n) { return a && b && !((a | b | n) & 3); }

void fG2Write(Sh4Context* c) {  // (destino na RAM de som, origem, bytes)
    State&         s = st();
    const uint32_t dst = c->r[4], src = c->r[5], n = c->r[6];
    uint32_t       off = 0;
    if (!valid_block(dst, src, n) || !ram_offset(dst, off)) return callOriginal(c, kG2Write);  // erro do original
    advance(c);
    for (uint32_t i = 0; i < n; i += 4) put32(s.ram, off + i, guest_read32(c, src + i));
    ++s.stats.writes, s.stats.writeBytes += n;
    s.writesByRegion[off >> 16] += n;
    c->r[0] = 1;
}

void fG2Read(Sh4Context* c) {  // (destino, origem na RAM de som, bytes)
    State&         s = st();
    const uint32_t dst = c->r[4], src = c->r[5], n = c->r[6];
    uint32_t       off = 0;
    if (!valid_block(dst, src, n) || !ram_offset(src, off)) return callOriginal(c, kG2Read);
    advance(c);
    for (uint32_t i = 0; i < n; i += 4) guest_write32(c, dst + i, le32(s.ram, off + i));
    ++s.stats.reads, s.stats.readBytes += n;
    if (!s.opt.readsPath.empty()) ++s.readsByAddr[off];
    c->r[0] = 1;
}

void fG2Fill(Sh4Context* c) {  // (destino, byte, bytes)
    State&         s = st();
    const uint32_t dst = c->r[4], n = c->r[6];
    const uint8_t  v   = uint8_t(c->r[5]);
    uint32_t       off = 0;
    if (!dst || ((dst | n) & 3) || !ram_offset(dst, off)) return callOriginal(c, kG2Fill);
    advance(c);
    for (uint32_t i = 0; i < n; ++i) s.ram[(off + i) & (kRamSize - 1)] = v;
    ++s.stats.fills;
    c->r[0] = 1;
}

void fG2ReadLong(Sh4Context* c) {  // (endereco, saida)
    State&         s = st();
    const uint32_t a = c->r[4], out = c->r[5];
    if (!a || !out || (a & 3)) return callOriginal(c, kG2ReadLong);
    advance(c);
    uint32_t off = 0, v = 0;
    if (ram_offset(a, off)) {
        v = le32(s.ram, off);
        if (!s.opt.readsPath.empty()) ++s.readsByAddr[off];
    } else if (reg_offset(a, off)) {
        v = s.regs[off / 4];
    } else {
        return callOriginal(c, kG2ReadLong);
    }
    sh4_write32(c, out, v);
    ++s.stats.readLongs;
    c->r[0] = 1;
}

void fG2WriteLong(Sh4Context* c) {  // (endereco, valor)
    State&         s = st();
    const uint32_t a = c->r[4], v = c->r[5];
    if (!a || (a & 3)) return callOriginal(c, kG2WriteLong);
    advance(c);
    uint32_t off = 0;
    if (ram_offset(a, off)) {
        put32(s.ram, off, v);
    } else if (reg_offset(a, off)) {
        if (off == kMcire) {
            s.regs[kMcipd / 4] &= ~v;  // reconhecimento
        } else {
            const uint32_t old = s.regs[off / 4];
            s.regs[off / 4]    = v;
            // o ARM sai do reset (bit 0 de 0x2C00 de 1 para 0) com o driver carregado: o que o boot do driver escreve e
            // o SH-4 le (0x144-0x154); o laco de comandos e esta reconstrucao
            if (off == kArmReset && (old & 1) && !(v & 1)) {
                put32(s.ram, kVersionWord, kDriverVersion), put32(s.ram, kVersionCopy, kDriverVersion);
                s.regs[0x2800 / 4] = 15;  // o boot grava MVOL 15 em 0x802800 (0x120-0x128)
                s.engine->set_master(15, false);
            }
        }
        if (off == kMcieb || off == kMcire || off == kMcipd) update_irq();
    } else {
        return callOriginal(c, kG2WriteLong);
    }
    ++s.stats.writeLongs;
    c->r[0] = 1;
}

// Inicio do DMA do G2 para a AICA (0x8C123564), chamado por acTransfer no modo DMA e pelo tratador do fim do DMA
// (0x8C1234A2). O original, com o DMA livre (ADST == 0), pega a cabeca da fila de pedidos do Audio64 sem retira-la
// (0x8C123624(0): cauda em S+0xAC, entradas em S+0xB4) e programa ADSTAG = desc[0] (AICA), ADSTAR = desc[1] (RAM),
// ADLEN = desc[2], ADDIR = desc[3] (0 = para a AICA), ADTSEL 5, ADEN e ADST. O nativo copia o pedido na RAM de som nativa
// e levanta o fim do DMA (ISTNRM bit 15): o tratador do jogo retira o pedido, grava 802 em desc+16, chama o callback de
// desc+20 e inicia o proximo. "Ocupado" (o ADST do original) dura ate o jogo reconhecer o bit 15.
void fDmaStart(Sh4Context* c) {
    State& s = st();
    if (s.dmaBusy) {
        uint32_t ist = 0;
        holly_intc_read(c, 0x005F6900, 4, ist);
        if (ist & kIstDmaEnd) return;  // o fim do DMA anterior ainda nao foi atendido
        s.dmaBusy = false;
    }
    const uint32_t tail = sh4_read32(c, kQueueState + 0xAC) & 63;
    const uint32_t desc = sh4_read32(c, kQueueState + 0xB4 + 4 * tail);
    if (!desc) return;
    const uint32_t g2 = sh4_read32(c, desc), mem = sh4_read32(c, desc + 4), n = sh4_read32(c, desc + 8),
                   dir = sh4_read32(c, desc + 12);
    uint32_t       off = 0;
    if (!ram_offset(g2, off)) sh4_fatal(c, "DMA do G2 para fora da RAM de som (provedor nativo do audio)", c->pc, g2);
    advance(c);
    if (!dir)
        for (uint32_t i = 0; i < n; i += 4) put32(s.ram, off + i, guest_read32(c, (mem & 0x1FFFFFFF) + i));
    else
        for (uint32_t i = 0; i < n; i += 4) guest_write32(c, (mem & 0x1FFFFFFF) + i, le32(s.ram, off + i));
    if (!dir) ++s.stats.writes, s.stats.writeBytes += n, s.writesByRegion[off >> 16] += n;
    else ++s.stats.reads, s.stats.readBytes += n;
    ++s.stats.transfers;
    s.dmaBusy = true;
    holly_intc_raise(kIstDmaEnd);
}

// acDigiGetCurrentReadPosition(porta, saida): a posicao atual da porta na tabela que o driver publica (o ponteiro da
// tabela fica em 0xE8 da RAM de som). Com o sistema nao instalado (*0x8C154DB4 == 0) ou porta >= 64, o original avisa
// e devolve 0 sem tocar a RAM de som.
void fReadPosition(Sh4Context* c) {
    State&         s    = st();
    const uint32_t port = c->r[4], out = c->r[5];
    if (!sh4_read32(c, kSystemInstalled) || port >= 64) return callOriginal(c, kReadPosition);
    advance(c);
    const uint32_t v = le32(s.ram, le32(s.ram, kPosTablePtr) + 4 * port);
    sh4_write32(c, out, v);
    ++s.stats.readLongs;
    c->r[0] = 1;
}

void fDspClear(Sh4Context*) {  // zera os registradores do DSP (0x3400-0x3BFC: 0x200 palavras a partir do fim)
    State& s = st();
    for (uint32_t i = 0; i < 0x200; ++i) s.regs[(0x3BFC - 4 * i) / 4] = 0;
    // os registradores zerados sao o programa do DSP: sem programa, sem efeito (revisao Codex HH14)
    s.dspProgram.reset(), s.dspOutputs.reset();
    s.engine->set_dsp(nullptr);
}

const std::pair<uint32_t, Sh4Func> kFns[] = {{kHostCommand, &fHostCommand}, {kG2Write, &fG2Write},
                                             {kG2Read, &fG2Read},           {kG2Fill, &fG2Fill},
                                             {kG2ReadLong, &fG2ReadLong},   {kG2WriteLong, &fG2WriteLong},
                                             {kDspClear, &fDspClear},       {kReadPosition, &fReadPosition},
                                             {kDmaStart, &fDmaStart}};

} // namespace

void install(BoundaryRegistry& registry, const Options& o) {
    for (const auto& [addr, fn] : kFns)
        if (!sh4_lookup_generated(addr) || !mods::has_slot(addr)) {
            char hexa[16];
            std::snprintf(hexa, sizeof hexa, "%08X", addr);
            throw std::runtime_error(std::string("provedor nativo do audio: o funil ") + hexa +
                                     " nao tem slot (gere o codigo de novo com [native] slots)");
        }
    State& s = st();
    s        = State{};
    s.on     = true;
    s.opt    = o;
    s.engine = std::make_unique<na::AicaChannels>(s.ram);
    for (Port& p : s.ports) p.shadow = channel_template();
    s.engine->set_master(15, false);
    if (!o.wavPath.empty()) {
        s.wav = std::fopen(o.wavPath.c_str(), "wb");
        if (s.wav) write_wav_header(s.wav, 0);
    }
    if (o.output) o.output->open(44100, 2);
    for (const auto& [addr, fn] : kFns) {
        registry.add(addr, dcmap::Subsystem::Audio, "audio64");
        registry.set_native(addr, fn);
        if (registry.select(addr, Provider::Native) != SelectResult::Ok)
            throw std::runtime_error("provedor nativo do audio: troca de provedor falhou");
    }
}

void uninstall(BoundaryRegistry& registry) {
    State& s = st();
    for (const auto& [addr, fn] : kFns) registry.select(addr, Provider::Legacy);
    if (s.wav) write_wav_header(s.wav, s.wavFrames), std::fclose(s.wav), s.wav = nullptr;
    if (!s.opt.readsPath.empty())
        if (std::FILE* f = std::fopen(s.opt.readsPath.c_str(), "w")) {
            std::fputs("endereco\tleituras\n", f);
            for (const auto& [a, n] : s.readsByAddr) std::fprintf(f, "%06X\t%llu\n", a, (unsigned long long)n);
            std::fclose(f);
        }
    if (s.opt.output) s.opt.output->close();
    if (const char* d = std::getenv("HH_AUDIO_RAM"); d && *d && !std::getenv("HH_AUDIO_RAM_EM"))  // a RAM no fim
        if (std::FILE* f = std::fopen(d, "wb")) std::fwrite(s.ram.data(), 1, s.ram.size(), f), std::fclose(f);
    if (std::getenv("HH_AUDIO_LOG")) {  // diagnostico: os canais com mais energia
        std::vector<std::pair<double, int>> e;
        for (int ch = 0; ch < 64; ++ch) e.push_back({s.engine->energy(ch), ch});
        std::sort(e.rbegin(), e.rend());
        for (const auto& [r, n] : s.writesByRegion)
            std::fprintf(stderr, "[audio] escritas na regiao %06X: %llu bytes\n", r << 16, (unsigned long long)n);
        for (int i = 0; i < 8 && e[size_t(i)].first > 0; ++i)
            std::fprintf(stderr, "[audio] energia porta %d: %.3g\n", e[size_t(i)].second, e[size_t(i)].first);
    }
    holly_intc_set_external(kIstExtAica, false);
    s.on = false;
}

bool installed() { return st().on; }

Stats stats() { return st().stats; }

std::string summary() {
    const Stats& s = st().stats;
    std::string  ops;
    for (int i = 0; i < 256; ++i)
        if (s.byOpcode[i]) {
            char b[32];
            std::snprintf(b, sizeof b, " %02X:%llu", i, (unsigned long long)s.byOpcode[i]);
            ops += b;
        }
    char b[1024];
    std::snprintf(b, sizeof b,
                  "[audio] nativo: comandos %llu (desconhecidos %llu;%s), key-on %llu, eventos %llu; RAM de som: escritas %llu "
                  "(%llu bytes), leituras %llu (%llu bytes), preenchimentos %llu, DMA %llu, palavras lidas %llu/escritas %llu; "
                  "amostras %llu (%.1f s), descartadas %llu%s%s\n",
                  (unsigned long long)s.commands, (unsigned long long)s.unknownCommands, ops.c_str(),
                  (unsigned long long)s.keyOns, (unsigned long long)s.events, (unsigned long long)s.writes,
                  (unsigned long long)s.writeBytes, (unsigned long long)s.reads, (unsigned long long)s.readBytes,
                  (unsigned long long)s.fills, (unsigned long long)s.transfers, (unsigned long long)s.readLongs, (unsigned long long)s.writeLongs,
                  (unsigned long long)s.samples, double(s.samples) / 44100.0, (unsigned long long)s.dropped,
                  s.first.empty() ? "" : "; primeiro: ", s.first.c_str());
    return b;
}

void pump(Sh4Context* c) {
    State& s = st();
    if (!s.on) return;
    advance(c);
    if (!s.opt.output || s.pending.empty()) return;
    const size_t frames = std::min(s.pending.size() / 2, s.opt.output->writable_frames());
    if (frames) {
        s.opt.output->write(std::span<const int16_t>(s.pending.data(), 2 * frames));
        s.pending.erase(s.pending.begin(), s.pending.begin() + std::ptrdiff_t(2 * frames));
    }
}

} // namespace hh::transitional::audio
