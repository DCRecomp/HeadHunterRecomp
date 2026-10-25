# Runtime hibrido. No HH04 so existe o nucleo da CPU usado pelo codigo gerado:
# contexto SH-4, despacho e verificacoes de fluxo (portados do Blue Stinger, MIT).
add_library(hh_runtime_cpu STATIC
  cpu/sh4_context.cpp
  cpu/sh4_runtime.cpp
  mods/mod_api.cpp
)
target_include_directories(hh_runtime_cpu PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_compile_features(hh_runtime_cpu PUBLIC cxx_std_20)

# Pontos de contato com o hardware sem hardware (HH04): todo acesso e fatal.
# Substituido pelo hardware temporario no HH05; nunca entra no produto.
add_library(hh_runtime_hw_m3 STATIC cpu/sh4_hw_m3.cpp)
target_link_libraries(hh_runtime_hw_m3 PUBLIC hh_runtime_cpu)
hh_mark_tool_only(hh_runtime_hw_m3)

# HH05: runtime hibrido temporario (portado do Blue Stinger, MIT): memoria, Holly/PVR/TA,
# GD-ROM e BIOS por HLE sobre data/, Maple, registradores AICA, threads do guest, boot
# nativo e diagnosticos. Sera removido do produto no HH15.
add_library(hh_runtime STATIC
  cpu/sh4_intc.cpp
  cpu/sh4_threads.cpp
  cpu/sh4_service.cpp
  cpu/time_legacy.cpp
  memory/memory.cpp
  memory/sh4_port.cpp
  memory/sh4_tmu.cpp
  memory/sh4_ccn.cpp
  memory/sh4_sq.cpp
  memory/sh4_dmac.cpp
  pvr/pvr_regs.cpp
  pvr/pvr_render.cpp
  pvr/pvr_video.cpp
  pvr/pvr_texture.cpp
  pvr/holly_intc.cpp
  pvr/holly_sb.cpp
  pvr/vram.cpp
  pvr/ta.cpp
  maple/maple.cpp
  input/input.cpp
  aica/aica_regs.cpp
  hle/bios.cpp
  hle/flashrom.cpp
  gdrom/gdrom.cpp
  gdrom/data_source.cpp
  boot/native_boot.cpp
  debug/snapshot.cpp
  debug/hw_probe.cpp
  debug/event_trace.cpp
  debug/call_trace.cpp
  debug/file_log.cpp
  debug/clock_log.cpp
)
target_link_libraries(hh_runtime PUBLIC hh_runtime_cpu hh_extract_core PRIVATE hh_dcmap xinput)
