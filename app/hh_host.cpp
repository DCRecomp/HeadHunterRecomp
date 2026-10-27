// SPDX-License-Identifier: MIT
#include "hh_host.h"

#include "funcs.h"

#include "boot/native_boot.h"
#include "cpu/sh4_runtime.h"
#include "cpu/sh4_threads.h"
#include "debug/call_trace.h"
#include "mods/mod_api.h"
#include "gdfs_adapter.h"
#include "time_adapter.h"
#include "pad_probe.h"
#include "pad_adapter.h"
#include "bu_adapter.h"
#include "video_adapter.h"
#include "render_capture.h"
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
#include <cstdio>
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

void onTrace(Sh4Context* c, uint32_t entry) {
    static uint64_t hits = 0;
    for (uint32_t w : g_opt->watch)
        if (entry == w && hits++ < 200)
            std::printf("watch vblank %llu: %08X r4=%08X r5=%08X r6=%08X r7=%08X pr=%08X\n",
                        (unsigned long long)g_res->vblanks, entry, c->r[4], c->r[5], c->r[6], c->r[7], c->pr);
}

// Poll central: parada pedida mesmo sem VBlank (revisao Codex HH06).
void onService(Sh4Context*) {
    if (g_opt->stop && g_opt->stop->load()) throw StopRequest{};
}

void onVblank(void*) {
    HostResult& r = *g_res;
    ++r.vblanks;
    pvr_video_frame(g_frame.argb, g_frame.width, g_frame.height);
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
    g_ram = ram.data();
    if (const char* v = std::getenv("HH_PAD_TRACE"); v && *v) g_padTrace = std::fopen(v, "w");
    const char* closurePath = std::getenv("HH_PAD_FECHAMENTO");
    g_areaCheck = closurePath && *closurePath, g_areaChanged = 0;
    try {
        sh4_set_fatal_handler(onFatal);
        bios_set_exit_hook([](Sh4Context*, uint32_t code) { throw GameExit{code}; });
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
        gdrom_set_disc(open_runtime_data(opt.data, {}));
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
        if (!opt.watch.empty()) sh4_set_trace(onTrace);
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
    sh4_set_trace(nullptr);
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
        maple_set_controller_present(true);
        transitional::input_bridge::install(nullptr);
    }
    if (transitional::bu::installed()) {
        std::fputs(transitional::bu::summary().c_str(), stderr);
        transitional::bu::uninstall(boundaries);
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
    gdrom_set_disc(nullptr);
    return res;
}

} // namespace hh
