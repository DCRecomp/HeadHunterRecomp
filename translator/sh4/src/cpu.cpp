// SPDX-License-Identifier: MIT
// Executor semantico do nucleo SH-4 - HH03.
//
// Uma semantica por (instrucao, gravura): despacho externo pelo enum Op da
// tabela gerada (equivalente ao mnemonico do oraculo) e interno pelo
// OperandFmt quando o mesmo opcode tem varios modos de acesso.
//
// Fontes de verdade (duas evidencias independentes nos pontos sensiveis):
//   - oraculo SLEIGH SuperH4 (SuperH4.sinc lido totalmente; inclusive a
//     nota "fmov so olha FPSCR_SZ");
//   - referencia MIT C:\Source\recomp-BStinger\runtime\cpu\sh4_ops.h.
//   - manual Renesas SH-4 para onde os dois divergem (documentado).
#include "sh4/cpu.hpp"

#include <bit>
#include <cfenv>
#include <cmath>
#include <cstring>

namespace hh::sh4 {

namespace {

// Enum Op + tabela declarativa gerada (mesmo conteudo incluido pelo
// decodificador; constexpr por TU, sem ODR).
#include "decoder_table.inc"

// ---------- conversores bit/float ----------
float f32(uint32_t u) { return std::bit_cast<float>(u); }
uint32_t u32(float f) { return std::bit_cast<uint32_t>(f); }
uint32_t sext8b(uint32_t v) { return uint32_t(int32_t(int8_t(v & 0xFF))); }
uint32_t sext16b(uint32_t v) { return uint32_t(int32_t(int16_t(v & 0xFFFF))); }

// ---------- flush de denormal (FPSCR.DN) ----------
float dn(float v, Sh4State& c) {
    if (!(c.fpscr & kFpscrDN)) return v;
    return std::fpclassify(v) == FP_SUBNORMAL ? (std::signbit(v) ? -0.0f : 0.0f)
                                              : v;
}
double dn_d(double v, Sh4State& c) {
    if (!(c.fpscr & kFpscrDN)) return v;
    return std::fpclassify(v) == FP_SUBNORMAL ? (std::signbit(v) ? -0.0 : 0.0)
                                              : v;
}

// ---------- modo de arredondamento (FPSCR.RM) ----------
// Aplica RM em volta da operacao; RM=0 (mais proximo) usa o modo do host
// direto. Requer fenv completo; sem ele RM e ignorado (limitacao
// documentada; o host HH05 define o modo por nucleo).
#if defined(FE_TOWARDZERO) && defined(FE_UPWARD) && defined(FE_DOWNWARD)
#define HH_FEENV_OK 1
#endif

#ifdef HH_FEENV_OK
template <typename F>
auto with_round(Sh4State& c, F&& fn) {
    const int rm = int(c.fpscr & kFpscrRM);
    if (rm == 0) return fn();
    const int saved = std::fegetround();
    const int fe = (rm == 1) ? FE_TOWARDZERO
                 : (rm == 2) ? FE_UPWARD
                             : FE_DOWNWARD;
    std::fesetround(fe);
    auto r = fn();
    std::fesetround(saved);
    return r;
}
#else
template <typename F>
auto with_round(Sh4State&, F&& fn) {
    return fn();
}
#endif

// ---------- saturacao ftrc (float/double -> int32, manual) ----------
uint32_t ftrc_sat(double v) {
    if (std::isnan(v)) return 0x80000000u;
    if (v >= 2147483648.0) return 0x7FFFFFFFu;
    if (v < -2147483648.0) return 0x80000000u;
    return uint32_t(int32_t(v));
}

// ---------- tabela FSCA (equivalente da tabela do hardware) ----------
const float* fsca_table() {
    static const float* table = [] {
        auto* t = new float[0x20000];
        for (uint32_t k = 0; k < 0x10000; ++k) {
            const double ang =
                double(k) * (6.283185307179586476925 / 65536.0);
            t[2 * k]     = float(std::sin(ang));
            t[2 * k + 1] = float(std::cos(ang));
        }
        return t;
    }();
    return table;
}

// ---------- registradores de controle ----------
// Codigo do creg extraido dos bits do opcode por familia; mapas verificados
// contra o oraculo op-a-op (analysis/hh03/query_cregs.py).
enum class Creg : int {
    none = -1,
    sr = 0, gbr, vbr, ssr, spc,   // codigo 0..4 nas familias stc/ldc
    sgr, dbr,                     // codigos especiais 0x3A/0xFA
    mach, macl, pr, fpul, fpscr,  // familias sts/lds
    bank0, bank1, bank2, bank3, bank4, bank5, bank6, bank7,
};

Creg bank(int idx) { return Creg(int(Creg::bank0) + (idx & 7)); }
Creg srfam_idx(int idx) { // 0..4 -> SR..SPC nas familias stc/ldc
    switch (idx & 7) {
    case 0: return Creg::sr;  case 1: return Creg::gbr;
    case 2: return Creg::vbr; case 3: return Creg::ssr;
    case 4: return Creg::spc; default: return Creg::none;
    }
}
Creg msfam_idx(int idx) { // MACH/MACL/PR/FPUL/FPSCR nas familias sts/lds
    switch (idx & 7) {
    case 0: return Creg::mach; case 1: return Creg::macl;
    case 2: return Creg::pr;   case 5: return Creg::fpul;
    case 6: return Creg::fpscr; default: return Creg::none;
    }
}

Creg creg_of(uint16_t op) {
    const unsigned lo = op & 0xF;
    const unsigned idx = (op >> 4) & 7;
    const bool b7 = (op & 0x80) != 0;
    switch (lo) {
    case 0x2: // st creg,rn (major 0) | stc.l SGR/DBR | sts.l creg (major 4)
        if ((op >> 12) == 0) return b7 ? bank(idx) : srfam_idx(idx);
        if (idx == 3) return Creg::sgr;       // 0x4032
        if ((op & 0xF0) == 0xF0) return Creg::dbr; // 0x4nF2 (STC.L DBR)
        return msfam_idx(idx);             // sts.l MACH/MACL/PR/FPUL/FPSCR
    case 0x3: // stc.l creg,@-Rn
        return b7 ? bank(idx) : srfam_idx(idx);
    case 0xA: // sts creg,rn (major 0) | stc SGR/DBR | lds creg (major 4)
        if ((op >> 12) == 0) {
            if (idx == 3) return Creg::sgr; // 0x003A
            if (b7)       return Creg::dbr; // 0x00FA
            return msfam_idx(idx);
        }
        return b7 ? Creg::dbr : msfam_idx(idx); // lds: DBR=0x40FA
    case 0xE: // ldc rn,creg
        return b7 ? bank(idx) : srfam_idx(idx);
    case 0x7: // ldc.l @Rm+,creg
        return b7 ? bank(idx) : srfam_idx(idx);
    case 0x6: // lds.l @Rm+,creg; 0x40F6 = DBR
        return b7 ? Creg::dbr : msfam_idx(idx);
    default:
        return Creg::none;
    }
}

uint32_t read_creg(Sh4State& c, Creg r) {
    switch (r) {
    case Creg::sr: return c.sr;
    case Creg::gbr: return c.gbr;  case Creg::vbr: return c.vbr;
    case Creg::ssr: return c.ssr;  case Creg::spc: return c.spc;
    case Creg::sgr: return c.sgr;  case Creg::dbr: return c.dbr;
    case Creg::mach: return c.mach; case Creg::macl: return c.macl;
    case Creg::pr: return c.pr;     case Creg::fpul: return c.fpul;
    case Creg::fpscr: return c.fpscr;
    case Creg::bank0:
    case Creg::bank1: case Creg::bank2: case Creg::bank3:
    case Creg::bank4: case Creg::bank5: case Creg::bank6: case Creg::bank7:
        return c.rbank[int(r) - int(Creg::bank0)];
    case Creg::none:
        break;
    }
    return 0;
}

void write_creg(Sh4State& c, Creg r, uint32_t v) {
    switch (r) {
    case Creg::sr: c.set_sr(v); return;
    case Creg::gbr: c.gbr = v; return;  case Creg::vbr: c.vbr = v; return;
    case Creg::ssr: c.ssr = v; return;  case Creg::spc: c.spc = v; return;
    case Creg::sgr: c.sgr = v; return;  case Creg::dbr: c.dbr = v; return;
    case Creg::mach: c.mach = v; return;
    case Creg::macl: c.macl = v; return;
    case Creg::pr: c.pr = v; return;
    case Creg::fpul: c.fpul = v; return;
    case Creg::fpscr: c.set_fpscr(v); return;
    case Creg::bank0:
    case Creg::bank1: case Creg::bank2: case Creg::bank3:
    case Creg::bank4: case Creg::bank5: case Creg::bank6: case Creg::bank7:
        c.rbank[int(r) - int(Creg::bank0)] = v;
        return;
    case Creg::none:
        return;
    }
}

uint32_t pop32(Sh4State& c, Sh4Bus& bus, unsigned m) {
    const uint32_t v = bus.read32(c.r[m]);
    c.r[m] += 4;
    return v;
}
void push32(Sh4State& c, Sh4Bus& bus, unsigned n, uint32_t v) {
    c.r[n] -= 4;
    bus.write32(c.r[n], v);
}

} // namespace

// ---------------------------------------------------------------- reset

void Sh4State::reset(uint32_t entry) {
    // Guarda os hooks do host; o resto volta ao estado de reset.
    void (*trapa)(Sh4State&, uint32_t) = on_trapa;
    void (*ldtlb)(Sh4State&) = on_ldtlb;
    void (*cache)(Sh4State&, uint32_t, uint32_t) = on_cache;
    void (*sleep)(Sh4State&) = on_sleep;
    void (*illegal)(Sh4State&, uint32_t, uint16_t) = on_illegal;
    *this = Sh4State{};
    pc = entry;
    on_trapa = trapa;
    on_ldtlb = ldtlb;
    on_cache = cache;
    on_sleep = sleep;
    on_illegal = illegal;
}

// Bancos de R0-R7: banco efetivo 1 so em modo privilegiado com RB=1
// (modelo da referencia MIT, coerente com o manual: os registradores
// Rn_BANK sao o banco NAO selecionado por RB).
void Sh4State::set_sr(uint32_t v) {
    v &= kSrWritable;
    const bool b1_old = (sr & kSrMD) && (sr & kSrRB);
    const bool b1_new = (v & kSrMD) && (v & kSrRB);
    if (b1_old != b1_new) {
        for (int i = 0; i < 8; ++i) std::swap(r[i], rbank[i]);
    }
    sr = v;
}

void Sh4State::set_fpscr(uint32_t v) {
    v &= kFpscrWritable;
    if (((v ^ fpscr) & kFpscrFR) != 0) {
        uint32_t tmp[16];
        std::memcpy(tmp, fr, sizeof(tmp));
        std::memcpy(fr, xf, sizeof(tmp));
        std::memcpy(xf, tmp, sizeof(tmp));
    }
    fpscr = v;
}

// ---------------------------------------------------------------- step

StepResult Sh4State::step(Sh4Bus& bus) {
    StepResult out;
    const uint32_t addr = pc;
    const bool in_delay = in_delay_;
    const uint32_t pending = branch_target_;
    in_delay_ = false;

    out.addr = addr;
    const uint16_t op = bus.read16(addr);
    out.opcode = op;

    const InstrInfo* row = lookup(op);
    if (row == nullptr) {
        // HH05+: excecao de instrucao ilegal (SSR/SPC/VBR). HH03 registra.
        out.illegal = true;
        if (on_illegal) on_illegal(*this, addr, op);
        return out;
    }

    const Decoded d = decode(op);
    std::optional<uint32_t> taken = execute(bus, addr, *row, d);

    if (in_delay) {
        // Slot ja rodou: o alvo do desvio original vence (desvio no slot e
        // indefinido pelo manual; nossa escolha: alvo externo).
        pc = pending;
    } else if (taken) {
        if (row->has_delay_slot) {
            // Alvo travado ANTES do slot; o slot roda em pc+2 e so entao o
            // PC salta.
            in_delay_ = true;
            branch_target_ = *taken;
            pc = addr + 2;
        } else {
            // bt/bf: desvio imediato, sem delay slot.
            pc = *taken;
        }
    } else {
        pc = addr + 2;
    }
    return out;
}

// ---------------------------------------------------------------- execute

std::optional<uint32_t> Sh4State::execute(Sh4Bus& bus, uint32_t addr,
                                          const InstrInfo& row,
                                          const Decoded& d) {
    using Opt = std::optional<uint32_t>;
    const unsigned n = d.rn;
    const unsigned m = d.rm;
    const uint16_t op = d.op;

    switch (row.op) {
    // ---------- transferencias ----------
    case Op::OP_NOP:
        return Opt{};
    case Op::OP_MOV:
        switch (row.fmt) {
        case OperandFmt::rm_rn: r[n] = r[m]; return Opt{};
        case OperandFmt::imm_rn: r[n] = uint32_t(d.imm8); return Opt{};
        default: break;
        }
        break;
    case Op::OP_MOVA:
        r[0] = (addr & ~3u) + 4 + 4u * (op & 0xFF);
        return Opt{};
    case Op::OP_MOVT:
        r[n] = uint32_t(t());
        return Opt{};

    case Op::OP_MOV_B:
    case Op::OP_MOV_W:
    case Op::OP_MOV_L: {
        const unsigned unit = row.op == Op::OP_MOV_B   ? 1
                              : row.op == Op::OP_MOV_W ? 2
                                                       : 4;
        switch (row.fmt) {
        case OperandFmt::m2r_at_rm:
            if (unit == 1) r[n] = sext8b(bus.read8(r[m]));
            else if (unit == 2) r[n] = sext16b(bus.read16(r[m]));
            else r[n] = bus.read32(r[m]);
            return Opt{};
        case OperandFmt::m2r_at_rm_pos: {
            const uint32_t a = r[m];
            if (unit == 1) { r[n] = sext8b(bus.read8(a)); r[m] = a + 1; }
            else if (unit == 2) {
                r[n] = sext16b(bus.read16(a));
                r[m] = a + 2;
            } else { r[n] = bus.read32(a); r[m] = a + 4; }
            return Opt{};
        }
        case OperandFmt::m2r_at_r0_rm:
            if (unit == 1) r[n] = sext8b(bus.read8(r[m] + r[0]));
            else if (unit == 2) r[n] = sext16b(bus.read16(r[m] + r[0]));
            else r[n] = bus.read32(r[m] + r[0]);
            return Opt{};
        case OperandFmt::m2r_at_disp_rm: {
            const uint32_t a = r[m] + unit * (op & 0xFu);
            if (unit == 1) r[n] = sext8b(bus.read8(a));
            else if (unit == 2) r[n] = sext16b(bus.read16(a));
            else r[n] = bus.read32(a);
            return Opt{};
        }
        case OperandFmt::m2r_at_disp_rm_r0: {
            // Somente mov.b/@(A,rM) e mov.w/@(A,rM): bits 11-8 sao o nibble
            // de tamanho (mask FF00), destino = R0 fixo (oraculo 0x84E0,
            // 0x8501: destino r0), endereco em Rm (bits 7-4).
            const uint32_t a = r[m] + unit * (op & 0xFu);
            if (unit == 1) r[0] = sext8b(bus.read8(a));
            else r[0] = sext16b(bus.read16(a));
            return Opt{};
        }
        case OperandFmt::m2r_at_disp_gbr: {
            const uint32_t addr2 = gbr + unit * (op & 0xFFu);
            if (unit == 1) r[0] = sext8b(bus.read8(addr2));
            else if (unit == 2) r[0] = sext16b(bus.read16(addr2));
            else r[0] = bus.read32(addr2);
            return Opt{};
        }
        case OperandFmt::movw_pc:
            r[n] = sext16b(bus.read16(addr + 4 + 2u * (op & 0xFF)));
            return Opt{};
        case OperandFmt::movl_pc:
            r[n] = bus.read32((addr & ~3u) + 4 + 4u * (op & 0xFF));
            return Opt{};
        case OperandFmt::r2m_at_rn:
            if (unit == 1) bus.write8(r[n], uint8_t(r[m]));
            else if (unit == 2) bus.write16(r[n], uint16_t(r[m]));
            else bus.write32(r[n], r[m]);
            return Opt{};
        case OperandFmt::r2m_at_rn_neg: {
            const uint32_t a = r[n] - unit;
            if (unit == 1) bus.write8(a, uint8_t(r[m]));
            else if (unit == 2) bus.write16(a, uint16_t(r[m]));
            else bus.write32(a, r[m]);
            r[n] = a;
            return Opt{};
        }
        case OperandFmt::r2m_r0_rn:
            if (unit == 1) bus.write8(r[n] + r[0], uint8_t(r[m]));
            else if (unit == 2) bus.write16(r[n] + r[0], uint16_t(r[m]));
            else bus.write32(r[n] + r[0], r[m]);
            return Opt{};
        case OperandFmt::r2m_disp_rn: {
            const uint32_t a = r[n] + unit * (op & 0xFu);
            if (unit == 1) bus.write8(a, uint8_t(r[m]));
            else if (unit == 2) bus.write16(a, uint16_t(r[m]));
            else bus.write32(a, r[m]);
            return Opt{};
        }
        case OperandFmt::r2m_disp_rn_r0: {
            // mov.b R0,@(disp,Rm) / mov.w R0,@(disp,Rm): bits 11-8 sao o
            // nibble de tamanho (oraculo 0x80E0/0x8DE1: endereco r14 em
            // bits 7-4), dado = R0 fixo.
            const uint32_t a = r[m] + unit * (op & 0xFu);
            if (unit == 1) bus.write8(a, uint8_t(r[0]));
            else bus.write16(a, uint16_t(r[0]));
            return Opt{};
        }
        case OperandFmt::r2m_disp_gbr: {
            const uint32_t addr2 = gbr + unit * (op & 0xFFu);
            if (unit == 1) bus.write8(addr2, uint8_t(r[0]));
            else if (unit == 2) bus.write16(addr2, uint16_t(r[0]));
            else bus.write32(addr2, r[0]);
            return Opt{};
        }
        default:
            break;
        }
        break; // nunca: LINHA com fmt fora do enumerado acima e bug do atlas
    }

    // binarias @(R0,GBR) de byte
    case Op::OP_AND_B:
    case Op::OP_OR_B:
    case Op::OP_XOR_B:
    case Op::OP_TST_B: {
        const uint32_t a = gbr + r[0];
        const uint8_t v = bus.read8(a);
        switch (row.op) {
        case Op::OP_AND_B: bus.write8(a, uint8_t(v & (op & 0xFF))); break;
        case Op::OP_OR_B:  bus.write8(a, uint8_t(v | (op & 0xFF))); break;
        case Op::OP_XOR_B: bus.write8(a, uint8_t(v ^ (op & 0xFF))); break;
        default:           set_t((v & (op & 0xFF)) == 0); break;
        }
        return Opt{};
    }

    case Op::OP_MAC_L: {
        // Ordem do manual: le @Rn+ primeiro, depois @Rm+ (importa com n==m).
        const int32_t a = int32_t(bus.read32(r[n]));
        r[n] += 4;
        const int32_t b = int32_t(bus.read32(r[m]));
        r[m] += 4;
        int64_t acc = int64_t((uint64_t(mach) << 32) | macl) +
                      int64_t(a) * int64_t(b);
        if (sr & kSrS) { // saturacao em 48 bits
            constexpr int64_t kMax = 0x00007FFFFFFFFFFFll;
            constexpr int64_t kMin = -0x0000800000000000ll;
            if (acc > kMax) acc = kMax;
            if (acc < kMin) acc = kMin;
        }
        mach = uint32_t(uint64_t(acc) >> 32);
        macl = uint32_t(acc);
        return Opt{};
    }
    case Op::OP_MAC_W: {
        const int16_t a = int16_t(bus.read16(r[n]));
        r[n] += 2;
        const int16_t b = int16_t(bus.read16(r[m]));
        r[m] += 2;
        const int64_t prod = int64_t(a) * int64_t(b); // M x N (bug da
                                                      // spec: tmpN*tmpN)
        if (sr & kSrS) { // saturacao em 32 bits sobre MACL; bit0 = overflow
            const int64_t acc = int64_t(int32_t(macl)) + prod;
            if (acc > 0x7FFFFFFFll) { mach |= 1; macl = 0x7FFFFFFF; }
            else if (acc < -0x80000000ll) {
                mach |= 1;
                macl = 0x80000000u;
            } else {
                macl = uint32_t(acc);
            }
        } else {
            const int64_t acc = int64_t((uint64_t(mach) << 32) | macl) + prod;
            mach = uint32_t(uint64_t(acc) >> 32);
            macl = uint32_t(acc);
        }
        return Opt{};
    }

    case Op::OP_MOVCA_L:
        bus.write32(r[n], r[0]);
        return Opt{};

    // ---------- aritmetica ----------
    case Op::OP_ADD:
        switch (row.fmt) {
        case OperandFmt::rm_rn: r[n] = r[n] + r[m]; return Opt{};
        case OperandFmt::imm_rn: r[n] = r[n] + uint32_t(d.imm8); return Opt{};
        default: break;
        }
        break;
    case Op::OP_ADDC: {
        const uint32_t a = r[n];
        const uint32_t s = a + r[m];
        const uint32_t res = s + uint32_t(t());
        set_t((a > s) || (s > res));
        r[n] = res;
        return Opt{};
    }
    case Op::OP_ADDV: {
        const uint32_t a = r[n], b = r[m], res = a + b;
        set_t((~(a ^ b) & (a ^ res)) >> 31);
        r[n] = res;
        return Opt{};
    }
    case Op::OP_SUB: r[n] = r[n] - r[m]; return Opt{};
    case Op::OP_SUBC: {
        const uint32_t a = r[n];
        const uint32_t s = a - r[m];
        const uint32_t res = s - uint32_t(t());
        set_t((a < s) || (s < res));
        r[n] = res;
        return Opt{};
    }
    case Op::OP_SUBV: {
        const uint32_t a = r[n], b = r[m], res = a - b;
        set_t(((a ^ b) & (a ^ res)) >> 31);
        r[n] = res;
        return Opt{};
    }
    case Op::OP_NEG: r[n] = 0u - r[m]; return Opt{};
    case Op::OP_NEGC: {
        const uint32_t t1 = 0u - r[m];
        const uint32_t res = t1 - uint32_t(t());
        set_t((0u < t1) || (t1 < res));
        r[n] = res;
        return Opt{};
    }
    case Op::OP_MUL_L: macl = r[n] * r[m]; return Opt{};
    case Op::OP_MULS_W:
        macl = uint32_t(int32_t(int16_t(r[n] & 0xFFFF)) *
                        int32_t(int16_t(r[m] & 0xFFFF)));
        return Opt{};
    case Op::OP_MULU_W:
        macl = uint32_t(uint16_t(r[n] & 0xFFFF) * uint16_t(r[m] & 0xFFFF));
        return Opt{};
    case Op::OP_DMULS_L: {
        const int64_t v = int64_t(int32_t(r[n])) * int64_t(int32_t(r[m]));
        mach = uint32_t(uint64_t(v) >> 32);
        macl = uint32_t(v);
        return Opt{};
    }
    case Op::OP_DMULU_L: {
        const uint64_t v = uint64_t(r[n]) * uint64_t(r[m]);
        mach = uint32_t(v >> 32);
        macl = uint32_t(v);
        return Opt{};
    }
    case Op::OP_DIV0S:
        set_sr_bit(kSrQ, (r[n] >> 31) != 0);
        set_sr_bit(kSrM, (r[m] >> 31) != 0);
        set_t(((sr & kSrQ) != 0) != ((sr & kSrM) != 0));
        return Opt{};
    case Op::OP_DIV0U:
        set_sr_bit(kSrQ, false);
        set_sr_bit(kSrM, false);
        set_t(false);
        return Opt{};
    case Op::OP_DIV1: {
        const bool old_q = (sr & kSrQ) != 0;
        const bool m_bit = (sr & kSrM) != 0;
        bool q = (r[n] & 0x80000000u) != 0;
        const uint32_t divisor = r[m];
        uint32_t rnv = (r[n] << 1) | uint32_t(t());
        const uint32_t before = rnv;
        bool carry;
        if (!old_q) {
            if (!m_bit) {
                rnv -= divisor;
                carry = rnv > before;
                q = q ? !carry : carry;
            } else {
                rnv += divisor;
                carry = rnv < before;
                q = q ? carry : !carry;
            }
        } else {
            if (!m_bit) {
                rnv += divisor;
                carry = rnv < before;
                q = q ? !carry : carry;
            } else {
                rnv -= divisor;
                carry = rnv > before;
                q = q ? carry : !carry;
            }
        }
        set_sr_bit(kSrQ, q);
        set_t(q == m_bit);
        r[n] = rnv;
        return Opt{};
    }
    case Op::OP_DT:
        r[n] -= 1;
        set_t(r[n] == 0); // setado apos o decremento
        return Opt{};
    case Op::OP_SWAP_B: {
        const uint32_t v = r[m];
        r[n] = (v & 0xFFFF0000u) | ((v & 0xFF) << 8) | ((v >> 8) & 0xFF);
        return Opt{};
    }
    case Op::OP_SWAP_W: r[n] = (r[m] << 16) | (r[m] >> 16); return Opt{};
    case Op::OP_XTRCT: r[n] = (r[m] << 16) | (r[n] >> 16); return Opt{};

    // ---------- comparacoes ----------
    case Op::OP_CMP_EQ:
        switch (row.fmt) {
        case OperandFmt::rm_rn: set_t(r[n] == r[m]); return Opt{};
        case OperandFmt::imm_rn: set_t(r[0] == uint32_t(d.imm8)); return Opt{};
        default: break;
        }
        break;
    case Op::OP_CMP_HS: set_t(r[n] >= r[m]); return Opt{};
    case Op::OP_CMP_GE: set_t(int32_t(r[n]) >= int32_t(r[m])); return Opt{};
    case Op::OP_CMP_HI: set_t(r[n] > r[m]); return Opt{};
    case Op::OP_CMP_GT: set_t(int32_t(r[n]) > int32_t(r[m])); return Opt{};
    case Op::OP_CMP_PZ: set_t(int32_t(r[n]) >= 0); return Opt{};
    case Op::OP_CMP_PL: set_t(int32_t(r[n]) > 0); return Opt{};
    case Op::OP_CMP_STR: {
        const uint32_t x = r[n] ^ r[m];
        set_t(!(x & 0xFF000000u) || !(x & 0x00FF0000u) ||
              !(x & 0x0000FF00u) || !(x & 0x000000FFu));
        return Opt{};
    }
    case Op::OP_TST:
        switch (row.fmt) {
        case OperandFmt::rm_rn: set_t((r[n] & r[m]) == 0); return Opt{};
        case OperandFmt::imm_rn: set_t((r[0] & uint32_t(d.imm8)) == 0); return Opt{};
        default: break;
        }
        break;
    case Op::OP_AND:
        switch (row.fmt) {
        case OperandFmt::rm_rn: r[n] = r[n] & r[m]; return Opt{};
        case OperandFmt::imm_rn: r[0] = r[0] & uint32_t(d.imm8); return Opt{};
        default: break;
        }
        break;
    case Op::OP_OR:
        switch (row.fmt) {
        case OperandFmt::rm_rn: r[n] = r[n] | r[m]; return Opt{};
        case OperandFmt::imm_rn: r[0] = r[0] | uint32_t(d.imm8); return Opt{};
        default: break;
        }
        break;
    case Op::OP_XOR:
        switch (row.fmt) {
        case OperandFmt::rm_rn: r[n] = r[n] ^ r[m]; return Opt{};
        case OperandFmt::imm_rn: r[0] = r[0] ^ uint32_t(d.imm8); return Opt{};
        default: break;
        }
        break;
    case Op::OP_NOT: r[n] = ~r[m]; return Opt{};
    case Op::OP_EXTU_B: r[n] = r[m] & 0xFF; return Opt{};
    case Op::OP_EXTU_W: r[n] = r[m] & 0xFFFF; return Opt{};
    case Op::OP_EXTS_B: r[n] = sext8b(r[m]); return Opt{};
    case Op::OP_EXTS_W: r[n] = sext16b(r[m]); return Opt{};

    // ---------- deslocamentos ----------
    case Op::OP_SHLL: set_t(r[n] >> 31); r[n] = r[n] << 1; return Opt{};
    case Op::OP_SHLR: set_t(r[n] & 1); r[n] = r[n] >> 1; return Opt{};
    case Op::OP_SHLL2: r[n] = r[n] << 2; return Opt{};
    case Op::OP_SHLL8: r[n] = r[n] << 8; return Opt{};
    case Op::OP_SHLL16: r[n] = r[n] << 16; return Opt{};
    case Op::OP_SHLR2: r[n] = r[n] >> 2; return Opt{};
    case Op::OP_SHLR8: r[n] = r[n] >> 8; return Opt{};
    case Op::OP_SHLR16: r[n] = r[n] >> 16; return Opt{};
    case Op::OP_SHAL: set_t(r[n] >> 31); r[n] = r[n] << 1; return Opt{};
    case Op::OP_SHAR:
        set_t(r[n] & 1);
        r[n] = uint32_t(int32_t(r[n]) >> 1);
        return Opt{};
    case Op::OP_SHAD: {
        const int32_t s = int32_t(r[m]);
        if (s >= 0) {
            r[n] = uint32_t(r[n]) << (s & 31);
        } else {
            const int k = (~s & 31) + 1;
            r[n] = k >= 32 ? uint32_t(int32_t(r[n]) >> 31) // k=32 (s=-32):
                            : uint32_t(int32_t(r[n]) >> k);
        }
        return Opt{};
    }
    case Op::OP_SHLD: {
        const int32_t s = int32_t(r[m]);
        if (s >= 0) {
            r[n] = r[n] << (s & 31);
        } else {
            const int k = (~s & 31) + 1;
            // k va de 1 a 32; deslocamento logico 32 = 0 (semantica da spec)
            r[n] = k >= 32 ? 0u : r[n] >> k;
        }
        return Opt{};
    }
    case Op::OP_ROTL: {
        const uint32_t v = r[n];
        r[n] = (v << 1) | uint32_t(t());
        set_t(v >> 31);
        return Opt{};
    }
    case Op::OP_ROTR: {
        const uint32_t v = r[n];
        r[n] = (v >> 1) | (uint32_t(t()) << 31);
        set_t(v & 1);
        return Opt{};
    }
    case Op::OP_ROTCL: {
        const uint32_t v = r[n];
        r[n] = (v << 1) | uint32_t(t());
        set_t(v >> 31);
        return Opt{};
    }
    case Op::OP_ROTCR: {
        const uint32_t v = r[n];
        r[n] = (v >> 1) | (uint32_t(t()) << 31);
        set_t(v & 1);
        return Opt{};
    }

    // ---------- controle ----------
    case Op::OP_CLRMAC: mach = 0; macl = 0; return Opt{};
    case Op::OP_CLRS: set_sr_bit(kSrS, false); return Opt{};
    case Op::OP_CLRT: set_t(false); return Opt{};
    case Op::OP_SETS: set_sr_bit(kSrS, true); return Opt{};
    case Op::OP_SETT: set_t(true); return Opt{};
    case Op::OP_LDTLB: if (on_ldtlb) on_ldtlb(*this); return Opt{};
    case Op::OP_SLEEP: if (on_sleep) on_sleep(*this); return Opt{};
    case Op::OP_TRAPA:
        if (on_trapa) on_trapa(*this, uint32_t(d.imm8 & 0xFF));
        return Opt{};

    // registradores de controle
    case Op::OP_STC:
        switch (row.fmt) {
        case OperandFmt::stc_creg: r[n] = read_creg(*this, creg_of(op)); return Opt{};
        case OperandFmt::stc_bank: r[n] = rbank[(op >> 4) & 7]; return Opt{};
        default: break;
        }
        break;
    case Op::OP_STS:
        r[n] = read_creg(*this, creg_of(op));
        return Opt{};
    case Op::OP_STC_L:
        switch (row.fmt) {
        case OperandFmt::stcl_creg:
            push32(*this, bus, n, read_creg(*this, creg_of(op)));
            return Opt{};
        case OperandFmt::stcl_bank:
            push32(*this, bus, n, rbank[(op >> 4) & 7]);
            return Opt{};
        default: break;
        }
        break;
    case Op::OP_STS_L:
        push32(*this, bus, n, read_creg(*this, creg_of(op)));
        return Opt{};
    case Op::OP_LDC:
        // Fonte = bits 11-8 (padrao MIT "0100mmmm...." + mascaras SLEIGH;
        // bits 7-4 sao o indice do creg, nao registrador).
        switch (row.fmt) {
        case OperandFmt::ldc_creg: write_creg(*this, creg_of(op), r[n]); return Opt{};
        case OperandFmt::ldc_bank: rbank[(op >> 4) & 7] = r[n]; return Opt{};
        default: break;
        }
        break;
    case Op::OP_LDS:
        write_creg(*this, creg_of(op), r[n]);
        return Opt{};
    case Op::OP_LDC_L:
        switch (row.fmt) {
        case OperandFmt::ldcl_creg:
            write_creg(*this, creg_of(op), pop32(*this, bus, n));
            return Opt{};
        case OperandFmt::ldcl_bank:
            rbank[(op >> 4) & 7] = pop32(*this, bus, n);
            return Opt{};
        default: break;
        }
        break;
    case Op::OP_LDS_L:
        write_creg(*this, creg_of(op), pop32(*this, bus, n));
        return Opt{};

    case Op::OP_OCBI:
    case Op::OP_OCBP:
    case Op::OP_OCBWB:
    case Op::OP_PREF:
        if (on_cache) on_cache(*this, r[n], op);
        return Opt{};
    case Op::OP_TAS_B: {
        const uint32_t a = r[n];
        const uint8_t v = bus.read8(a);
        set_t(v == 0);
        bus.write8(a, uint8_t(v | 0x80));
        return Opt{};
    }

    // ---------- desvios (alvo/PR travados ANTES do delay slot) ----------
    case Op::OP_BRA:
    case Op::OP_BSR:
        if (row.op == Op::OP_BSR) pr = addr + 4;
        return Opt(uint32_t(addr + 4 + 2u * uint32_t(d.disp12)));
    case Op::OP_BT:
    case Op::OP_BF:
    case Op::OP_BT_S:
    case Op::OP_BF_S: {
        const uint32_t target = addr + 4 + 2u * (op & 0xFF);
        const bool taken = row.op == Op::OP_BT     ? t()
                           : row.op == Op::OP_BF   ? !t()
                           : row.op == Op::OP_BT_S ? t()
                                                   : !t();
        return taken ? Opt(target) : Opt{};
    }
    case Op::OP_BRAF:
        return Opt(addr + 4 + r[n]);
    case Op::OP_BSRF: {
        pr = addr + 4;
        return Opt(addr + 4 + r[n]);
    }
    case Op::OP_JMP: return Opt(r[n]);
    case Op::OP_JSR: {
        pr = addr + 4;
        return Opt(r[n]);
    }
    case Op::OP_RTS: return Opt(pr);
    case Op::OP_RTE: {
        set_sr(ssr);
        return Opt(spc);
    }

    // ---------- FPU: logica de um op ou par (PR/SZ) ----------
    case Op::OP_FABS:
        fr[pr_bit() ? (n & 14u) : n] &= 0x7FFFFFFFu;
        return Opt{};
    case Op::OP_FNEG:
        fr[pr_bit() ? (n & 14u) : n] ^= 0x80000000u;
        return Opt{};
    case Op::OP_FADD:
    case Op::OP_FSUB:
    case Op::OP_FMUL:
    case Op::OP_FDIV:
    case Op::OP_FCMP_EQ:
    case Op::OP_FCMP_GT: {
        if (pr_bit()) {
            const double a = dn_d(get_dr(n), *this);
            const double b = dn_d(get_dr(m), *this);
            switch (row.op) {
            case Op::OP_FADD: set_dr(n, with_round(*this, [&] { return a + b; })); break;
            case Op::OP_FSUB: set_dr(n, with_round(*this, [&] { return a - b; })); break;
            case Op::OP_FMUL: set_dr(n, with_round(*this, [&] { return a * b; })); break;
            case Op::OP_FDIV: set_dr(n, with_round(*this, [&] { return a / b; })); break;
            case Op::OP_FCMP_EQ: set_t(a == b); break;
            default:             set_t(a > b); break;
            }
            // flush do resultado (FTZ) para aritmetica (nao para comparacao)
            if (row.op != Op::OP_FCMP_EQ && row.op != Op::OP_FCMP_GT) {
                const double r2 = get_dr(n);
                set_dr(n, dn_d(r2, *this));
            }
            return Opt{};
        }
        const float a = dn(f32(fr[n]), *this);
        const float b = dn(f32(fr[m]), *this);
        switch (row.op) {
        case Op::OP_FADD:
            set_fr(n, u32(with_round(*this, [&] { return a + b; })));
            break;
        case Op::OP_FSUB:
            set_fr(n, u32(with_round(*this, [&] { return a - b; })));
            break;
        case Op::OP_FMUL:
            set_fr(n, u32(with_round(*this, [&] { return a * b; })));
            break;
        case Op::OP_FDIV:
            set_fr(n, u32(with_round(*this, [&] { return a / b; })));
            break;
        case Op::OP_FCMP_EQ: set_t(f32(fr[n]) == f32(fr[m])); break;
        case Op::OP_FCMP_GT: set_t(f32(fr[n]) > f32(fr[m])); break;
        default: break;
        }
        return Opt{};
    }
    case Op::OP_FMAC: { // sempre single (manual PR=1 indefinido: segue oraculo)
        const float p = f32(fr[0]) * f32(fr[m]);
        fr[n] = u32(p + f32(fr[n]));
        return Opt{};
    }
    case Op::OP_FSQRT: {
        if (pr_bit()) {
            set_dr(n, with_round(*this, [&] { return std::sqrt(dn_d(get_dr(n), *this)); }));
            set_dr(n, dn_d(get_dr(n), *this));
            return Opt{};
        }
        set_fr(n, u32(with_round(*this, [&] { return std::sqrt(dn(f32(fr[n]), *this)); })));
        set_fr(n, u32(dn(f32(fr[n]), *this)));
        return Opt{};
    }
    case Op::OP_FSRRA: {
        if (pr_bit()) return Opt{}; // indefinido em PR=1: no-op (espec)
        set_fr(n, u32(1.0f / std::sqrt(f32(fr[n]))));
        return Opt{};
    }
    case Op::OP_FLDI0: fr[n] = 0x00000000u; return Opt{};
    case Op::OP_FLDI1: fr[n] = 0x3F800000u; return Opt{};
    case Op::OP_FLDS: fpul = fr[n & 0xF]; return Opt{};
    case Op::OP_FSTS: fr[n & 0xF] = fpul; return Opt{};
    case Op::OP_FLOAT:
        if (pr_bit()) set_dr(n, double(int32_t(fpul)));
        else fr[n & 0xF] = u32(float(int32_t(fpul)));
        return Opt{};
    case Op::OP_FTRC:
        if (pr_bit()) fpul = ftrc_sat(get_dr(n));
        else fpul = ftrc_sat(double(f32(fr[n & 0xF])));
        return Opt{};
    case Op::OP_FCNVSD: set_dr(n, double(f32(fpul))); return Opt{};
    case Op::OP_FCNVDS: fpul = u32(float(get_dr(n))); return Opt{};
    case Op::OP_FSCA: {
        if (pr_bit()) return Opt{}; // indefinido em PR=1: no-op (espec)
        const float* t = fsca_table() + 2 * (fpul & 0xFFFF);
        // base n&14: o rect1 exige rn par e o rect2 (0xFFFD) tem rn=15 ->
        // DR14 = fr[14]/fr[15] (n&0xF daria fr[16], fora do array).
        const unsigned b = n & 14u;
        fr[b] = u32(t[0]);
        fr[b + 1] = u32(t[1]);
        return Opt{};
    }
    case Op::OP_FIPR: {
        // indefinido com PR=1: no-op (oraculo tem guarda explicita). Campos:
        // vetor destino = bits 11-10 de Rn; vetor fonte = bits 9-8 de Rn
        // (padrao "1111nnmm11101101", oraculo linha 1228).
        if (pr_bit()) return Opt{};
        const unsigned a = ((n >> 2) & 3) * 4u;
        const unsigned b = (n & 3) * 4u;
        float s = 0.0f;
        for (unsigned k = 0; k < 4; ++k) s += f32(fr[a + k]) * f32(fr[b + k]);
        fr[a + 3] = u32(dn(s, *this));
        return Opt{};
    }
    case Op::OP_FTRV: {
        // indefinido com PR=1: no-op (oraculo). Vetor destino: bits 11-10
        // (base n&12; bits 9-8 fixados em 01 na gravura).
        if (pr_bit()) return Opt{};
        const unsigned a = n & 12u;
        float v[4], res[4];
        for (unsigned k = 0; k < 4; ++k) v[k] = dn(f32(fr[a + k]), *this);
        for (unsigned linha = 0; linha < 4; ++linha) {
            float s = 0.0f;
            for (unsigned k = 0; k < 4; ++k) {
                // XMTRX coluna-major: elemento [linha][col] = xf[col*4+linha]
                s += dn(f32(xf[linha + 4u * k]), *this) * v[k];
            }
            res[linha] = s;
        }
        for (unsigned k = 0; k < 4; ++k) fr[a + k] = u32(res[k]);
        return Opt{};
    }
    case Op::OP_FRCHG: {
        // indefinido com PR=1: no-op (spec, com guarda)
        if (!pr_bit()) set_fpscr(fpscr ^ kFpscrFR);
        return Opt{};
    }
    case Op::OP_FSCHG: {
        if (!pr_bit()) set_fpscr(fpscr ^ kFpscrSZ);
        return Opt{};
    }

    // ---------- FPU mov (todos so olham SZ; nota da spec) ----------
    case Op::OP_FMOV:
        switch (row.fmt) {
        case OperandFmt::fp_fmov_dr: { // reg-reg (F00C)
            if (!sz_bit()) { // single puro, sem paridade (spec)
                fr[n & 0xF] = fr[m & 0xF];
                return Opt{};
            }
            const uint32_t* s = fpair(m);
            uint32_t* dd = fpair(n);
            const uint32_t a = s[0], b2 = s[1];
            dd[0] = a;
            dd[1] = b2;
            return Opt{};
        }
        case OperandFmt::fp_fmov_dr_r2m_idx: { // FRm,@(R0,Rn) (F007)
            const uint32_t a = r[n] + r[0];
            if (sz_bit()) {
                const uint32_t* s = fpair(m);
                bus.write32(a, s[0]);
                bus.write32(a + 4, s[1]);
                return Opt{};
            }
            bus.write32(a, fr[m & 0xF]);
            return Opt{};
        }
        default: break;
        }
        break;
    case Op::OP_FMOV_S:
        switch (row.fmt) {
        case OperandFmt::fp_fmov_s_m2r: { // @Rm,FRn
            if (sz_bit()) {
                const uint32_t* s = fpair(m);
                uint32_t* dd = fpair(n);
                const uint32_t a = bus.read32(r[m]);
                const uint32_t b2 = bus.read32(r[m] + 4);
                dd[0] = a;
                dd[1] = b2;
                return Opt{};
            }
            fr[n & 0xF] = bus.read32(r[m]);
            return Opt{};
        }
        case OperandFmt::fp_fmov_s_m2r_pos: { // @Rm+,FRn
            const uint32_t a = r[m];
            if (sz_bit()) {
                uint32_t* dd = fpair(n);
                dd[0] = bus.read32(a);
                dd[1] = bus.read32(a + 4);
                r[m] = a + 8;
                return Opt{};
            }
            fr[n & 0xF] = bus.read32(a);
            r[m] = a + 4;
            return Opt{};
        }
        case OperandFmt::fp_fmov_s_m2r_idx: { // @(R0,Rm),FRn
            const uint32_t a = r[m] + r[0];
            if (sz_bit()) {
                uint32_t* dd = fpair(n);
                dd[0] = bus.read32(a);
                dd[1] = bus.read32(a + 4);
                return Opt{};
            }
            fr[n & 0xF] = bus.read32(a);
            return Opt{};
        }
        case OperandFmt::fp_fmov_s_r2m: { // FRm,@Rn
            if (sz_bit()) {
                const uint32_t* s = fpair(m);
                const uint32_t v0 = s[0], v1 = s[1];
                bus.write32(r[n], v0);
                bus.write32(r[n] + 4, v1);
                return Opt{};
            }
            bus.write32(r[n], fr[m & 0xF]);
            return Opt{};
        }
        case OperandFmt::fp_fmov_s_r2m_neg: { // FRm,@-Rn
            const uint32_t a = r[n] - (sz_bit() ? 8u : 4u);
            if (sz_bit()) {
                const uint32_t* s = fpair(m);
                bus.write32(a, s[0]);
                bus.write32(a + 4, s[1]);
                r[n] = a;
                return Opt{};
            }
            bus.write32(a, fr[m & 0xF]);
            r[n] = a;
            return Opt{};
        }
        default: break;
        }
        break;
    } // switch (row.op)

    // [default] forma legal sem semantica: bug do nucleo (nunca deve ocorrer;
    // o teste varre todas as 198 linhas e qualquer chamada aqui falha).
    if (on_illegal) on_illegal(*this, addr, op);
    return Opt{};
}

} // namespace hh::sh4
