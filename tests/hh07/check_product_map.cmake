# SPDX-License-Identifier: MIT
# Derivado de recomp-BStinger tests/n0/check_product_map.cmake (MIT).
# HH07.8: o executavel do jogo nao contem a leitura de CDI (hh_extract_core: cdi.cpp,
# extract.cpp; o Builder e as ferramentas usam). Le o mapa do linker do HeadHunter.exe e falha
# se algum simbolo ou objeto dela aparece. Uso: cmake -DMAP=<map> -P check_product_map.cmake
if(NOT EXISTS "${MAP}")
  message(FATAL_ERROR "mapa do linker ausente: ${MAP} (compile o alvo HeadHunter)")
endif()
file(READ "${MAP}" text)
set(bad "")
foreach(sym CdiImage cdi.obj extract.obj prepare_runtime_data verify_runtime_data)
  string(FIND "${text}" "${sym}" pos)
  if(NOT pos EQUAL -1)
    list(APPEND bad "${sym}")
  endif()
endforeach()
if(bad)
  message(FATAL_ERROR "o produto contem a leitura de CDI: ${bad}")
endif()
string(FIND "${text}" "boot_read_data" ok)
if(ok EQUAL -1)
  message(FATAL_ERROR "mapa sem boot_read_data: mapa de outro executavel?")
endif()
message(STATUS "produto sem leitura de CDI")
