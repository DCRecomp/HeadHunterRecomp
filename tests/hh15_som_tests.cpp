// SPDX-License-Identifier: MIT
// HH15.6: a area de som nativa (runtime/system/sound_bus) atende o contrato do RTC e do controle com o efeito do modelo
// da AICA de antes (valores conferidos aqui contra a contagem do provedor de tempo estavel), o modelo (aica/aica_regs)
// chega aos mesmos valores pela delegacao, e tudo o mais na area e erro explicito no servico.
#include "test.h"

#include "aica/aica_regs.h"
#include "cpu/sh4_runtime.h"
#include "cpu/time_hooks.h"
#include "debug/clock_log.h"
#include "system/sound_bus.h"

#include <cstdio>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <initializer_list>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace {

struct Fatal : std::runtime_error {
    using std::runtime_error::runtime_error;
};

constexpr uint64_t kSecond = hh::kCompatTicksPerSecond;
constexpr uint32_t kCtrl = 0xA0702C00, kRtcHi = 0xA0710000, kRtcLo = 0xA0710004, kRtcEn = 0xA0710008;

using Read  = bool (*)(hh::Sh4Context*, uint32_t, unsigned, uint32_t&);
using Write = bool (*)(hh::Sh4Context*, uint32_t, unsigned, uint32_t);

// O roteiro do contrato: RTC definido no boot, leituras depois de 3 s, escrita liberada (16 bits baixos e depois os
// altos, que desligam a liberacao), escrita sem liberacao ignorada, controle guardado.
std::vector<uint32_t> script(Read rd, Write wr) {
    hh::Sh4Context c{};
    std::vector<uint32_t> seen;
    auto get = [&](uint32_t a) {
        uint32_t v = 0xDEADBEEF;
        HH_CHECK(rd(&c, a, 4, v));
        seen.push_back(v);
    };
    hh::aica_regs_reset();
    hh::aica_rtc_set(0x906D38C5, 0);
    c.cycles = 3 * kSecond + 5;
    get(kRtcHi), get(kRtcLo), get(kRtcEn);
    HH_CHECK(wr(&c, kRtcEn, 4, 1));
    HH_CHECK(wr(&c, kRtcLo, 4, 0x1234));
    get(kRtcHi), get(kRtcLo);
    HH_CHECK(wr(&c, kRtcHi, 4, 0xABCD));
    c.cycles += 2 * kSecond;
    get(kRtcHi), get(kRtcLo);
    HH_CHECK(wr(&c, kRtcHi, 4, 0x1111));  // liberacao desligada pela escrita dos altos: ignorada
    get(kRtcHi);
    HH_CHECK(wr(&c, kCtrl, 4, 0x00000301));
    get(kCtrl);
    return seen;
}

} // namespace

HH_TEST(hh15_area_de_som_igual_ao_modelo_no_contrato) {
    const std::vector<uint32_t> bus   = script(hh::sound_bus_read, hh::sound_bus_write);
    const std::vector<uint32_t> model = script(hh::aica_reg_read, hh::aica_reg_write);
    const std::vector<uint32_t> want  = {0x906D, 0x38C8, 0, 0x906D, 0x1234, 0xABCD, 0x1236, 0xABCD, 0x301};
    HH_CHECK(bus == want);
    HH_CHECK(model == want);
    hh::Sh4Context c{};
    uint32_t       v = 0;
    HH_CHECK(!hh::sound_bus_read(&c, 0xA05F6800, 4, v));  // fora da area: nao e do servico
    HH_CHECK(!hh::sound_bus_write(&c, 0xFFD80004, 4, 1));  // P4
    hh::aica_regs_reset();
}

