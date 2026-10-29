// SPDX-License-Identifier: MIT
#include "hh_host.h"

#include "funcs.h"

#include "boot/native_boot.h"
#include "cpu/sh4_intc.h"
#include "cpu/sh4_runtime.h"
#include "cpu/sh4_threads.h"
#include "memory/sys_logical.h"
#include "system/syscall_service.h"
#include "debug/call_trace.h"
#include "mods/mod_api.h"
#include "gdfs_adapter.h"
#include "time_adapter.h"
#include "pad_probe.h"
#include "pad_adapter.h"
#include "bu_adapter.h"
#include "audio/audio.h"
#include "audio_device.h"
#include "video_adapter.h"
#include "render_capture.h"
#include "kamui_device.h"
#include "platform/saves.h"
#include "input_bridge.h"
#include "hh_verified_files.h"
#include "gdrom/data_source.h"
#include "gdrom/gdrom.h"
#include "hle/bios.h"
#include "input/input.h"
#include "maple/maple.h"
#include "pvr/pvr_video.h"
#include "pvr/vram.h"

#include <chrono>
#include <cstdlib>
#include <memory>
#include <array>
#include <cstdio>
#include <map>
#include <cstring>
#include <stdexcept>
#include <thread>

namespace hh {
namespace {

struct StopRequest {};
struct GameExit {
    uint32_t code;
};
struct FatalError : std::runtime_error {
    using std::runtime_error::runtime_error;
};

const HostOptions* g_opt = nullptr;
// HH10 (portao C1): HH_PAD_TRACE=<arquivo> grava, a cada VBlank, a unidade 0 do PDS_PERIPHERAL
// (52 bytes em 0x8C311DAC) e as leituras do controle: a linha do tempo do que o jogo le, igual nos
// dois provedores de controles; depois, o conteudo apontado por info e name (portao C7)
const uint8_t* g_ram      = nullptr;
std::FILE*     g_padTrace = nullptr;
// HH10 (com HH_PAD_FECHAMENTO, so no legado): VBlanks em que a area nativa do BIOS HLE nao esta como o
// boot a deixa (0xFF): prova dinamica de que o jogo nao usa a faixa (bios.h)
bool           g_areaCheck   = false;
uint64_t       g_areaChanged = 0;
HostResult*        g_res = nullptr;
HostFrame          g_frame;
std::chrono::steady_clock::time_point g_epoch;
constexpr auto kField = std::chrono::nanoseconds(16683333);  // 59,94 Hz

void onFatal(Sh4Context* c, const char* what, uint32_t pc, uint32_t extra) {
    char b[320];
    std::snprintf(b, sizeof b, "%s em PC=%08X (valor %08X) PR=%08X R15=%08X", what, pc, extra, c ? c->pr : 0,
                  c ? c->r[15] : 0);
    throw FatalError(b);
}

// Diagnostico (HH13.6, so desenvolvimento): HH_FN_PERFIL=<arquivo.tsv>;<ini>-<fim> conta as entradas nas funcoes do
// intervalo por ponto de retorno (PR: o chamador e achado depois pelo mapa das funcoes), com o primeiro e o ultimo
// VBlank. Saida privada (pasta ignorada).
struct FnProfile {
    uint32_t                                               lo = 0, hi = 0;
    std::string                                            path;
    std::map<std::pair<uint32_t, uint32_t>, std::array<uint64_t, 3>> hits;  // (funcao, pr) -> {n, primeiro, ultimo}
};
FnProfile* g_fnProfile = nullptr;

void onTrace(Sh4Context* c, uint32_t entry) {
    if (g_fnProfile && entry >= g_fnProfile->lo && entry < g_fnProfile->hi) {
        auto& h = g_fnProfile->hits[{entry, c->pr}];
        if (!h[0]++) h[1] = g_res->vblanks;
        h[2] = g_res->vblanks;
    }
    // limite por endereco (HH_WATCH_MAX, padrao 200): uma funcao frequente nao esconde as outras
    static const uint64_t                  maxHits = [] { const char* v = std::getenv("HH_WATCH_MAX"); return v ? std::strtoull(v, nullptr, 10) : 200ull; }();
    static std::map<uint32_t, uint64_t> hits;
    for (uint32_t w : g_opt->watch)
        if (entry == w && hits[w]++ < maxHits)
            std::printf("watch vblank %llu: %08X r4=%08X r5=%08X r6=%08X r7=%08X pr=%08X\n",
                        (unsigned long long)g_res->vblanks, entry, c->r[4], c->r[5], c->r[6], c->r[7], c->pr);
}

// Poll central: parada pedida mesmo sem VBlank (revisao Codex HH06).
void onService(Sh4Context*) {
    if (g_opt->stop && g_opt->stop->load()) throw StopRequest{};
}

Sh4Context* g_ctx = nullptr;  // HH14: o contexto do jogo para o audio nativo no VBlank

void onVblank(void*) {
    HostResult& r = *g_res;
    ++r.vblanks;
    if (transitional::kamui::mode() == transitional::kamui::Mode::Native)  // HH13.6: quadro a partir do estado nativo
        transitional::kamui::video_frame(g_frame.argb, g_frame.width, g_frame.height);
    else
        pvr_video_frame(g_frame.argb, g_frame.width, g_frame.height);
    if (transitional::audio::installed()) transitional::audio::pump(g_ctx);  // HH14: o som ate este VBlank
    if (transitional::video::installed())  // HH12: o quadro do filme nativo no lugar do quadro do PVR (como no N5 do BS)
        transitional::video::compose_frame(g_frame.argb, g_frame.width, g_frame.height);
    g_frame.vblank = r.vblanks;
    g_frame.reads  = maple_cond_seq(0);
    bool blank     = true;
    for (uint32_t x : g_frame.argb)
        if (x & 0x00FFFFFFu) {
            blank = false;
            break;
        }
    if (!blank && !r.nonBlank++) r.firstNonBlank = r.vblanks;
    if (g_opt->onFrame) g_opt->onFrame(g_frame);
    // Diagnostico (HH12.0, so desenvolvimento): HH_RAM_DUMP=<vblank>,...;<pasta> grava ram_<v>.bin (16 MiB) e
    // vram_<v>.bin (8 MiB, layout de 32 bits) nesses VBlanks. Saida privada (pasta ignorada).
    static const std::string dumpSpec = [] { const char* v = std::getenv("HH_RAM_DUMP"); return std::string(v ? v : ""); }();
    if (!dumpSpec.empty()) {
        const size_t semi = dumpSpec.find(';');
        const std::string list = dumpSpec.substr(0, semi), dir = semi == std::string::npos ? "." : dumpSpec.substr(semi + 1);
        for (size_t i = 0; i < list.size();) {
            const size_t j = list.find(',', i);
            if (std::strtoull(list.c_str() + i, nullptr, 10) == r.vblanks) {
                const std::string n = std::to_string(r.vblanks);
                if (std::FILE* f = std::fopen((dir + "/ram_" + n + ".bin").c_str(), "wb")) std::fwrite(g_ram, 1, 16u << 20, f), std::fclose(f);
                if (std::FILE* f = std::fopen((dir + "/vram_" + n + ".bin").c_str(), "wb")) std::fwrite(vram_data(), 1, 8u << 20, f), std::fclose(f);
            }
            if (j == std::string::npos) break;
            i = j + 1;
        }
    }
    if (g_areaCheck && !transitional::pad::installed())
        for (uint32_t i = 0; i < kHleNativeAreaSize; ++i)
            if (g_ram[(kHleNativeAreaBase & 0xFFFFFF) + i] != 0xFF) {
                ++g_areaChanged;
                break;
            }
    if (g_padTrace) {
        std::fprintf(g_padTrace, "%llu\t%llu\t", static_cast<unsigned long long>(r.vblanks),
                     static_cast<unsigned long long>(transitional::pad::installed() ? transitional::pad::reads() : g_frame.reads));
        for (int i = 0; i < 52; ++i) std::fprintf(g_padTrace, "%02X", g_ram[0x311DAC + i]);
        // portao C7: o conteudo apontado por info (+0x30, 120 bytes) e por name (+0x24, texto), que
        // os provedores poem em areas diferentes
        auto ptr = [](uint32_t off) {
            uint32_t v;
            std::memcpy(&v, g_ram + 0x311DAC + off, 4);
            return (v >> 24) == 0x8C && (v & 0xFFFFFF) + 120 <= (16u << 20) ? g_ram + (v & 0xFFFFFF) : nullptr;
        };
        std::fputc('\t', g_padTrace);
        if (const uint8_t* info = ptr(0x30))
            for (int i = 0; i < 120; ++i) std::fprintf(g_padTrace, "%02X", info[i]);
        std::fputc('\t', g_padTrace);
        if (const uint8_t* name = ptr(0x24))
            for (int i = 0; i < 64 && name[i]; ++i) std::fputc(name[i] >= 0x20 && name[i] < 0x7F ? name[i] : '?', g_padTrace);
        std::fputc('\n', g_padTrace);
    }
    if (g_opt->stop && g_opt->stop->load()) throw StopRequest{};
    if (g_opt->vblankLimit && r.vblanks >= g_opt->vblankLimit) throw StopRequest{};
    if (g_opt->realtime) {  // ritmo do Dreamcast; atrasado mais de 250 ms: recomeca a contagem
        const auto due = g_epoch + kField * r.vblanks;
        const auto now = std::chrono::steady_clock::now();
        if (now < due) std::this_thread::sleep_until(due);
        else if (now - due > std::chrono::milliseconds(250)) g_epoch = now - kField * r.vblanks;
    }
}

} // namespace

HostResult host_run(const HostOptions& opt) {
    HostResult res;
    transitional::BoundaryRegistry                   boundaries;
    std::unique_ptr<native::files::DataFileSystem> files;
    std::unique_ptr<VerifiedFileSystem>            verified;
    std::unique_ptr<native::saves::ISaveStore>     saves;
    std::unique_ptr<native::audio::IAudioOutput>   audioOut;  // HH14: saida do audio do jogo (janela)
    g_opt = &opt;
    g_res = &res;
    hhg::register_functions();
    // HH18: mods em C++ do projeto exportado (HH_MOD, compilados com HH_MODS e HH_MODS_DIR): ativados depois do
    // registro das funcoes geradas, antes do boot (sem mods compilados, nada muda)
    hh::mods::reset();
    hh::mods::init_all();
    for (const std::string& m : hh::mods::registered()) std::fprintf(stderr, "[mods] ativo: %s\n", m.c_str());
    std::vector<uint8_t> ram(16u << 20);
    Sh4Context           c{};
    c.ram = ram.data();
    g_ctx = &c;
    g_ram = ram.data();
    if (const char* v = std::getenv("HH_PAD_TRACE"); v && *v) g_padTrace = std::fopen(v, "w");
    const char* closurePath = std::getenv("HH_PAD_FECHAMENTO");
    g_areaCheck = closurePath && *closurePath, g_areaChanged = 0;
    try {
        sh4_set_fatal_handler(onFatal);
        bios_set_exit_hook([](Sh4Context*, uint32_t code) { throw GameExit{code}; });
        syscall_service_set_exit_hook([](Sh4Context*, uint32_t code) { throw GameExit{code}; });  // HH15.3
        // Relogio fixo (testes deterministicos) ou hora local; 7305 dias = 1950 -> 1970.
        const uint32_t rtc = opt.rtcUnix >= 0 ? uint32_t(opt.rtcUnix + 7305ll * 86400) : boot_rtc_now();
        const BootFiles boot = boot_read_data(opt.data);  // versao pelo SHA-256 do executavel de boot
        if (!opt.expectVersion.empty() && boot.versionId != opt.expectVersion)
            // a identidade vem do executavel, nao da linha "version" do manifesto (revisao Codex HH08)
            throw std::runtime_error("os dados em " + opt.data.string() + " sao da versao " + boot.versionId +
                                     ", esperada " + opt.expectVersion + " (dados de outro disco). Refaca a instalacao com o Builder.");
        // HH09: tempo e eventos pelo provedor nativo (antes do boot: o reset do hardware o inicia)
        if (opt.time != TimeProvider::Legacy) {
            transitional::time::Options to;
            to.experimental = opt.time == TimeProvider::Experimental;
            if (const char* v = std::getenv("HH_TIME_TRACE")) to.trace = v;
            if (const char* v = std::getenv("HH_TEMPO_MUTACAO")) to.mutation = v;
            if (const char* v = std::getenv("HH_PROGRESS_TRACE")) to.progress_trace = v;
            if (const char* v = std::getenv("HH_PROGRESSO_LIMITE")) to.stall_units = std::strtoull(v, nullptr, 10);
            else to.stall_units = transitional::time::kDefaultStallUnits;
            transitional::time::install(to);
        }
        boot_prepare(c, boot, rtc);
        sh4_set_session_context(&c);  // HH15.1: despertar da cadencia B (depois do reset do contexto)
        // HH15.3: syscalls pelo servico nativo (o produto) ou pelo BIOS HLE com o drive simulado (verificacao; o gdFs
        // original do --arquivos legado le setores pelo drive)
        if (opt.files == FileProvider::Legacy && opt.system == SystemProvider::Native)
            throw std::runtime_error("--arquivos legado le setores pelo drive simulado: exige --sistema hle");
        if (opt.system == SystemProvider::Native) syscall_service_install(&c, opt.data);
        else gdrom_set_disc(open_runtime_data(opt.data, {}));
        if (opt.files == FileProvider::Native) {
            // data\disc e o que o Builder extraiu do disco; nada de setor nem de disc_meta
            files = std::make_unique<native::files::DataFileSystem>(opt.data / "disc", opt.modsDir);
            transitional::gdfs::check_sizes(*files);
            for (const auto& m : files->mods())
                std::fprintf(stderr, "[mods] recurso substituido: %s (%llu -> %llu bytes)\n", m.logical.c_str(),
                             static_cast<unsigned long long>(m.originalSize), static_cast<unsigned long long>(m.modSize));
            // cada arquivo e conferido contra o manifesto do Builder na primeira abertura (HH08.6)
            verified = std::make_unique<VerifiedFileSystem>(
                *files, opt.filesManifest.empty() ? opt.data / "disc.manifest" : opt.filesManifest);
            transitional::gdfs::install(boundaries, *verified);
        }
        if (opt.saves == SavesProvider::Native) {
            // HH11: saves pelo provedor nativo (API bu sobre arquivos; sem VMU nem Maple)
            if (opt.savesRoot.empty()) throw std::runtime_error("--saves nativo sem pasta de saves");
            native::saves::FileStoreOptions so;
            so.log = [](const std::string& m) { std::fprintf(stderr, "[saves] %s\n", m.c_str()); };
            saves  = native::saves::make_file_store(opt.savesRoot, so);
            std::fprintf(stderr, "[saves] nativo: %s\n", opt.savesRoot.string().c_str());
            const char* bm = std::getenv("HH_BU_MUTACAO");  // negativos do portao hh11_saves
            transitional::bu::install(boundaries, *saves, transitional::bu::parse_mutations(bm ? bm : ""));
        }
        if (opt.video == VideoProvider::Native) {
            // HH12: filmes pelos MP4 do Builder (data\video) e pelos mods (mods\video); sem o decodificador 4X
            transitional::video::Options vo;
            vo.dataDir = opt.data / "video";
            if (const char* vd = std::getenv("HH_VIDEO_DADOS"); vd && *vd) vo.dataDir = vd;  // portao hh12_video (ausente)
            if (!opt.modsDir.empty()) vo.modsDir = opt.modsDir.parent_path() / "video";
            vo.audio = opt.videoAudio;
            std::vector<std::string> warn;
            transitional::video::install(boundaries, vo, &warn);
            for (const std::string& w : warn) std::fprintf(stderr, "[video] %s\n", w.c_str());
            std::fprintf(stderr, "[video] nativo: %s\n", vo.dataDir.string().c_str());
        }
        // HH14: audio nativo (HH_AUDIO_WAV=<arquivo>: tudo o que o motor produziu; HH_AUDIO_LEITURAS=<tsv>: leituras do
        // SH-4 na RAM de som, diagnostico)
        if (opt.audio == AudioProvider::Native) {
            transitional::audio::Options ao;
            if (opt.audioDevice) audioOut = native::audio::make_xaudio2_output(200, true);
            ao.output = audioOut.get();
            if (const char* w = std::getenv("HH_AUDIO_WAV"); w && *w) ao.wavPath = w;
            if (const char* r = std::getenv("HH_AUDIO_LEITURAS"); r && *r) ao.readsPath = r;
            transitional::audio::install(boundaries, ao);
        }
        // HH13.6: funis do SDK grafico. --render nativo usa o provedor nativo (os funis deixam de tocar o PVR e o render le
        // o estado nativo); o software continua no caminho legado. HH_KAMUI=legado|sombra|nativo escolhe explicitamente
        // (sombra: os originais continuam e um estado nativo e conferido com o do runtime em cada STARTRENDER).
        {
            const char*       k    = std::getenv("HH_KAMUI");
            const std::string kv   = k ? k : "";
            const bool        kNat = kv == "nativo" || (kv.empty() && opt.render == RenderProvider::Native);
            if (kv == "sombra") transitional::kamui::install(boundaries, transitional::kamui::Mode::Shadow);
            else if (kNat) transitional::kamui::install(boundaries, transitional::kamui::Mode::Native);
        }
        // HH13: IR de render capturada do decodificador do TA (HH_RENDER_IR=capturar|conferir|comparar; HH_RENDER_IR_CADA=N
        // confere 1 render a cada N); o rasterizador em software continua a referencia. --render nativo desenha a IR no
        // Vulkan; comparar mantem o software e mede o Vulkan (HH_RENDER_IR_BMP=<pasta>, 1 a cada HH_RENDER_IR_BMP_CADA).
        if (const char* dir = std::getenv("HH_RENDER_IR_BMP"); dir && *dir) {
            const char* cada = std::getenv("HH_RENDER_IR_BMP_CADA");
            transitional::render_capture::set_compare_dump(dir, cada ? uint32_t(std::strtoul(cada, nullptr, 10)) : 300u);
        }
        if (const char* ir = std::getenv("HH_RENDER_IR"); ir && *ir) {
            const char*       cada  = std::getenv("HH_RENDER_IR_CADA");
            const uint32_t    every = cada ? uint32_t(std::strtoul(cada, nullptr, 10)) : 1u;
            const std::string m     = ir;
            using RM                = transitional::render_capture::Mode;
            transitional::render_capture::install(m == "conferir" ? RM::Check : (m == "comparar" ? RM::Compare : RM::Capture),
                                                  every);
        } else if (opt.render == RenderProvider::Native) {
            transitional::render_capture::install(transitional::render_capture::Mode::Native);
        }
        call_trace_install();  // HH_CALL_TRACE=<tsv>;<end>,... (diagnostico; HH08.1)
        transitional::pad::probe_install();  // HH_PAD_CONTRATO=<tsv>: contrato do pd contra o legado (HH10.1)
        transitional::pad::closure_install();  // HH_PAD_FECHAMENTO=<tsv>: prova de fechamento (HH10.4)
        if (opt.controls == ControlsProvider::Native) {
            // HH10: controles pelo provedor nativo do pd; o Maple fica sem controle (a camada per
            // continua, para a VMU ate o HH11). A mutacao "maple" (negativo) deixa o controle no Maple.
            if (!opt.input) throw std::runtime_error("--controles nativo sem fonte de entrada (InputSystem)");
            if (std::getenv("HH_PAD_CONTRATO")) throw std::runtime_error("HH_PAD_CONTRATO confere o servidor legado: use --controles legado");
            const char*    mv  = std::getenv("HH_PAD_NATIVO_MUTACAO");
            const uint32_t mut = transitional::pad::parse_mutations(mv ? mv : "");
            maple_set_controller_present((mut & transitional::pad::kNatMaple) != 0);
            transitional::input_bridge::install((mut & transitional::pad::kNatMaple) ? opt.input : nullptr);
            transitional::pad::install(boundaries, *opt.input, mut);  // coleta no disparo (pad_adapter.h)
        } else if (!input_source()) {
            input_configure_from_env();  // HH_INPUT_SCRIPT=<roteiro> (deterministico)
        }
        g_epoch = std::chrono::steady_clock::now();
        pvr_set_vblank_hook(&onVblank, nullptr);
        if (opt.stop) sh4_set_service_hook(onService);
        static FnProfile fnProfile;
        if (const char* fp = std::getenv("HH_FN_PERFIL"); fp && *fp) {
            const std::string spec = fp;
            const size_t      semi = spec.find(';'), dash = spec.find('-', semi);
            if (semi == std::string::npos || dash == std::string::npos)
                throw std::runtime_error("HH_FN_PERFIL=<arquivo.tsv>;<ini>-<fim> (enderecos em hexa)");
            fnProfile      = FnProfile{};
            fnProfile.path = spec.substr(0, semi);
            fnProfile.lo   = uint32_t(std::stoul(spec.substr(semi + 1, dash - semi - 1), nullptr, 16));
            fnProfile.hi   = uint32_t(std::stoul(spec.substr(dash + 1), nullptr, 16));
            g_fnProfile    = &fnProfile;
        }
        if (!opt.watch.empty() || g_fnProfile) sh4_set_trace(onTrace);
        const Sh4Func entry = sh4_lookup(c.pc);
        if (!entry) throw std::runtime_error("entrada do jogo sem codigo recompilado");
        entry(&c);
        res.end   = HostEnd::Error;
        res.error = "a funcao de entrada do jogo retornou";
    } catch (const StopRequest&) {
        res.end = opt.stop && opt.stop->load() ? HostEnd::Stopped : HostEnd::Limit;
    } catch (const GameExit& e) {
        res.end      = HostEnd::GameExit;
        res.exitCode = e.code;
    } catch (const std::exception& e) {
        res.end   = HostEnd::Error;
        res.error = e.what();
    }
    pvr_set_vblank_hook(nullptr, nullptr);
    sh4_set_service_hook(nullptr);
    sh4_set_session_context(nullptr);
    sh4_set_trace(nullptr);
    if (g_fnProfile) {  // HH_FN_PERFIL
        if (std::FILE* f = std::fopen(g_fnProfile->path.c_str(), "w")) {
            std::fprintf(f, "funcao\tpr\tchamadas\tprimeiro_vblank\tultimo_vblank\n");
            for (const auto& [k, h] : g_fnProfile->hits)
                std::fprintf(f, "%08X\t%08X\t%llu\t%llu\t%llu\n", k.first, k.second, (unsigned long long)h[0],
                             (unsigned long long)h[1], (unsigned long long)h[2]);
            std::fclose(f);
        }
        g_fnProfile = nullptr;
    }
    if (!sh4_threads_shutdown() && res.end != HostEnd::Error) {
        res.end   = HostEnd::Error;
        res.error = "falha ao liberar as fibras das threads do guest";
    }
    call_trace_flush();
    if (g_padTrace) std::fclose(g_padTrace), g_padTrace = nullptr;
    transitional::pad::closure_finish(transitional::pad::installed());
    if (closurePath && *closurePath && transitional::bu::installed())  // HH11: contadores do provedor nativo dos saves
        if (std::FILE* f = std::fopen(closurePath, "a")) {
            const auto b = transitional::bu::stats();
            std::fprintf(f, "bu_load_fnv\t%016llx\nbu_load_bytes\t%llu\n", (unsigned long long)b.lastLoadFnv,
                         (unsigned long long)b.lastLoadBytes);
            std::fprintf(f, "bu_init\t%llu\nbu_passadas\t%llu\nbu_save_ok\t%llu\nbu_save_falha\t%llu\nbu_load_ok\t%llu\n"
                            "bu_load_falha\t%llu\nbu_sem_suporte\t%llu\n",
                         (unsigned long long)b.inits, (unsigned long long)b.ticks, (unsigned long long)b.saveOk,
                         (unsigned long long)b.saveFail, (unsigned long long)b.loadOk, (unsigned long long)b.loadFail,
                         (unsigned long long)b.unsupported);
            std::fclose(f);
        }
    if (g_areaCheck && !transitional::pad::installed())
        if (std::FILE* f = std::fopen(closurePath, "a"))
            std::fprintf(f, "area_nativa_alterada\t%llu\n", static_cast<unsigned long long>(g_areaChanged)), std::fclose(f);
    if (transitional::pad::installed()) {
        std::fputs(transitional::pad::summary().c_str(), stderr);
        transitional::pad::uninstall(boundaries);
        maple_reset();  // fim da sessao: a presenca do controle so muda com o barramento zerado
        maple_set_controller_present(true);
        transitional::input_bridge::install(nullptr);
    }
    if (transitional::bu::installed()) {
        std::fputs(transitional::bu::summary().c_str(), stderr);
        transitional::bu::uninstall(boundaries);
    }
    if (transitional::audio::installed()) {
        std::fputs(transitional::audio::summary().c_str(), stderr);
        transitional::audio::uninstall(boundaries);
    }
    if (transitional::kamui::installed()) {
        std::fputs(transitional::kamui::summary().c_str(), stderr);
        transitional::kamui::uninstall(boundaries);
    }
    if (transitional::video::installed()) {
        std::fputs(transitional::video::summary().c_str(), stderr);
        transitional::video::uninstall(boundaries);
    }
    // HH13: o resumo sai sempre que a captura foi instalada (HH_RENDER_IR / --render nativo), mesmo sem render (revisao Codex)
    if (const char* ir = std::getenv("HH_RENDER_IR"); (ir && *ir) || opt.render == RenderProvider::Native ||
                                                       transitional::render_capture::stats().renders)
        std::fputs(transitional::render_capture::summary().c_str(), stderr);
    transitional::render_capture::install(transitional::render_capture::Mode::Off);  // sempre (revisao Codex do BS)
    if (transitional::time::installed()) {
        transitional::time::uninstall();
        std::fputs(transitional::time::progress_summary().c_str(), stderr);
        const auto s = transitional::time::stats();
        std::fprintf(stderr,
                     "[tempo] %s: VBlank-In %llu, VBlank-Out %llu, render %llu, Maple %llu, GD-ROM %llu; agendados %llu, "
                     "cancelados %llu, recuos do relogio %llu, VBlank-In colapsados %llu\n",
                     opt.time == TimeProvider::Experimental ? "experimental" : "nativo",
                     static_cast<unsigned long long>(s.delivered[0]), static_cast<unsigned long long>(s.delivered[1]),
                     static_cast<unsigned long long>(s.delivered[2]), static_cast<unsigned long long>(s.delivered[3]),
                     static_cast<unsigned long long>(s.delivered[4]), static_cast<unsigned long long>(s.posted),
                     static_cast<unsigned long long>(s.cancelled), static_cast<unsigned long long>(s.clockRegressions),
                     static_cast<unsigned long long>(s.vblankCollapsed));
    }
    std::fputs(sys_logical_summary().c_str(), stderr);  // HH15.1: servicos do tempo logico (vazio fora dele)
    {  // HH15.1: origem das entregas de interrupcao (quantum de ciclos x atendimento pedido x espera do hardware x ldc sr)
        auto n = [](IrqOrigin o) { return static_cast<unsigned long long>(sh4_irq_origin_count(o)); };
        std::fprintf(stderr, "[irq] entregas %llu: quantum %llu, pedido %llu, espera do hardware %llu, ldc sr %llu, despertar %llu\n",
                     static_cast<unsigned long long>(sh4_irq_taken_count()), n(IrqOrigin::Quantum), n(IrqOrigin::Forced),
                     n(IrqOrigin::Hardware), n(IrqOrigin::SrCheck), n(IrqOrigin::Wakeup));
    }
    if (files) {
        const auto fs = transitional::gdfs::stats();
        std::fprintf(stderr,
                     "[arquivos] nativo: %llu aberturas, %llu leituras, %llu bytes, %llu nao encontrados; disco %d\n",
                     static_cast<unsigned long long>(fs.opens), static_cast<unsigned long long>(fs.reads),
                     static_cast<unsigned long long>(fs.bytes), static_cast<unsigned long long>(fs.notFound),
                     transitional::gdfs::current_disc());
        transitional::gdfs::uninstall(boundaries);
        verified.reset();
        files.reset();
    }
    std::fputs(syscall_service_summary().c_str(), stderr);  // HH15.3 (vazio com o HLE)
    syscall_service_uninstall();
    if (opt.system == SystemProvider::Hle) gdrom_set_disc(nullptr);
    return res;
}

} // namespace hh
