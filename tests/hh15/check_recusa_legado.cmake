# SPDX-License-Identifier: MIT
# HH15.3-HH15.5: o produto recusa os provedores legados que dependem do hardware simulado (drive, Maple com controle,
# VMU, decodificador 4X sobre a AICA e o PVR, AICA) com saida 2 e mensagem, antes de qualquer dado; sem a opcao a mesma
# chamada nao sai com 2. Os provedores nativos continuam aceitos.
# Uso: cmake -DEXE=<HeadHunter.exe> -P check_recusa_legado.cmake
if(NOT EXISTS "${EXE}")
  message(FATAL_ERROR "executavel ausente: ${EXE}")
endif()
set(base --headless --vblanks 1 --data hh15_sem_dados)
set(bad "")
foreach(opt IN ITEMS arquivos controles saves video audio)
  execute_process(COMMAND "${EXE}" ${base} --${opt} legado
                  RESULT_VARIABLE rc OUTPUT_VARIABLE out ERROR_VARIABLE err TIMEOUT 60)
  if(NOT rc EQUAL 2 OR NOT "${out}${err}" MATCHES "--${opt} legado exige .*nao existe no produto")
    list(APPEND bad "--${opt} legado: saida ${rc}")
  endif()
  execute_process(COMMAND "${EXE}" ${base} --${opt} nativo RESULT_VARIABLE rc OUTPUT_QUIET ERROR_QUIET TIMEOUT 60)
  if(rc EQUAL 2)
    list(APPEND bad "--${opt} nativo tambem sai com 2 (o teste nao distinguiria a recusa)")
  endif()
endforeach()
if(bad)
  string(REPLACE ";" "\n  " text "${bad}")
  message(FATAL_ERROR "o produto nao recusa os provedores legados:\n  ${text}")
endif()
message(STATUS "produto recusa os 5 provedores legados")
