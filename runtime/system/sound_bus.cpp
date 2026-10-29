// SPDX-License-Identifier: MIT
// HH15.6: area de som nativa (ver sound_bus.h). O efeito no contrato e o do modelo aica/aica_regs.cpp, que agora
// delega a ela o controle e o RTC (conferido pelas referencias da HH15 e pelos portoes do HH09 e do HH14).
#include "system/sound_bus.h"

#include "cpu/sh4_runtime.h"
#include "cpu/time_hooks.h"
#include "debug/clock_log.h"

#include <ctime>

namespace hh {
namespace {

constexpr uint32_t kCtrl  = 0x00702C00;
constexpr uint32_t kRtcHi = 0x00710000;
constexpr uint32_t kRtcLo = 0x00710004;
constexpr uint32_t kRtcEn = 0x00710008;

uint32_t g_ctrl      = 0;
bool     g_rtcEnable = false;
bool     g_rtcSet    = false;  // sem valor definido: hora do host no primeiro acesso

uint32_t canon(uint32_t a) { return a < 0xE0000000 ? (a & 0x1FFFFFFF) : 0; }
bool     inArea(uint32_t p) { return (p >= 0x00700000 && p < 0x00720000) || (p >= 0x00800000 && p < 0x01000000); }

uint32_t rtcNow(uint64_t now) {
    if (!g_rtcSet) sound_bus_rtc_set(sound_bus_rtc_host_now(), now);
    return time_provider().rtc_seconds(now);
}

} // namespace

void sound_bus_reset() {
    g_ctrl      = 0;
    g_rtcEnable = false;
    sound_bus_rtc_set(sound_bus_rtc_host_now(), 0);
}

void sound_bus_rtc_set(uint32_t seconds, uint64_t now) {
    time_provider().rtc_set(seconds, now);
    g_rtcSet = true;
}

uint32_t sound_bus_rtc_host_now() {
    const std::time_t now = std::time(nullptr);
    std::tm           local{}, utc{};
#ifdef _WIN32
    localtime_s(&local, &now);
    gmtime_s(&utc, &now);
#else
    localtime_r(&now, &local);
    gmtime_r(&now, &utc);
#endif
    const std::time_t offset = std::mktime(&local) - std::mktime(&utc);  // fuso + horario de verao
    constexpr int64_t epoch1950 = int64_t(20 * 365 + 5) * 24 * 60 * 60;  // 1950 -> 1970
    return uint32_t(epoch1950 + int64_t(now) + int64_t(offset));
}

bool sound_bus_read(Sh4Context* c, uint32_t a, unsigned size, uint32_t& v) {
    const uint32_t p = canon(a);
    if (!inArea(p)) return false;
    if (size != 4) sh4_fatal(c, "area de som: leitura com tamanho fora do contrato", c->pc, a);
    if (p == kCtrl) v = g_ctrl;
    else if (p == kRtcHi) v = rtcNow(time_now(c)) >> 16;
    else if (p == kRtcLo) v = rtcNow(time_now(c)) & 0xFFFF;
    else if (p == kRtcEn) v = 0;
    else sh4_fatal(c, "area de som: leitura fora do contrato (AICA ou RAM de som pela CPU)", c->pc, a);
    if (g_clock_log && (p == kRtcHi || p == kRtcLo)) clock_log_read(ClockRead::Rtc, v);
    return true;
}

bool sound_bus_write(Sh4Context* c, uint32_t a, unsigned size, uint32_t v) {
    const uint32_t p = canon(a);
    if (!inArea(p)) return false;
    if (size != 4) sh4_fatal(c, "area de som: escrita com tamanho fora do contrato", c->pc, a);
    if (p != kCtrl && p != kRtcHi && p != kRtcLo && p != kRtcEn)
        sh4_fatal(c, "area de som: escrita fora do contrato (AICA ou RAM de som pela CPU)", c->pc, a);
    if (g_clock_log) clock_log_read(ClockRead::Aud, p), clock_log_read(ClockRead::Aud, v);
    if (p == kCtrl) {
        g_ctrl = v;
    } else if (p == kRtcEn) {
        g_rtcEnable = v & 1;
    } else if (g_rtcEnable) {
        const uint32_t now = rtcNow(time_now(c));
        const uint32_t val = p == kRtcHi ? (now & 0xFFFF) | ((v & 0xFFFF) << 16) : (now & 0xFFFF0000) | (v & 0xFFFF);
        sound_bus_rtc_set(val, time_now(c));
        if (p == kRtcHi) g_rtcEnable = false;
    }
    return true;
}

} // namespace hh
