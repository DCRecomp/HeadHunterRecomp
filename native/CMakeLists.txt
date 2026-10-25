# Sistemas nativos definitivos (ARCHITECTURE.md, docs/planos/HH07-PLANO.md). No HH07 so as
# interfaces: C++20 puro, sem contexto do SH-4, enderecos do guest ou registradores (conferido
# pelo teste hh07_native_limpo, que tambem le este arquivo). Implementacoes chegam do HH08 ao HH14.
add_library(hh_native_api INTERFACE)
target_include_directories(hh_native_api INTERFACE ${CMAKE_CURRENT_SOURCE_DIR})
target_compile_features(hh_native_api INTERFACE cxx_std_20)

if(BUILD_TESTS)
  # Compila todos os cabecalhos so com native/ no include path: nada do legado pode vazar.
  add_executable(hh_native_api_check ${PROJECT_SOURCE_DIR}/tests/hh07/native_api_check.cpp)
  target_link_libraries(hh_native_api_check PRIVATE hh_native_api)
endif()

# HH08: arquivos do jogo sobre arquivos comuns (data\disc + mods\assets).
add_library(hh_native_files STATIC platform/data_files.cpp)
target_link_libraries(hh_native_files PUBLIC hh_native_api)

# HH09: relogio logico, fila de eventos e monitor de progresso (portados do N2a/N2b do BS).
add_library(hh_native_time STATIC platform/clock.cpp platform/event_queue.cpp platform/progress.cpp)
target_link_libraries(hh_native_time PUBLIC hh_native_api)

# HH10: entrada nativa (SDL3/XInput, mapa de acoes em TOML, perfis), portada do N3 do BS.
add_library(hh_native_input STATIC platform/input_map.cpp platform/input_system.cpp)
target_link_libraries(hh_native_input PUBLIC hh_native_api PRIVATE tomlplusplus::tomlplusplus xinput)
