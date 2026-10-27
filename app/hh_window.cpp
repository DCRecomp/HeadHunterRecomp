// SPDX-License-Identifier: MIT
// HeadHunter (Preview, HH06): janela SDL3 sobre o nucleo de execucao (hh_host). O jogo roda
// numa thread propria no ritmo do Dreamcast (59,94 VBlanks por segundo); a janela apresenta o
// quadro mais recente na frequencia do monitor, com area logica 4:3 e barras. Entrada (HH10,
// --controles nativo, padrao): nucleo nativo (native/platform/input.h) com o teclado da janela e
// os controles do SDL3 Gamepad (ou XInput, se o controles.toml pedir), mapa em controles.toml ao
// lado do executavel; no legado, teclado + XInput do runtime. Com HH_INPUT_SCRIPT, so o roteiro
// (modo espectador).
//
//   HeadHunter [--data <pasta>] [--vblanks N] [--rtc <segundos Unix>] [--log <arquivo> --hash-every K]
//              [--arquivos legado|nativo] [--tempo legado|nativo|experimental] [--controles legado|nativo]
//              [--saves legado|nativo] [--saves-pasta <pasta> | --saves-portatil] [--video legado|nativo]
//              [--mods <pasta>]
// Render (HH13, --render nativo, padrao na janela): a IR do TA desenhada no Vulkan, igual ao software na rota de
// referencia; sem Vulkan ou se o dispositivo cair, o software (o --headless fica no software, salvo --render nativo).
// Filmes (HH12, --video nativo, padrao na janela): os MP4 que o Builder converteu dos 4X (data\video), com audio
// pelo XAudio2 e mods em mods\video; o --headless fica no legado (roteiro de referencia), salvo --video nativo.
// Saves (HH11, --saves nativo, padrao na janela): Documentos\DC-Recomp\Head Hunter\saves (pasta Documentos do
// Windows, que segue o redirecionamento), --saves-pasta <pasta> ou --saves-portatil (saves\ ao lado do
// executavel). --headless (verificacao da instalacao contra o roteiro de referencia, gravado sem memory card)
// usa o legado, salvo --saves nativo (entao um cartao temporario, sem tocar nos saves do usuario).
// Formato do Dreamcast (HH11.4; o arquivo do save e o arquivo da VMU, sem transformacao):
//   --saves-exportar <pasta>                      cada save vira <pasta>\<NOME>.VMS
//   --saves-importar <arquivo.vms> [--saves-nome <NOME>]  (padrao: o nome do arquivo, em maiusculas)
// operam na pasta dos saves (as mesmas opcoes acima) e saem sem abrir o jogo.
// Os dois discos estao juntos em data\ (HH08): nao ha troca de disco; o jogo continua do disco
// 1 para o 2 sozinho.
// --log grava "quadro N: LxA hash H" a cada K VBlanks (mesmo hash do hh-run): prova que a
// execucao na janela e a mesma do modo headless com o mesmo roteiro.
// Pasta de dados padrao: data\ ao lado do executavel. Teclas padrao (runtime/input):
// setas = direcional, ENTER = START, Z/X/A/S = A/B/X/Y. ESC fecha.
//
// Fase hibrida: liga o runtime temporario (removido do produto no HH15).
#include "hh_discs.h"
#include "hh_host.h"

#include "input/input.h"
#include "input_bridge.h"
#include "platform/input.h"
#include "platform/install_config.h"
#include "platform/saves.h"
#include "hh_verified_files.h"

#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>

#include <process.h>
#include <shlobj.h>

#include <atomic>
#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <memory>
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

namespace ni = hh::native::input;

