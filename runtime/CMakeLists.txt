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
# nativo e diagnosticos. Sai do produto nos cortes do HH15.
#
# HH15.2 (docs/planos/HH15-PLANO.md; modelo: N8.2 do BS): o runtime e montado de bibliotecas de objetos por papel, com
# os mesmos objetos de antes. O manifesto (papel, fonte) vai para runtime_papeis.tsv no build e a catraca
# hh15_produto_papeis o confere contra o mapa do linker do HeadHunter.exe.
#   aot         substrato do codigo gerado (servico/cadencia, threads do guest, ponte de interrupcoes, RAM e roteamento)
#   nativo      servicos do tempo logico (HH15.1), servico nativo das syscalls (HH15.3), barramento de perifericos sem
#               dispositivos (HH15.4) e area de som nativa (HH15.6)
#   simulacao   hardware simulado ainda no caminho do produto (cortes HH15.3-HH15.7)
#   estavel     tempo estavel por ciclos e modelos de recuo dos servicos fora do contrato
#   diagnostico instrumentacao das referencias, inerte sem a variavel (compartilhada, como no BS)
#   validacao   so o executavel de verificacao: sonda de hardware, ruido de ciclos, cadencia de referencia e mutacoes
#               do runtime; o produto compila debug/validacao_stubs.cpp no lugar (app/CMakeLists.txt)
#   hle         so a verificacao e as ferramentas (HH15.3): BIOS HLE, flash e drive de GD-ROM simulados e a fonte de
#               setores; o produto compila system/hle_ausente.cpp no lugar e usa o servico nativo das syscalls
#   perifericos so a verificacao (HH15.4): o modelo do Maple com o controle do --controles legado; o produto compila
#               system/maple_ausente.cpp no lugar (barramento sem dispositivos, system/maple_bus)
#   aica        so a verificacao (HH15.6): o modelo da AICA (RAM de som do --audio legado, diagnosticos HH_AICA_*); o
#               produto compila system/aica_ausente.cpp no lugar (area de som nativa, system/sound_bus)
#   sb          so a verificacao (HH15.7d): os modelos do system block do Holly e do DMAC (o resto da regiao, os
#               negativos HH_SB_MUTACAO e o canal 2 para o TA do runtime no HH_KAMUI=legado/sombra); o produto compila
#               system/sb_ausente.cpp e system/dmac_ausente.cpp no lugar (o contrato medido: system/sb_bus e
#               system/dma_bus)
#   intc        so a verificacao (HH15.7d2): o controlador de interrupcoes anterior, referencia do banco de status nativo
#               (em sombra, comparado a cada operacao); o produto compila system/event_bus.cpp no lugar (o banco nativo,
#               native/render/kamui/event_status)
set(hh_rt_aot         cpu/sh4_service.cpp cpu/sh4_threads.cpp cpu/irq_bridge.cpp memory/memory.cpp)
set(hh_rt_nativo      memory/sys_logical.cpp system/syscall_service.cpp system/maple_bus.cpp system/sound_bus.cpp
                      system/dma_bus.cpp system/sb_bus.cpp)
set(hh_rt_simulacao   memory/sh4_sq.cpp pvr/pvr_regs.cpp pvr/pvr_render.cpp pvr/pvr_video.cpp
                      pvr/pvr_texture.cpp pvr/vram.cpp pvr/ta.cpp
                      input/input.cpp boot/native_boot.cpp)
set(hh_rt_hle         hle/bios.cpp hle/flashrom.cpp gdrom/gdrom.cpp gdrom/data_source.cpp)
set(hh_rt_perifericos maple/maple.cpp)
set(hh_rt_aica        aica/aica_regs.cpp)
set(hh_rt_sb          pvr/holly_sb.cpp memory/sh4_dmac.cpp)
set(hh_rt_intc        pvr/holly_intc.cpp)
set(hh_rt_estavel     cpu/time_legacy.cpp cpu/sh4_intc.cpp memory/sh4_port.cpp memory/sh4_tmu.cpp memory/sh4_ccn.cpp)
set(hh_rt_diagnostico debug/snapshot.cpp debug/event_trace.cpp debug/call_trace.cpp debug/file_log.cpp
                      debug/clock_log.cpp)
set(hh_rt_validacao   debug/hw_probe.cpp debug/validacao.cpp)
set(hh_rt_papeis aot nativo simulacao estavel diagnostico validacao hle perifericos aica sb intc)
set(hh_rt_manifesto "papel\tfonte\n")
set(hh_rt_objetos "")
foreach(papel IN LISTS hh_rt_papeis)
  add_library(hh_rt_${papel} OBJECT ${hh_rt_${papel}})
  target_link_libraries(hh_rt_${papel} PUBLIC hh_runtime_cpu hh_extract_core hh_native_render_soft hh_native_render_kamui
                        PRIVATE hh_dcmap)
  list(APPEND hh_rt_objetos $<TARGET_OBJECTS:hh_rt_${papel}>)
  foreach(f IN LISTS hh_rt_${papel})
    string(APPEND hh_rt_manifesto "${papel}\t${f}\n")
  endforeach()
endforeach()
file(WRITE "${CMAKE_BINARY_DIR}/runtime_papeis.tsv" "${hh_rt_manifesto}")
add_library(hh_runtime STATIC ${hh_rt_objetos})
target_link_libraries(hh_runtime PUBLIC hh_runtime_cpu hh_extract_core hh_native_render_soft hh_native_render_kamui
                      PRIVATE hh_dcmap xinput)
