# SPDX-License-Identifier: MIT
# HH15.2: o produto recusa as variaveis de teste (sonda, ruido de ciclos, mutacoes) com saida 2 e mensagem, antes de
# qualquer dado; sem elas a mesma chamada nao sai com 2. Uso: cmake -DEXE=<HeadHunter.exe> -P check_recusa.cmake
if(NOT EXISTS "${EXE}")
  message(FATAL_ERROR "executavel ausente: ${EXE}")
endif()
set(bad "")
foreach(var HH_HW_PROBE HH_CICLOS_RUIDO HH_CADENCIA HH_TEMPO_MUTACAO HH_ARQUIVOS_MUTACAO HH_TMU_MUTACAO)
  execute_process(COMMAND ${CMAKE_COMMAND} -E env ${var}=1 "${EXE}" --headless --data hh15_sem_dados
                  RESULT_VARIABLE rc OUTPUT_VARIABLE out ERROR_VARIABLE err TIMEOUT 60)
  if(NOT rc EQUAL 2 OR NOT "${out}${err}" MATCHES "${var} e uma variavel de teste")
    list(APPEND bad "${var}: saida ${rc}")
  endif()
endforeach()
execute_process(COMMAND "${EXE}" --headless --data hh15_sem_dados RESULT_VARIABLE rc OUTPUT_QUIET ERROR_QUIET TIMEOUT 60)
if(rc EQUAL 2)
  list(APPEND bad "sem variavel de teste a chamada tambem sai com 2 (o teste nao distinguiria a recusa)")
endif()
if(bad)
  string(REPLACE ";" "\n  " text "${bad}")
  message(FATAL_ERROR "o produto nao recusa as variaveis de teste:\n  ${text}")
endif()
message(STATUS "produto recusa as variaveis de teste")