std::string readFile(const fs::path& p) {
    std::ifstream f(p, std::ios::binary);
    if (!f) throw std::runtime_error("nao foi possivel ler " + p.string());
    return std::string((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
}

// Controles pelo SDL3 (HH10, como no Blue Stinger N3-1): o SDL traz o banco de mapeamentos de
// Xbox, PlayStation, Switch e genericos; os nomes de botoes e eixos do SDL sao os do controles.toml.
void gamepadEvent(ni::InputSystem& sys, const SDL_Event& e) {
    switch (e.type) {
    case SDL_EVENT_GAMEPAD_ADDED:
        if (SDL_Gamepad* g = SDL_OpenGamepad(e.gdevice.which)) {
            const char* n        = SDL_GetGamepadName(g);
            char        guid[33] = {};  // perfis por GUID ([perfis."<guid>"] no controles.toml)
            SDL_GUIDToString(SDL_GetGamepadGUIDForID(e.gdevice.which), guid, sizeof guid);
            sys.gamepad_added(e.gdevice.which, n ? n : "controle", guid);
        }
        break;
    case SDL_EVENT_GAMEPAD_REMOVED:
        sys.gamepad_removed(e.gdevice.which);
        if (SDL_Gamepad* g = SDL_GetGamepadFromID(e.gdevice.which)) SDL_CloseGamepad(g);
        break;
    case SDL_EVENT_GAMEPAD_BUTTON_DOWN:
    case SDL_EVENT_GAMEPAD_BUTTON_UP:
        if (const char* n = SDL_GetGamepadStringForButton(SDL_GamepadButton(e.gbutton.button)))
            sys.gamepad_button(e.gbutton.which, n, e.type == SDL_EVENT_GAMEPAD_BUTTON_DOWN);
        break;
    case SDL_EVENT_GAMEPAD_AXIS_MOTION:
        if (const char* n = SDL_GetGamepadStringForAxis(SDL_GamepadAxis(e.gaxis.axis))) {
            const bool trig = e.gaxis.axis == SDL_GAMEPAD_AXIS_LEFT_TRIGGER || e.gaxis.axis == SDL_GAMEPAD_AXIS_RIGHT_TRIGGER;
            sys.gamepad_axis(e.gaxis.which, n, trig ? ni::trigger_from_s16(e.gaxis.value) : ni::axis_from_s16(e.gaxis.value));
        }
        break;
    default: break;
    }
}

fs::path exeDir() {
    const char* base = SDL_GetBasePath();
    return base ? fs::path(reinterpret_cast<const char8_t*>(base)) : fs::current_path();
}

// Pasta padrao dos saves: Documentos\DC-Recomp\Head Hunter\saves (precedente do Blue Stinger); sem a pasta
// Documentos, saves\ ao lado do executavel.
fs::path defaultSavesRoot() {
    PWSTR    docs = nullptr;
    fs::path root;
    if (SUCCEEDED(SHGetKnownFolderPath(FOLDERID_Documents, KF_FLAG_CREATE, nullptr, &docs)) && docs)
        root = fs::path(docs) / L"DC-Recomp" / L"Head Hunter" / L"saves";
    CoTaskMemFree(docs);
    if (root.empty()) {
        root = exeDir() / "saves";
        std::fprintf(stderr, "[saves] pasta Documentos indisponivel: usando %s\n", root.string().c_str());
    }
    return root;
}

struct TempSaves {  // cartao temporario do --headless com --saves nativo (apagado no fim)
    fs::path dir;
    ~TempSaves() {
        std::error_code ec;
        if (!dir.empty()) fs::remove_all(dir, ec);
    }
};

// controles.toml ao lado do executavel (criado com o texto padrao se faltar). Arquivo com erro:
// valem os controles padrao, com aviso.
ni::ActionMap loadActionMap() {
    const fs::path  path = exeDir() / "controles.toml";
    std::error_code ec;
    if (!fs::exists(path, ec)) {
        std::ofstream out(path, std::ios::binary);
        out << ni::ActionMap::default_text();
    }
    ni::ActionMap m = ni::ActionMap::defaults();
    if (fs::exists(path, ec)) {
        std::string err;
        auto        parsed = ni::ActionMap::parse(readFile(path), &err);
        if (parsed) m = parsed.value();
        else std::fprintf(stderr, "[controles] %s tem um erro e foi ignorado (valem os padrao): %s\n",
                          path.string().c_str(), err.c_str());
    }
    return m;
}

} // namespace

#ifndef HH_VERSION_TEXT
#define HH_VERSION_TEXT "dev"
#endif

int main(int argc, char** argv) {
    // HH18 (verificacao do Builder): --version imprime a identificacao; --verify-data confere todos os arquivos de data\
    // contra o manifesto do Builder (sem abrir o jogo). Os dois saem na hora.
    if (argc >= 2 && std::string(argv[1]) == "--version") {
        std::printf("Head Hunter Recompiled %s\n", HH_VERSION_TEXT);
        return 0;
    }
    if (argc >= 2 && std::string(argv[1]) == "--verify-data") {
        try {
            const fs::path            data = argc >= 4 && std::string(argv[2]) == "--data" ? fs::path(argv[3]) : exeDir() / "data";
            const hh::InstallData     inst = hh::resolve_install(data);
            if (!inst.merged) {  // revisao Codex: a verificacao da instalacao completa exige o disco 2 junto
                std::printf("PARADA: dados so do disco 1 em %s (falta arquivos.manifest)\n", data.string().c_str());
                return 1;
            }
            hh::native::files::DataFileSystem files(data / "disc", {});
            hh::VerifiedFileSystem    verified(files, inst.manifest);
            const std::string         bad = verified.verify_all();
            if (!bad.empty()) {
                std::printf("PARADA: dados diferentes do manifesto: %s\n", bad.c_str());
                return 1;
            }
            std::printf("ok: %zu arquivos conferidos (%s)\n", verified.verified(), inst.merged ? "discos 1 e 2" : "disco 1");
            return 0;
        } catch (const std::exception& e) {
            std::printf("PARADA: %s\n", e.what());
            return 1;
        }
    }
    hh::HostOptions opt;
    std::string     logPath;
    uint64_t        hashEvery = 0;
    bool            headless  = false;
    bool            savesSet = false, savesPortable = false, videoSet = false, renderSet = false;
    fs::path        vmsExport, vmsImport;
    std::string     vmsName;
    opt.realtime = true;
    opt.saves    = hh::SavesProvider::Native;
    opt.video    = hh::VideoProvider::Native;  // HH12: filmes nativos padrao na janela (promovido; legado como reserva)
    opt.render   = hh::RenderProvider::Native;  // HH13: Vulkan padrao na janela (recuo automatico para o software)
    opt.data     = exeDir() / "data";
    for (int i = 1; i < argc; ++i) {
        const std::string a = argv[i];
        if (a == "--data" && i + 1 < argc) opt.data = argv[++i];
        else if (a == "--vblanks" && i + 1 < argc) opt.vblankLimit = std::strtoull(argv[++i], nullptr, 10);
        else if (a == "--rtc" && i + 1 < argc) opt.rtcUnix = std::strtoll(argv[++i], nullptr, 10);
        else if (a == "--log" && i + 1 < argc) logPath = argv[++i];
        else if (a == "--hash-every" && i + 1 < argc) hashEvery = std::strtoull(argv[++i], nullptr, 10);
        else if (a == "--headless") headless = true;
        else if (a == "--arquivos" && i + 1 < argc &&
                 (std::string(argv[i + 1]) == "legado" || std::string(argv[i + 1]) == "nativo"))
            opt.files = std::string(argv[++i]) == "nativo" ? hh::FileProvider::Native : hh::FileProvider::Legacy;
        else if (a == "--tempo" && i + 1 < argc &&
                 (std::string(argv[i + 1]) == "legado" || std::string(argv[i + 1]) == "nativo" ||
                  std::string(argv[i + 1]) == "experimental")) {
            const std::string t = argv[++i];
            opt.time = t == "nativo" ? hh::TimeProvider::Native
                       : t == "experimental" ? hh::TimeProvider::Experimental : hh::TimeProvider::Legacy;
        }
        else if (a == "--mods" && i + 1 < argc) opt.modsDir = argv[++i];
        else if (a == "--controles" && i + 1 < argc &&
                 (std::string(argv[i + 1]) == "legado" || std::string(argv[i + 1]) == "nativo"))
            opt.controls = std::string(argv[++i]) == "nativo" ? hh::ControlsProvider::Native : hh::ControlsProvider::Legacy;
        else if (a == "--saves" && i + 1 < argc &&
                 (std::string(argv[i + 1]) == "legado" || std::string(argv[i + 1]) == "nativo"))
            opt.saves = std::string(argv[++i]) == "nativo" ? hh::SavesProvider::Native : hh::SavesProvider::Legacy, savesSet = true;
        else if (a == "--saves-pasta" && i + 1 < argc) opt.savesRoot = argv[++i];
        else if (a == "--video" && i + 1 < argc &&
                 (std::string(argv[i + 1]) == "legado" || std::string(argv[i + 1]) == "nativo"))
            opt.video = std::string(argv[++i]) == "nativo" ? hh::VideoProvider::Native : hh::VideoProvider::Legacy, videoSet = true;
        else if (a == "--render" && i + 1 < argc &&
                 (std::string(argv[i + 1]) == "software" || std::string(argv[i + 1]) == "nativo"))
            opt.render = std::string(argv[++i]) == "nativo" ? hh::RenderProvider::Native : hh::RenderProvider::Software, renderSet = true;
        else if (a == "--saves-portatil") savesPortable = true;
        else if (a == "--saves-exportar" && i + 1 < argc) vmsExport = argv[++i];
        else if (a == "--saves-importar" && i + 1 < argc) vmsImport = argv[++i];
        else if (a == "--saves-nome" && i + 1 < argc) vmsName = argv[++i];
        else {
            std::fprintf(stderr, "uso: HeadHunter [--data <pasta>] [--vblanks N] [--rtc <segundos Unix>] "
                                 "[--log <arquivo> --hash-every K] [--headless] [--arquivos legado|nativo] "
                                 "[--tempo legado|nativo|experimental] [--controles legado|nativo] [--saves legado|nativo] "
                                 "[--saves-pasta <pasta> | --saves-portatil] [--video legado|nativo] [--render software|nativo] "
                                 "[--mods <pasta>]\n"
                                 "       HeadHunter [--saves-pasta <pasta> | --saves-portatil] --saves-exportar <pasta>\n"
                                 "       HeadHunter [--saves-pasta <pasta> | --saves-portatil] --saves-importar <arquivo.vms> "
                                 "[--saves-nome <NOME>]\n");
            return 2;
        }
    }
    if (!vmsExport.empty() || !vmsImport.empty()) {
        namespace ns = hh::native::saves;
        // precedencia: --saves-pasta > --saves-portatil > instalacao.toml > Documentos (revisao Codex HH18)
        if (opt.savesRoot.empty() && !savesPortable) {
            std::string warn;
            if (hh::native::install::read_config(exeDir(), &warn).saves == hh::native::install::SavesLocation::Portable)
                savesPortable = true;
            if (!warn.empty()) std::fprintf(stderr, "[saves] %s\n", warn.c_str());
        }
        const fs::path root = !opt.savesRoot.empty() ? opt.savesRoot : savesPortable ? exeDir() / "saves" : defaultSavesRoot();
        ns::FileStoreOptions so;
        so.log     = [](const std::string& m) { std::fprintf(stderr, "[saves] %s\n", m.c_str()); };
        auto store = ns::make_file_store(root, so);
        if (!vmsImport.empty()) {
            std::string n = vmsName.empty() ? vmsImport.filename().replace_extension().string() : vmsName;
            for (char& ch : n) ch = char(std::toupper(static_cast<unsigned char>(ch)));
            const ns::SaveError e = ns::valid_save_name(n) ? ns::import_vms(*store, 0, n, vmsImport) : ns::SaveError::BadName;
            std::printf(e == ns::SaveError::None ? "importado: %s -> %s\n" : "PARADA: %s -> %s: ", vmsImport.string().c_str(),
                        n.c_str());
            if (e != ns::SaveError::None) std::printf("%s\n", ns::save_error_name(e));
            return e == ns::SaveError::None ? 0 : 1;
        }
        std::vector<ns::SaveEntry> list;
        if (const ns::SaveError e = store->list(0, list); e != ns::SaveError::None) {
            std::printf("PARADA: %s: %s\n", root.string().c_str(), ns::save_error_name(e));
            return 1;
        }
        std::error_code ec;
        fs::create_directories(vmsExport, ec);
        int failed = 0;
        for (const auto& s : list) {
            const fs::path      out = vmsExport / (s.name + ".VMS");
            const ns::SaveError e   = ns::export_vms(*store, 0, s.name, out);
            if (e == ns::SaveError::None) std::printf("exportado: %s -> %s\n", s.name.c_str(), out.string().c_str());
            else std::printf("PARADA: %s: %s\n", s.name.c_str(), ns::save_error_name(e)), ++failed;
        }
        return failed ? 1 : 0;
    }
    try {
        const hh::InstallData inst = hh::resolve_install(opt.data);
        opt.filesManifest          = inst.manifest;
        opt.expectVersion          = hh::kInstallBootVersion;
        // o produto e o jogo inteiro: sem o disco 2 junto, a instalacao esta incompleta
        if (!inst.merged)
            throw std::runtime_error("a instalacao em " + opt.data.string() +
                                     " nao tem o disco 2 (falta arquivos.manifest). Refaca a instalacao com o Builder "
                                     "informando os dois discos.");
        // mods de recursos: mods\assets ao lado do executavel (opcional; mesma arvore de data\disc)
        if (opt.modsDir.empty()) opt.modsDir = exeDir() / "mods" / "assets";
    } catch (const std::exception& e) {
        std::printf("PARADA: %s\n", e.what());
        return 1;
    }
    // --headless (verificacao da instalacao pelo Builder e pela regressao): o mesmo executavel e
    // o mesmo nucleo, sem janela nem ritmo de tempo real; exige um limite de VBlanks.
    if (headless && !opt.vblankLimit) {
        std::fprintf(stderr, "--headless exige --vblanks\n");
        return 2;
    }
    TempSaves tempSaves;
    if (headless && !savesSet) opt.saves = hh::SavesProvider::Legacy;
    // o --headless (verificacao do Builder e roteiro de referencia, gravado com os filmes legados) fica no legado
    if (headless && !videoSet) opt.video = hh::VideoProvider::Legacy;
    if (headless && !renderSet) opt.render = hh::RenderProvider::Software;  // HH13: verificacao sem depender da GPU
    opt.videoAudio = !headless;  // HH12: audio dos filmes nativos so com a janela (headless: saida nula)
    // modo portatil gravado pelo Recompiler (instalacao.toml ao lado do executavel; vale ao abrir o jogo direto)
    {
        std::string warn;
        const auto  ic = hh::native::install::read_config(exeDir(), &warn);
        if (!warn.empty()) std::fprintf(stderr, "[saves] %s\n", warn.c_str());
        if (ic.saves == hh::native::install::SavesLocation::Portable) savesPortable = true;
    }
    if (opt.saves == hh::SavesProvider::Native && opt.savesRoot.empty()) {
        if (savesPortable) opt.savesRoot = exeDir() / "saves";
        else if (headless) {
            tempSaves.dir = fs::temp_directory_path() / ("hh-saves-headless-" + std::to_string(_getpid()));
            std::error_code ec;
            fs::remove_all(tempSaves.dir, ec);
            opt.savesRoot = tempSaves.dir;
        } else opt.savesRoot = defaultSavesRoot();
    }
    std::FILE* log = nullptr;
    if (!logPath.empty()) {
        if (!hashEvery || !(log = std::fopen(logPath.c_str(), "w"))) {
            std::fprintf(stderr, "nao foi possivel gravar o log %s (exige --hash-every)\n", logPath.c_str());
            return 2;
        }
    }

    // Entrada. Nativo: InputSystem com o roteiro (espectador; ponte do runtime/input), nenhum
    // dispositivo no headless (controle presente e neutro) ou o teclado da janela + controles.
    // Legado: roteiro ou teclado da janela + XInput do runtime/input.
    hh::KeyEventInput*               keys = nullptr;
    std::unique_ptr<ni::InputSystem> input;
    const char*                      script = std::getenv("HH_INPUT_SCRIPT");
    if (opt.controls == hh::ControlsProvider::Native) {
        try {
            if (script && *script) {
                input = std::make_unique<ni::InputSystem>();
                input->set_test_source(hh::transitional::input_bridge::from_runtime_source(
                    std::shared_ptr<hh::InputSource>(hh::make_scripted_input(readFile(script)))));
            } else if (headless) {
                input = std::make_unique<ni::InputSystem>();
            } else {
                input = std::make_unique<ni::InputSystem>(loadActionMap());
                if (input->map().options.backend == "xinput") input->enable_xinput();
            }
        } catch (const std::exception& e) {
            std::printf("PARADA: %s\n", e.what());
            return 1;
        }
        opt.input = input.get();
    } else if (script || headless) {
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
        if (!SDL_Init(SDL_INIT_VIDEO | (input ? SDL_INIT_GAMEPAD : 0))) {
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
                    else if (const std::string n = keyName(e.key.key); !n.empty()) {
                        if (input) input->key(n, e.type == SDL_EVENT_KEY_DOWN);
                        else if (keys) keys->key(n, e.type == SDL_EVENT_KEY_DOWN);
                    }
                } else if (e.type == SDL_EVENT_WINDOW_FOCUS_LOST) {
                    if (input) input->release_keyboard();
                    else if (keys) keys->releaseAll();
                } else if (input) {
                    gamepadEvent(*input, e);
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
