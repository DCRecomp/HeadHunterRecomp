# Tradutor (decodificador, analise e codegen AOT). Nada daqui entra no produto.
# sh4/: decodificador declarativo + executor de passo unico do HH03 (oraculo).
add_subdirectory(sh4)

# Decodificador, analise e gerador portados do Blue Stinger (MIT, HH04).
add_library(hh_translator STATIC
  decoder/sh4_decoder.cpp
  analysis/analysis.cpp
  codegen/codegen.cpp
)
target_include_directories(hh_translator PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
hh_mark_tool_only(hh_translator)

# Analise + geracao como biblioteca: hh-recomp e, no HH06, o Builder.
add_library(hh_recomp_lib STATIC app/config.cpp app/recomp.cpp)
target_include_directories(hh_recomp_lib PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(hh_recomp_lib PUBLIC hh_translator PRIVATE tomlplusplus::tomlplusplus hh_extract_core)
hh_mark_tool_only(hh_recomp_lib)

add_executable(hh-recomp app/main.cpp)
target_link_libraries(hh-recomp PRIVATE hh_recomp_lib)
hh_icone(hh-recomp)

add_executable(hh-disasm app/disasm_main.cpp)
target_link_libraries(hh-disasm PRIVATE hh_translator)
hh_icone(hh-disasm)

# HH07: varredura de dependencias de hardware (manifesto, grafo, bibliotecas do SDK, fronteira,
# fechamento, metricas). Portado do Blue Stinger (bs-hwscan, MIT). Ferramenta de analise.
add_library(hh_hwscan STATIC
  hwscan/hwscan.cpp
  hwscan/sdkid.cpp
  hwscan/closure.cpp
  hwscan/metrics.cpp
  hwscan/boundary_io.cpp
  hwscan/pipeline.cpp
  hwscan/probe.cpp
)
target_include_directories(hh_hwscan PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/hwscan)
target_link_libraries(hh_hwscan PUBLIC hh_translator hh_dcmap hh_recomp_lib PRIVATE tomlplusplus::tomlplusplus)
hh_mark_tool_only(hh_hwscan)

add_executable(hh-hwscan hwscan/main.cpp)
target_link_libraries(hh-hwscan PRIVATE hh_hwscan)
hh_icone(hh-hwscan)
