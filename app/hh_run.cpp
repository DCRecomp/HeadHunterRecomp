// SPDX-License-Identifier: MIT
// hh-run (HH05/HH06): executa o codigo recompilado sem janela, a partir dos dados de execucao
// (hh-extract --runtime), por um numero de VBlanks, sobre o nucleo comum app/hh_host.
//
//   hh-run --data <pasta> [--vblanks N] [--capture <pasta> [--every K]] [--rtc <segundos Unix>]
//          [--watch <endereco>]...   (diagnostico: VBlank e r4-r7 de cada entrada na funcao)
// Entrada: HH_INPUT_SCRIPT=<roteiro> (deterministico); sem ele, nenhum botao. --controles nativo
// (padrao, HH10) entrega as amostras pelo nucleo nativo; legado, pelo Maple do runtime.
//
// Saida: uma linha por quadro amostrado (VBlank, dimensoes, hash FNV-1a do ARGB, leituras do
// controle) e o motivo da parada. rc: 0 = limite de VBlanks atingido; 3 = o jogo pediu para
// sair antes do limite; 1 = erro (PC e valor). Os portoes de teste exigem rc 0.
#include "hh_discs.h"
#include "hh_host.h"
#include "input_bridge.h"
#include "input/input.h"
#include "platform/input.h"

#include <iterator>
#include <memory>

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

namespace fs = std::filesystem;

namespace {

uint64_t fnv1a(const std::vector<uint32_t>& v) {
    uint64_t h = 1469598103934665603ull;
    for (uint32_t x : v)
        for (int k = 0; k < 4; ++k) h = (h ^ ((x >> (8 * k)) & 0xFF)) * 1099511628211ull;
    return h;
}

void writePpm(const fs::path& p, const std::vector<uint32_t>& argb, uint32_t w, uint32_t h) {
    std::ofstream f(p, std::ios::binary);
    f << "P6\n" << w << ' ' << h << "\n255\n";
    for (uint32_t x : argb) {
        const char rgb[3] = {char((x >> 16) & 0xFF), char((x >> 8) & 0xFF), char(x & 0xFF)};
        f.write(rgb, 3);
    }
}

int usage() {
    std::fprintf(stderr, "uso: hh-run --data <pasta> [--vblanks N] [--capture <pasta> [--every K]] [--rtc <segundos Unix>]"
                         " [--watch <endereco>]... [--arquivos legado|nativo] [--tempo legado|nativo|experimental]"
                         " [--controles legado|nativo] [--mods <pasta>]\n");
    return 2;
}

} // namespace

int main(int argc, char** argv) {
    hh::HostOptions opt;
    opt.vblankLimit = 600;
    fs::path capture;
    uint64_t every = 0;
    for (int i = 1; i < argc; ++i) {
        const std::string a = argv[i];
        auto next = [&]() -> const char* { return i + 1 < argc ? argv[++i] : nullptr; };
        const char* v = nullptr;
        if (a == "--data" && (v = next())) opt.data = v;
        else if (a == "--vblanks" && (v = next())) opt.vblankLimit = std::strtoull(v, nullptr, 10);
        else if (a == "--capture" && (v = next())) capture = v;
        else if (a == "--every" && (v = next())) every = std::strtoull(v, nullptr, 10);
        else if (a == "--rtc" && (v = next())) opt.rtcUnix = std::strtoll(v, nullptr, 10);
        else if (a == "--watch" && (v = next())) opt.watch.push_back(uint32_t(std::strtoul(v, nullptr, 16)));
        else if (a == "--arquivos" && (v = next()) && (std::string(v) == "legado" || std::string(v) == "nativo"))
            opt.files = std::string(v) == "nativo" ? hh::FileProvider::Native : hh::FileProvider::Legacy;
        else if (a == "--tempo" && (v = next()) &&
                 (std::string(v) == "legado" || std::string(v) == "nativo" || std::string(v) == "experimental"))
            opt.time = std::string(v) == "nativo"         ? hh::TimeProvider::Native
                       : std::string(v) == "experimental" ? hh::TimeProvider::Experimental
                                                          : hh::TimeProvider::Legacy;
        else if (a == "--mods" && (v = next())) opt.modsDir = v;
        else if (a == "--controles" && (v = next()) && (std::string(v) == "legado" || std::string(v) == "nativo"))
            opt.controls = std::string(v) == "nativo" ? hh::ControlsProvider::Native : hh::ControlsProvider::Legacy;
        else return usage();
    }
    if (opt.data.empty()) return usage();
    try {
        const hh::InstallData inst = hh::resolve_install(opt.data);
        opt.filesManifest          = inst.manifest;
        opt.expectVersion          = hh::kInstallBootVersion;
        if (!inst.merged) std::fprintf(stderr, "[disco] dados so do disco 1 (sem arquivos.manifest)\n");
    } catch (const std::exception& e) {
        std::printf("PARADA: %s\n", e.what());
        return 1;
    }
    if (!capture.empty()) {
        fs::create_directories(capture);
        if (!every) every = 60;
    }
    opt.onFrame = [&](const hh::HostFrame& f) {
        if (!every || f.vblank % every) return;
        bool blank = true;
        for (uint32_t x : f.argb)
            if (x & 0x00FFFFFFu) {
                blank = false;
                break;
            }
        std::printf("quadro %llu: %ux%u hash %016llx leituras %llu%s\n", (unsigned long long)f.vblank, f.width, f.height,
                    (unsigned long long)fnv1a(f.argb), (unsigned long long)f.reads, blank ? " (preto)" : "");
        if (!capture.empty() && f.width && f.height) {
            char name[64];
            std::snprintf(name, sizeof name, "frame_%06llu.ppm", (unsigned long long)f.vblank);
            writePpm(capture / name, f.argb, f.width, f.height);
        }
    };

    // HH10: no --controles nativo, as amostras vem de um InputSystem: o roteiro (HH_INPUT_SCRIPT),
    // pela ponte, ou nenhum (controle presente e neutro, como o estado fixo do Maple no legado)
    std::unique_ptr<hh::native::input::InputSystem> input;
    if (opt.controls == hh::ControlsProvider::Native) {
        input = std::make_unique<hh::native::input::InputSystem>();
        if (const char* script = std::getenv("HH_INPUT_SCRIPT"); script && *script) {
            std::ifstream f(script, std::ios::binary);
            if (!f) {
                std::printf("PARADA: roteiro de entrada nao encontrado: %s\n", script);
                return 1;
            }
            const std::string text((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
            input->set_test_source(hh::transitional::input_bridge::from_runtime_source(
                std::shared_ptr<hh::InputSource>(hh::make_scripted_input(text))));
            std::fprintf(stderr, "[input] roteiro %s (controles nativos)\n", script);
        }
        opt.input = input.get();
    }
    const hh::HostResult r = hh::host_run(opt);
    std::printf("vblanks: %llu | quadros nao pretos: %llu (primeiro no vblank %llu)\n", (unsigned long long)r.vblanks,
                (unsigned long long)r.nonBlank, (unsigned long long)r.firstNonBlank);
    switch (r.end) {
    case hh::HostEnd::Limit: std::printf("ok: limite de VBlanks atingido\n"); return 0;
    case hh::HostEnd::GameExit:
        std::printf("o jogo pediu para sair (codigo %08X) antes do limite de VBlanks\n", r.exitCode);
        return 3;
    default: std::printf("PARADA: %s\n", r.error.c_str()); return 1;
    }
}