// Revisao Codex HH15.6: o HH_CLOCK_LOG do roteiro (Rtc nas leituras do RTC, Aud no endereco e valor das escritas) tem
// a contagem e o hash esperados, calculados aqui dos valores do contrato (nao do servico), nos dois caminhos.
HH_TEST(hh15_area_de_som_clock_log_do_contrato) {
    auto fnv = [](std::initializer_list<uint32_t> vs) {
        uint64_t h = 0xCBF29CE484222325ull;
        for (uint32_t v : vs)
            for (int i = 0; i < 4; ++i) h = (h ^ ((v >> (8 * i)) & 0xFF)) * 0x100000001B3ull;
        return h;
    };
    char want[160];
    std::snprintf(want, sizeof want, "\t7\t%016llX\t0\t%016llX\t10\t%016llX\t",
                  (unsigned long long)fnv({0x906D, 0x38C8, 0x906D, 0x1234, 0xABCD, 0x1236, 0xABCD}),
                  (unsigned long long)fnv({}),
                  (unsigned long long)fnv({0x00710008, 1, 0x00710004, 0x1234, 0x00710000, 0xABCD, 0x00710000, 0x1111,
                                           0x00702C00, 0x301}));
    const std::filesystem::path path = std::filesystem::temp_directory_path() / "hh15_som_clock_log.tsv";
    for (auto [rd, wr] : {std::pair<Read, Write>{hh::sound_bus_read, hh::sound_bus_write},
                          std::pair<Read, Write>{hh::aica_reg_read, hh::aica_reg_write}}) {
        hh::clock_log_init(path.string());
        script(rd, wr);
        hh::clock_log_vblank(1);
        hh::clock_log_init("");
        std::ifstream in(path);
        std::string   header, line;
        std::getline(in, header);
        std::getline(in, line);
        // colunas: vblank, tmu, spg, mdst, rtc, ist, aud, audrd (n e h de cada); confere rtc..aud
        size_t at = 0;
        for (int tab = 0; tab < 7 && at != std::string::npos; ++tab) at = line.find('\t', at + 1);
        HH_CHECK(at != std::string::npos && line.compare(at, std::strlen(want), want) == 0);
    }
    std::filesystem::remove(path);
    hh::aica_regs_reset();
}

// Revisao Codex HH15.6: o reset entre sessoes poe o RTC na hora do host no instante 0 (o mesmo efeito do RTC sem valor
// definido, que o define no primeiro acesso) e desliga o controle e a liberacao de escrita.
HH_TEST(hh15_area_de_som_reset_entre_sessoes) {
    for (auto [rd, wr] : {std::pair<Read, Write>{hh::sound_bus_read, hh::sound_bus_write},
                          std::pair<Read, Write>{hh::aica_reg_read, hh::aica_reg_write}}) {
        hh::Sh4Context c{};
        uint32_t       hi = 0, lo = 0, ctrl = 0xDEADBEEF;
        hh::aica_rtc_set(0x12345678, 0);
        HH_CHECK(wr(&c, kCtrl, 4, 0x301));
        HH_CHECK(wr(&c, kRtcEn, 4, 1));
        const uint32_t before = hh::sound_bus_rtc_host_now();
        hh::aica_regs_reset();
        const uint32_t after = hh::sound_bus_rtc_host_now();
        HH_CHECK(rd(&c, kCtrl, 4, ctrl));
        HH_CHECK(ctrl == 0);
        HH_CHECK(wr(&c, kRtcLo, 4, 0x1234));  // liberacao desligada pelo reset: ignorada
        HH_CHECK(rd(&c, kRtcHi, 4, hi));
        HH_CHECK(rd(&c, kRtcLo, 4, lo));
        const uint32_t rtc = (hi << 16) | lo;
        HH_CHECK(rtc >= before && rtc <= after);
        HH_CHECK(rtc != 0x12345678);
    }
    hh::aica_regs_reset();
}

HH_TEST(hh15_area_de_som_recusa_fora_do_contrato) {
    hh::sh4_set_fatal_handler([](hh::Sh4Context*, const char* w, uint32_t, uint32_t) { throw Fatal(w); });
    hh::Sh4Context c{};
    int            refused = 0;
    auto expectFatal = [&](auto&& f) {
        bool fatal = false;
        try {
            f();
        } catch (const Fatal&) {
            fatal = true;
        }
        HH_CHECK(fatal);
        refused += fatal;
    };
    uint32_t v = 0;
    for (uint32_t a : {0xA0700000u, 0xA0702800u, 0xA0703000u, 0xA0710010u, 0xA0800000u, 0xA09FFFFCu, 0x00C00000u}) {
        expectFatal([&] { hh::sound_bus_read(&c, a, 4, v); });
        expectFatal([&] { hh::sound_bus_write(&c, a, 4, 0); });
    }
    expectFatal([&] { hh::sound_bus_read(&c, kRtcLo, 2, v); });
    expectFatal([&] { hh::sound_bus_write(&c, kCtrl, 1, 0); });
    HH_CHECK(refused == 16);
    // o modelo da verificacao nao modela os canais: devolve "nao tratado" (o mapa registra), sem erro
    HH_CHECK(!hh::aica_reg_read(&c, 0xA0700000, 4, v));
    hh::sh4_set_fatal_handler(nullptr);
    hh::aica_regs_reset();
}
