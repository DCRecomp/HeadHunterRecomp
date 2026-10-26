// SPDX-License-Identifier: MIT
// Derivado de recomp-BStinger builder/main_cli.cpp (MIT, Copyright (c) 2026 Blue Stinger
// Recompiled contributors; ver third_party/licenses/recomp-BStinger-LICENSE.txt).
// hh-builder: o fluxo completo do Builder pela linha de comando (automacao e testes).
//   hh-builder --disc <disco 1.cdi> --disc2 <disco 2.cdi> --out <pasta> [--kit <pasta>] [--sdl3 <pasta>]
//              [--vcvars <vcvars64.bat> | --vswhere <vswhere.exe>] [--clean] [--verify-vblanks N]
//              [--name <subpasta>]   (padrao HeadHunter; o jogo instalado nao precisa dos CDIs)
//              [--projeto <pasta>]   exporta tambem o projeto C++ editavel (project_export.h)
//              [--portatil | --documentos]  local dos saves gravado em instalacao.toml
//   hh-builder --de-projeto <pasta> --out <pasta> [--disc <cdi> --disc2 <cdi>]   compila um projeto editado
//   hh-builder --disc <cdi> --disc2 <cdi> --out <recursos> --projeto <pasta> --somente-exportar
//              so exporta (sem compilador, compilacao nem instalacao); [--tipo Release|Debug] no build
// Codigo de saida: 0 = HeadHunter.exe instalado e verificado; 1 = falha; 2 = uso;
// 3 = cancelado (Ctrl+C).
#include "builder_core.h"
#include "child_process.h"

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <shellapi.h>

#include <atomic>
#include <cstdio>
#include <string>
#include <vector>

namespace fs = std::filesystem;
using namespace hh::builder;

namespace {

std::atomic<bool> g_cancel{false};

BOOL WINAPI onCtrl(DWORD) {
    g_cancel = true;
    return TRUE;
}

void out(const std::string& s) {  // UTF-8 no console e em pipes
    std::fwrite(s.data(), 1, s.size(), stdout);
    std::fflush(stdout);
}

fs::path exeDir() {
    std::wstring b(32768, L'\0');
    b.resize(GetModuleFileNameW(nullptr, b.data(), DWORD(b.size())));
    return fs::path(b).parent_path();
}

#ifndef HH_KIT_DIR
#define HH_KIT_DIR ""
#endif
#ifndef HH_SDL3_DIR
#define HH_SDL3_DIR ""
#endif

// Kit de fontes: kit\ ao lado do executavel (distribuicao do Builder) ou, no repositorio, o proprio projeto com as
// fontes do SDL3 baixadas pelo CMake (build\_deps\sdl3-src); o toml++ fica ao lado delas (toml_dir).
void defaultKit(Request& req) {
    std::error_code ec;
    if (fs::is_regular_file(exeDir() / L"kit" / L"CMakeLists.txt", ec)) {
        req.kitDir = exeDir() / L"kit";
        return;
    }
    req.kitDir = fs::u8path(HH_KIT_DIR);
    if (req.sdl3Dir.empty() && *HH_SDL3_DIR) req.sdl3Dir = fs::u8path(HH_SDL3_DIR);
}

} // namespace

