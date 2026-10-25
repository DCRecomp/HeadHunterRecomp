// SPDX-License-Identifier: MIT
// Derivado de recomp-BStinger tests/unit/test_pad_adapter.cpp (MIT, Copyright (c) 2026 Blue Stinger
// Recompiled contributors; ver third_party/licenses/recomp-BStinger-LICENSE.txt).
// HH10: provedor nativo do pd 1.50 (transitional/pad_adapter) sobre uma RAM de teste: modelo de
// presenca (identificar -> ler; leitura sem controle -> perder) com a amostra coletada no proprio
// disparo (regra medida no HH), old lido da RAM, bordas, gatilhos com histerese na RAM do SDK, info
// na area nativa do BIOS HLE e a guarda dela, getter, tabela de estado e os negativos.
#include "test.h"

#include "cpu/sh4_context.h"
#include "cpu/sh4_runtime.h"
#include "hle/bios.h"
#include "input/input.h"
#include "input_bridge.h"
#include "pad_adapter.h"
#include "pad_contract.h"
#include "platform/input.h"

#include <cstring>
#include <map>
#include <stdexcept>
#include <string>
#include <vector>

using namespace hh;
namespace ni  = hh::native::input;
namespace pad = hh::transitional::pad;

namespace {

[[noreturn]] void throwingFatal(Sh4Context*, const char* what, uint32_t, uint32_t) { throw std::runtime_error(what); }

constexpr uint32_t kPerInfoBase = 0x8C32B408;  // buffer de info da unidade 0 (o valor medido no HH)
constexpr uint32_t kPerRawBase  = 0x8C320970;  // descricoes brutas (literal 0x8C16F18C)

struct Ram {
    std::vector<uint8_t> mem = std::vector<uint8_t>(16u << 20);
    Sh4Context           c{};
    Ram() {
        c.ram   = mem.data();
        c.r[15] = 0x8C100000;
        // camada per: per_info(i) = *(*(8C17F4A8)) + (int16)*(8C17F49A) + 120 i = kPerInfoBase + 120 i
        w32(pad::kPerInfoPtrLiteral, 0x8C300000);
        w32(0x8C300000, kPerInfoBase - 100);
        const uint16_t off = 100;
        std::memcpy(at(pad::kPerInfoOffLiteral), &off, 2);
        for (int i = 0; i < pad::kUnits; ++i) std::memcpy(at(kPerInfoBase + 120u * uint32_t(i) + 0x14), "(none)", 6);
        std::memset(at(kHleNativeAreaBase), 0xFF, kHleNativeAreaSize);  // como o boot nativo deixa
        w32(pad::kPerRawLiteral, kPerRawBase);
        // configuracao 5 da camada per: *(*(8C17F600) + (int16)*(8C17F5E8)) = 0 (sem conversao do support)
        w32(pad::kPerCfgPtrLiteral, 0x8C301000);
        const uint16_t cfgOff = 0x10;
        std::memcpy(at(pad::kPerCfg5OffLiteral), &cfgOff, 2);
        // limiares dos gatilhos (pdSetThreshold)
        *at(pad::kThrHighAddr) = 0x80, *at(pad::kThrLowAddr) = 0x40;
    }
    uint8_t* at(uint32_t a) { return mem.data() + (a & 0xFFFFFF); }
    void     w32(uint32_t a, uint32_t v) { std::memcpy(at(a), &v, 4); }
    uint32_t r32(uint32_t a) {
        uint32_t v;
        std::memcpy(&v, at(a), 4);
        return v;
    }
    pad::Pds unit(int i) { return pad::pds_load(at(pad::kTable + pad::kUnitSize * uint32_t(i))); }
};

ni::SampleFn script(std::map<uint64_t, std::string> steps) {
    return [steps](const ni::SampleContext& ctx) {
        std::string t = "-";
        for (const auto& [k, v] : steps)
            if (k <= ctx.sample) t = v;
        return hh::transitional::input_bridge::to_controller(pad_state_from_text(t));
    };
}

std::string cstr(Ram& r, uint32_t a) { return std::string(reinterpret_cast<const char*>(r.at(a))); }

} // namespace

