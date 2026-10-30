// SPDX-License-Identifier: MIT
// HH15.7d4a: o aviso de VBlank como servico nativo (system/video_bus.h). Movido de pvr/pvr_video.cpp sem mudanca.
#include "system/video_bus.h"

namespace hh {
namespace {

VblankHook g_hook    = nullptr;
void*      g_hookArg = nullptr;

} // namespace

void video_bus_set_vblank_hook(VblankHook fn, void* arg) {
    g_hook    = fn;
    g_hookArg = arg;
}

void video_bus_vblank() {
    if (g_hook) g_hook(g_hookArg);
}

} // namespace hh
