# SPDX-License-Identifier: MIT
# Icone de todos os executaveis (arquitetura do icone do Blue Stinger, MIT; aqui um modulo porque sao varios alvos):
# app/res/headhunter.ico (desenho original, versionado) ou, se existir e HH_ICONE_LOCAL estiver ligado,
# icone-local/headhunter.ico: feito da tela de titulo da copia do proprio usuario (tools/dev/gerar_icone.py --titulo).
# Esse fica fora do git e so entra em builds locais: e arte do jogo, nao se publica (LEGAL.md).
# A escolha e na configuracao do CMake: por ou tirar o icone local vale na proxima configuracao.
option(HH_ICONE_LOCAL "Usar icone-local/headhunter.ico (arte do jogo: nao publicar) se existir" ON)

get_filename_component(hh_raiz "${CMAKE_CURRENT_LIST_DIR}/.." ABSOLUTE)
set(HH_ICONE "${hh_raiz}/app/res/headhunter.ico")
if(HH_ICONE_LOCAL AND EXISTS "${hh_raiz}/icone-local/headhunter.ico")
  set(HH_ICONE "${hh_raiz}/icone-local/headhunter.ico")
  message(STATUS "icone: icone-local/headhunter.ico (arte do jogo; uso local, nao publicar)")
endif()

set(HH_ICONE_DIR "${CMAKE_BINARY_DIR}/hh_icone")
if(WIN32)
  # o compilador de recursos nao acompanha o .ico: o hash dele no .rc gerado faz a troca do icone recompilar o recurso
  set_property(DIRECTORY "${CMAKE_SOURCE_DIR}" APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS "${HH_ICONE}")
  file(SHA256 "${HH_ICONE}" HH_ICONE_HASH)
  # o .rc cita so o nome do icone, copiado para a pasta do build e achado pelo /I dela: o rc le o .rc gerado (UTF-8) na
  # pagina de codigo ANSI, e um caminho absoluto com acento falharia (RC2135 no Blue Stinger, 2026-10-25)
  configure_file("${HH_ICONE}" "${HH_ICONE_DIR}/headhunter.ico" COPYONLY)
  configure_file("${hh_raiz}/app/res/headhunter.rc.in" "${HH_ICONE_DIR}/headhunter.rc" @ONLY)
endif()

function(hh_icone)
  if(WIN32)
    foreach(alvo IN LISTS ARGN)
      target_sources(${alvo} PRIVATE "${HH_ICONE_DIR}/headhunter.rc")
      target_include_directories(${alvo} PRIVATE "${HH_ICONE_DIR}")
    endforeach()
  endif()
endfunction()
