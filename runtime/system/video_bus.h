// SPDX-License-Identifier: MIT
#pragma once
// HH15.7d4a: o aviso de VBlank para o host apresentar quadros como servico nativo (antes em pvr/pvr_video.cpp): o banco
// de status chama video_bus_vblank no VBlank-In, depois do estado atualizado, na thread que executa o jogo.
namespace hh {

using VblankHook = void (*)(void* arg);
void video_bus_set_vblank_hook(VblankHook fn, void* arg);  // nullptr desliga
void video_bus_vblank();

} // namespace hh