int main() {
    SetConsoleOutputCP(CP_UTF8);
    int     argc = 0;
    wchar_t** wv = CommandLineToArgvW(GetCommandLineW(), &argc);
    std::vector<std::wstring> args(wv, wv + argc);
    LocalFree(wv);

    Request req;
    defaultKit(req);
    for (size_t i = 1; i < args.size(); ++i) {
        const std::wstring& a    = args[i];
        const bool          more = i + 1 < args.size();
        if (a == L"--disc" && more) req.disc = args[++i];
        else if (a == L"--disc2" && more) req.disc2 = args[++i];
        else if (a == L"--portatil") req.portableSaves = true;
        else if (a == L"--documentos") req.portableSaves = false;
        else if (a == L"--out" && more) req.outDir = args[++i];
        else if (a == L"--kit" && more) req.kitDir = args[++i];
        else if (a == L"--sdl3" && more) req.sdl3Dir = args[++i];
        else if (a == L"--vcvars" && more) req.vcvars = args[++i];
        else if (a == L"--vswhere" && more) req.vswhere = args[++i];
        else if (a == L"--verify-vblanks" && more) req.verifyVblanks = unsigned(std::stoul(args[++i]));
        else if (a == L"--name" && more) req.installName = args[++i];
        else if (a == L"--version-label" && more) req.versionLabel = narrow(args[++i]);
        else if (a == L"--projeto" && more) req.exportProject = args[++i];
        else if (a == L"--de-projeto" && more) req.fromProject = args[++i];
        else if (a == L"--clean") req.clean = true;
        else if (a == L"--somente-exportar") req.exportOnly = true;
        else if (a == L"--tipo" && more) req.buildType = narrow(args[++i]);
        else {
            std::fprintf(stderr,
                         "uso: hh-builder --disc <disco 1.cdi> --disc2 <disco 2.cdi> --out <pasta> [--kit <pasta>] [--sdl3 <pasta>]\n"
                         "                [--vcvars <vcvars64.bat> | --vswhere <vswhere.exe>] [--clean] [--name <subpasta>]\n"
                         "                [--projeto <pasta>]      exporta o projeto C++ editavel\n"
                         "                [--portatil | --documentos]  local dos saves (instalacao.toml)\n"
                         "  hh-builder --de-projeto <pasta> --out <pasta> [--disc <cdi> --disc2 <cdi>]   compila o projeto (mods)\n"
                         "                [--verify-vblanks N] [--tipo Release|Debug]\n"
                         "  hh-builder --disc <cdi> --disc2 <cdi> --out <recursos> --projeto <pasta> --somente-exportar\n"
                         "                (valida, prepara os dados em <recursos>, gera o C++ e exporta; sem compilador)\n");
            return 2;
        }
    }
    if (req.disc.empty() != req.disc2.empty()) {
        std::fprintf(stderr, "informe os dois discos (--disc e --disc2)\n");
        return 2;
    }
    if ((req.disc.empty() && req.fromProject.empty()) || req.outDir.empty()) {
        std::fprintf(stderr, "uso: hh-builder --disc <disco 1.cdi> --disc2 <disco 2.cdi> --out <pasta> (veja --help)\n");
        return 2;
    }
    SetConsoleCtrlHandler(onCtrl, TRUE);

    Callbacks cb;
    cb.cancel   = &g_cancel;
    int lastPct = -1;
    Step lastStep = Step::Done;
    cb.progress = [&](Step s, double f, const std::string&) {
        const int pct = int(f * 100);
        if (pct != lastPct || s != lastStep) {
            out("[progresso] " + std::to_string(pct) + "% " + step_name(s) + "\n");
            lastPct = pct, lastStep = s;
        }
    };
    const bool verbose = GetEnvironmentVariableW(L"HH_BUILDER_VERBOSE", nullptr, 0) > 0;
    cb.log = [&](const std::string& l) {
        if (verbose || l.rfind("== ", 0) == 0 || l.rfind("ERRO", 0) == 0 || l.rfind("OK:", 0) == 0) out(l + "\n");
    };
    const Result r = build(req, cb);
    if (r.ok && req.exportOnly) {
        out("exportado: " + path_text(r.projectDir) + "\n");
        return 0;
    }
    if (r.ok) {
        out("instalado: " + path_text(r.exe) + "\n");
        if (!r.warning.empty()) out("aviso: " + r.warning + "\n");
        return 0;
    }
    out(std::string("falhou em \"") + step_name(r.failedStep) + "\": " + r.error + "\n");
    out("log completo: " + path_text(r.logFile) + "\n");
    return r.cancelled ? 3 : 1;
}
