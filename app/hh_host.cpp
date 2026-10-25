// SPDX-License-Identifier: MIT
#include "hh_host.h"

#include "funcs.h"

#include "boot/native_boot.h"
#include "cpu/sh4_runtime.h"
#include "cpu/sh4_threads.h"
#include "gdrom/data_source.h"
#include "gdrom/gdrom.h"
#include "hle/bios.h"
#include "input/input.h"
#include "maple/maple.h"
#include "pvr/pvr_video.h"

#include <chrono>
#include <cstdio>
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
    g_opt = &opt;
    g_res = &res;
    hhg::register_functions();
    std::vector<uint8_t> ram(16u << 20);
    Sh4Context           c{};
    c.ram = ram.data();
    try {
        sh4_set_fatal_handler(onFatal);
        bios_set_exit_hook([](Sh4Context*, uint32_t code) { throw GameExit{code}; });
        // Relogio fixo (testes deterministicos) ou hora local; 7305 dias = 1950 -> 1970.
        const uint32_t rtc = opt.rtcUnix >= 0 ? uint32_t(opt.rtcUnix + 7305ll * 86400) : boot_rtc_now();
        boot_prepare(c, boot_read_data(opt.data), rtc);
        gdrom_set_disc(open_runtime_data(opt.data, {}));
        if (!input_source()) input_configure_from_env();  // HH_INPUT_SCRIPT=<roteiro> (deterministico)
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
    gdrom_set_disc(nullptr);
    return res;
}

} // namespace hh