// Conexao, leitura, desconexao com botao: o servidor ve a amostra do disparo anterior a ele (coleta no
// disparo), depois da identificacao.
HH_TEST(hh10_pad_adapter_presence_model_and_server) {
    Ram             r;
    ni::InputSystem sys;
    sys.set_test_source(script({{0, "A+AX=79+LT=200"}, {1, "DESCONECTADO"}}));
    pad::test::begin(sys);
    pad::test::server(&r.c);  // antes de qualquer disparo: porta vazia
    pad::Pds u = r.unit(0);
    HH_CHECK(u.info == kHleNativePadInfo && u.id == 0 && u.on == 0 && u.off == 0xFFFFFFFFu && u.name == pad::kNoDeviceName);
    HH_CHECK(cstr(r, kHleNativePadInfo + 0x14) == "(none)" && r.r32(kHleNativePadInfo) == 0);
    pad::test::dma_fired(&r.c);  // 1: identificar (presente) e ja coletado
    HH_CHECK(r.r32(kHleNativePadInfo) == 1 && cstr(r, kHleNativePadInfo + 0x14).rfind("Dreamcast Controller", 0) == 0);
    pad::test::server(&r.c);     // so a identificacao: o servidor ainda ve "sem dispositivo"
    HH_CHECK(r.unit(0).id == 0 && r.unit(0).on == 0);
    pad::test::dma_fired(&r.c);  // 2: le a amostra 0 (coletada no disparo)
    pad::test::server(&r.c);
    u = r.unit(0);
    HH_CHECK(u.id == 0x000F06FEu && u.support == 0x000F06FEu && u.name == kHleNativePadInfo + 0x14);
    HH_CHECK(u.on == (0x4u | 0x20000u) && u.press == u.on && u.release == 0 && u.off == ~u.on);  // A e L (200 >= 0x80)
    HH_CHECK(u.x1 == 79 - 128 && u.y1 == 0 && u.l == 200 && u.r == 0);
    HH_CHECK(*r.at(pad::kLatchLAddr) == 1 && *r.at(pad::kLatchRAddr) == 0);  // histerese na RAM do SDK
    // old vem da RAM: o jogo reescreve on; a passada seguinte usa esse valor
    r.w32(pad::kTable + 0x08, 0x4);
    pad::test::server(&r.c);  // mesma amostra
    u = r.unit(0);
    HH_CHECK(u.old == 0x4 && u.press == 0x20000u && u.release == 0);
    pad::test::dma_fired(&r.c);  // 3: a amostra 1 e DESCONECTADO: perdido no disparo
    HH_CHECK(r.r32(kHleNativePadInfo) == 0 && cstr(r, kHleNativePadInfo + 0x14) == "(none)");
    pad::test::server(&r.c);
    u = r.unit(0);
    HH_CHECK(u.id == 0 && u.on == 0 && u.release == (0x4u | 0x20000u) && u.name == pad::kNoDeviceName);
    HH_CHECK(pad::stats().lost == 1 && pad::stats().reads == 1);
    // demais unidades: info da camada per; subunidade so muda name; resposta FC e desconhecida viram
    // os codigos de estado de 8C15DB06
    HH_CHECK(r.unit(6).info == kPerInfoBase + 120u * 6 && r.unit(6).name == pad::kNoDeviceName);
    HH_CHECK(r.unit(1).info == kPerInfoBase + 120u && r.unit(1).name == kPerInfoBase + 120u + 20);
    r.at(kPerRawBase + 24 * 7)[4]  = 0xFC;
    r.at(kPerRawBase + 24 * 13)[4] = 0x42;
    pad::test::server(&r.c);
    HH_CHECK(r.r32(pad::kStatusTable + 4 * 7) == 0xFF0000FCu && r.r32(pad::kStatusTable + 4 * 13) == 0xFF000000u);
    HH_CHECK(r.r32(pad::kStatusTable) == 0 && r.r32(pad::kStatusTable + 4 * 8) == 0);
    pad::test::end();
}

// Gatilho entre os limiares mantem o estado anterior (histerese do 8C15D900) pelo adaptador.
HH_TEST(hh10_pad_adapter_trigger_hysteresis) {
    Ram             r;
    ni::InputSystem sys;
    sys.set_test_source(script({{0, "LT=200"}, {1, "LT=100"}, {2, "LT=50"}, {3, "LT=100"}}));
    pad::test::begin(sys);
    pad::test::dma_fired(&r.c);  // identificar
    uint32_t seen[4];
    for (int k = 0; k < 4; ++k) {
        pad::test::dma_fired(&r.c);
        pad::test::server(&r.c);
        seen[k] = r.unit(0).on & 0x20000u;
    }
    // 200 aciona; 100 (entre 0x40 e 0x80) mantem; 50 solta; 100 de novo fica solto
    HH_CHECK(seen[0] && seen[1] && !seen[2] && !seen[3]);
    pad::test::end();
}

