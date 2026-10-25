// SPDX-License-Identifier: MIT
// HeadHunter (Preview, HH06): janela SDL3 sobre o nucleo de execucao (hh_host). O jogo roda
// numa thread propria no ritmo do Dreamcast (59,94 VBlanks por segundo); a janela apresenta o
// quadro mais recente na frequencia do monitor, com area logica 4:3 e barras. Entrada:
// teclado (eventos da janela) + XInput; com HH_INPUT_SCRIPT, so o roteiro (modo espectador).
//
//   HeadHunter [--data <pasta>] [--vblanks N] [--rtc <segundos Unix>] [--log <arquivo> --hash-every K]
// --log grava "quadro N: LxA hash H" a cada K VBlanks (mesmo hash do hh-run): prova que a
// execucao na janela e a mesma do modo headless com o mesmo roteiro.
// Pasta de dados padrao: data\ ao lado do executavel. Teclas padrao (runtime/input):
// setas = direcional, ENTER = START, Z/X/A/S = A/B/X/Y. ESC fecha.
//
// Fase hibrida: liga o runtime temporario (removido do produto no HH15).
#include "hh_host.h"

#include "input/input.h"

#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>

#include <atomic>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <mutex>
#include <string>
#include <thread>

namespace fs = std::filesystem;

namespace {

std::string keyName(SDL_Keycode k) {
    if (k >= SDLK_A && k <= SDLK_Z) return std::string(1, char('A' + (k - SDLK_A)));
    if (k >= SDLK_0 && k <= SDLK_9) return std::string(1, char('0' + (k - SDLK_0)));
    if (k >= SDLK_F1 && k <= SDLK_F12) return "F" + std::to_string(1 + (k - SDLK_F1));
    switch (k) {
    case SDLK_RETURN: case SDLK_KP_ENTER: return "ENTER";
    case SDLK_SPACE: return "SPACE";
    case SDLK_TAB: return "TAB";
    case SDLK_BACKSPACE: return "BACKSPACE";
    case SDLK_LSHIFT: case SDLK_RSHIFT: return "SHIFT";
    case SDLK_LCTRL: case SDLK_RCTRL: return "CTRL";
    case SDLK_UP: return "UP";
    case SDLK_DOWN: return "DOWN";
    case SDLK_LEFT: return "LEFT";
    case SDLK_RIGHT: return "RIGHT";
    default: return "";
    }
}

struct Shared {
    std::mutex            mu;
    std::vector<uint32_t> argb;
    uint32_t              w = 0, h = 0;
    uint64_t              seq = 0;
};

fs::path exeDir() {
    const char* base = SDL_GetBasePath();
    return base ? fs::path(reinterpret_cast<const char8_t*>(base)) : fs::current_path();
}

} // namespace

