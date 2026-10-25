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