// Unidade-base de outra porta (B0) pela descricao bruta da camada per: controle com erro 0 e resposta 8
// decodificado do quadro; com erro, "sem dispositivo".
HH_TEST(hh10_pad_adapter_other_port_base_unit) {
    for (uint8_t error : {uint8_t(0), uint8_t(1)}) {
        Ram            r;
        const uint32_t info = kPerInfoBase + 120u * 6, raw = kPerRawBase + 24u * 6, frame = 0x8C200000;
        r.w32(info, 1);
        r.w32(info + 4, 0xFE060F00);  // dados da funcao como na RAM -> support 000F06FE
        r.at(raw)[2] = error;
        r.at(raw)[4] = 8;
        r.w32(raw + 8, frame);
        const uint8_t cond[8] = {0xFB, 0xFF, 0, 0, 0x80, 0x80, 0x80, 0x80};  // A (ativo em 0)
        std::memcpy(r.at(frame + 8), cond, 8);
        ni::InputSystem sys;
        pad::test::begin(sys);
        pad::test::server(&r.c);
        const pad::Pds u = r.unit(6);
        if (error == 0) HH_CHECK(u.id == 0x000F06FEu && u.on == 0x4 && u.name == info + 20);
        else HH_CHECK(u.id == 0 && u.on == 0 && u.name == pad::kNoDeviceName);
        pad::test::end();
    }
}

// pdGetPeripheral (8C15DC40): r0 = 8C311DAC + 52 * porta, MACL como o original.
HH_TEST(hh10_pad_adapter_getter) {
    Ram             r;
    ni::InputSystem sys;
    pad::test::begin(sys);
    r.c.r[4] = 3;
    pad::test::getter(&r.c);
    HH_CHECK(r.c.r[0] == pad::kTable + 3 * 52 && r.c.macl == 3 * 52);
    pad::test::end();
}

// Guarda da area nativa do BIOS HLE: ocupada antes da primeira escrita ou alterada por outro = erro.
HH_TEST(hh10_pad_adapter_native_area_guard) {
    sh4_set_fatal_handler(&throwingFatal);
    {
        Ram             r;
        ni::InputSystem sys;
        r.at(kHleNativePadInfo)[5] = 0;  // ja ocupada
        pad::test::begin(sys);
        bool threw = false;
        try {
            pad::test::server(&r.c);
        } catch (const std::runtime_error& e) {
            threw = std::string(e.what()).find("ja estava ocupada") != std::string::npos;
        }
        HH_CHECK(threw);
        pad::test::end();
    }
    {
        Ram             r;
        ni::InputSystem sys;
        pad::test::begin(sys);
        pad::test::server(&r.c);
        r.at(kHleNativePadInfo)[3] ^= 1;  // outro codigo escreveu
        bool threw = false;
        try {
            pad::test::server(&r.c);
        } catch (const std::runtime_error& e) {
            threw = std::string(e.what()).find("outro codigo escreveu") != std::string::npos;
        }
        HH_CHECK(threw);
        pad::test::end();
    }
    sh4_set_fatal_handler(nullptr);
}

// Portao C6 (remapeamento) ate o PDS: controles.toml com A<->B num perfil de GUID + controle fisico
// apertando "b" = roteiro logico com A; um controle de outro GUID (sem perfil) apertando "b" diverge.
HH_TEST(hh10_pad_adapter_remap_c6) {
    const std::string guid = "030000005e0400008e02000000007200";
    std::string       err;
    auto map = ni::ActionMap::parse("[perfis.\"" + guid + "\"]\nA = \"b\"\nB = \"a\"\n", &err);
    HH_CHECK(map);
    if (!map) return;
    auto onAfter = [](ni::InputSystem& sys) {
        Ram r;
        pad::test::begin(sys);
        pad::test::dma_fired(&r.c);  // identificar
        pad::test::dma_fired(&r.c);  // ler
        pad::test::server(&r.c);
        const uint32_t on = r.unit(0).on;
        pad::test::end();
        return on;
    };
    ni::InputSystem logical;
    logical.set_test_source(script({{0, "A"}}));
    const uint32_t want = onAfter(logical);
    HH_CHECK(want == 0x4u);  // A do Dreamcast
    ni::InputSystem physical(map.value());
    physical.gamepad_added(1, "Xbox 360 Controller", guid);
    physical.gamepad_button(1, "b", true);
    HH_CHECK(onAfter(physical) == want);
    ni::InputSystem other(map.value());
    other.gamepad_added(2, "Generico", "03000000aaaaaaaabbbbbbbb00000000");
    other.gamepad_button(2, "b", true);
    HH_CHECK(onAfter(other) != want);  // negativo: perfil de outro GUID nao se aplica
}

HH_TEST(hh10_pad_adapter_mutations) {
    HH_CHECK(pad::parse_mutations("old_host,press_on") == (pad::kNatOldHost | pad::kNatPressOn));
    HH_CHECK(pad::parse_mutations("") == pad::kNatNone);
    bool threw = false;
    try {
        pad::parse_mutations("nada");
    } catch (const std::runtime_error&) {
        threw = true;
    }
    HH_CHECK(threw);
}