int main(int argc, char** argv) {
    hh::HostOptions opt;
    std::string     logPath;
    uint64_t        hashEvery = 0;
    bool            headless  = false;
    opt.realtime = true;
    opt.data     = exeDir() / "data";
    for (int i = 1; i < argc; ++i) {
        const std::string a = argv[i];
        if (a == "--data" && i + 1 < argc) opt.data = argv[++i];
        else if (a == "--vblanks" && i + 1 < argc) opt.vblankLimit = std::strtoull(argv[++i], nullptr, 10);
        else if (a == "--rtc" && i + 1 < argc) opt.rtcUnix = std::strtoll(argv[++i], nullptr, 10);
        else if (a == "--log" && i + 1 < argc) logPath = argv[++i];
        else if (a == "--hash-every" && i + 1 < argc) hashEvery = std::strtoull(argv[++i], nullptr, 10);
        else if (a == "--headless") headless = true;
        else {
            std::fprintf(stderr, "uso: HeadHunter [--data <pasta>] [--vblanks N] [--rtc <segundos Unix>] "
                                 "[--log <arquivo> --hash-every K] [--headless]\n");
            return 2;
        }
    }
    // --headless (verificacao da instalacao pelo Builder e pela regressao): o mesmo executavel e
    // o mesmo nucleo, sem janela nem ritmo de tempo real; exige um limite de VBlanks.
    if (headless && !opt.vblankLimit) {
        std::fprintf(stderr, "--headless exige --vblanks\n");
        return 2;
    }
    std::FILE* log = nullptr;
    if (!logPath.empty()) {
        if (!hashEvery || !(log = std::fopen(logPath.c_str(), "w"))) {
            std::fprintf(stderr, "nao foi possivel gravar o log %s (exige --hash-every)\n", logPath.c_str());
            return 2;
        }
    }

    // Entrada: roteiro (espectador) ou teclado da janela + XInput.
    hh::KeyEventInput* keys = nullptr;
    if (std::getenv("HH_INPUT_SCRIPT") || headless) {
        hh::input_configure_from_env();
    } else {
        auto k = hh::make_key_event_input();
        keys   = k.get();
        std::vector<std::unique_ptr<hh::InputSource>> v;
        v.push_back(std::move(k));
        v.push_back(hh::make_xinput_input());
        hh::input_set_source(hh::make_combined_input(std::move(v)));
    }

    Shared            shared;
    std::atomic<bool> stop{false};
    std::string       presentError;  // falha de apresentacao (thread da janela)
    opt.stop    = &stop;
    opt.onFrame = [&](const hh::HostFrame& f) {
        if (log && f.vblank % hashEvery == 0) {
            uint64_t h = 1469598103934665603ull;  // FNV-1a, como no hh-run
            for (uint32_t x : f.argb)
                for (int k = 0; k < 4; ++k) h = (h ^ ((x >> (8 * k)) & 0xFF)) * 1099511628211ull;
            std::fprintf(log, "quadro %llu: %ux%u hash %016llx\n", (unsigned long long)f.vblank, f.width, f.height,
                         (unsigned long long)h);
        }
        if (headless) return;
        std::lock_guard lk(shared.mu);
        shared.argb = f.argb;
        shared.w = f.width, shared.h = f.height;
        ++shared.seq;
    };

    hh::HostResult result;
    if (headless) {
        opt.realtime = false;
        result       = hh::host_run(opt);
    } else {
        if (!SDL_Init(SDL_INIT_VIDEO)) {
            std::fprintf(stderr, "SDL_Init: %s\n", SDL_GetError());
            return 1;
        }
        SDL_Window*   win = SDL_CreateWindow("Head Hunter (Preview)", 1280, 960, SDL_WINDOW_RESIZABLE);
        SDL_Renderer* ren = win ? SDL_CreateRenderer(win, nullptr) : nullptr;
        if (!ren || !SDL_SetRenderLogicalPresentation(ren, 640, 480, SDL_LOGICAL_PRESENTATION_LETTERBOX)) {
            std::fprintf(stderr, "janela: %s\n", SDL_GetError());
            return 1;
        }
        SDL_SetRenderVSync(ren, 1);  // sem vsync o quadro ainda aparece; nao e erro

        std::atomic<bool> finished{false};
        std::thread       game([&] {
            result   = hh::host_run(opt);
            finished = true;  // limite, saida do jogo ou erro: a janela fecha
        });

        SDL_Texture* tex = nullptr;
        uint32_t     tw = 0, th = 0;
        uint64_t     shown   = 0;
        bool         running = true;
        while (running) {
            SDL_Event e;
            while (SDL_PollEvent(&e)) {
                if (e.type == SDL_EVENT_QUIT) running = false;
                else if ((e.type == SDL_EVENT_KEY_DOWN || e.type == SDL_EVENT_KEY_UP) && !e.key.repeat) {
                    if (e.key.key == SDLK_ESCAPE && e.type == SDL_EVENT_KEY_DOWN) running = false;
                    else if (keys) {
                        const std::string n = keyName(e.key.key);
                        if (!n.empty()) keys->key(n, e.type == SDL_EVENT_KEY_DOWN);
                    }
                } else if (e.type == SDL_EVENT_WINDOW_FOCUS_LOST && keys) {
                    keys->releaseAll();
                }
            }
            {
                std::lock_guard lk(shared.mu);
                if (shared.seq != shown && shared.w && shared.h) {
                    if (!tex || tw != shared.w || th != shared.h) {
                        if (tex) SDL_DestroyTexture(tex);
                        tex = SDL_CreateTexture(ren, SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_STREAMING,
                                                int(shared.w), int(shared.h));
                        tw = shared.w, th = shared.h;
                    }
                    if (!tex || !SDL_UpdateTexture(tex, nullptr, shared.argb.data(), int(shared.w * 4)))
                        presentError = std::string("textura do quadro: ") + SDL_GetError();
                    shown = shared.seq;
                }
            }
            SDL_SetRenderDrawColor(ren, 0, 0, 0, 255);
            if (!SDL_RenderClear(ren) || (tex && !SDL_RenderTexture(ren, tex, nullptr, nullptr)) ||
                !SDL_RenderPresent(ren))
                presentError = std::string("apresentacao: ") + SDL_GetError();
            if (!presentError.empty() || finished.load()) running = false;
        }
        stop = true;  // o jogo para no proximo VBlank ou ponto de poll (hh_host)
        game.join();
        if (tex) SDL_DestroyTexture(tex);
        SDL_DestroyRenderer(ren);
        SDL_DestroyWindow(win);
        SDL_Quit();
    }
    if (log && std::fclose(log) != 0) presentError = "falha ao gravar o log " + logPath;

    if (!presentError.empty()) {
        std::printf("PARADA: %s\n", presentError.c_str());
        return 1;
    }
    switch (result.end) {
    case hh::HostEnd::Limit: std::printf("ok: limite de VBlanks atingido (%llu)\n", (unsigned long long)result.vblanks); return 0;
    case hh::HostEnd::Stopped: std::printf("janela fechada no VBlank %llu\n", (unsigned long long)result.vblanks); return 0;
    case hh::HostEnd::GameExit: std::printf("o jogo pediu para sair (codigo %08X)\n", result.exitCode); return 3;
    default: std::printf("PARADA: %s\n", result.error.c_str()); return 1;
    }
}
