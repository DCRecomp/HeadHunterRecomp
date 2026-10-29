# SPDX-License-Identifier: MIT
# HH15.3-HH15.5: o produto recusa os provedores legados que dependem do hardware simulado (drive, Maple com controle,
# VMU, decodificador 4X sobre a AICA e o PVR, AICA) com saida 2 e mensagem, antes de qualquer dado; sem a opcao a mesma
# chamada nao sai com 2. Os provedores nativos continuam aceitos.
# Uso: cmake -DEXE=<HeadHunter.exe> -P check_recusa_legado.cmake
if(NOT EXISTS "${EXE}")
  message(FATAL_ERROR "executavel ausente: ${EXE}")
endif()
set(saves "${CMAKE_CURRENT_BINARY_DIR}/hh15_recusa_saves")
# HH15.6 (revisao Codex): a recusa vale em todos os modos do produto, inclusive os que saem antes do jogo
# (--verify-data, exportacao e importacao de saves), e com a opcao em qualquer posicao
set(modes "jogo" "verify" "exportar" "importar")
set(base_jogo --headless --vblanks 1 --data hh15_sem_dados)
set(base_verify --verify-data --data hh15_sem_dados)
set(base_exportar --saves-pasta "${saves}/raiz" --saves-exportar "${saves}/saida")
set(base_importar --saves-pasta "${saves}/raiz" --saves-importar "${saves}/inexistente.vms")
set(bad "")
foreach(mode IN LISTS modes)
  foreach(opt IN ITEMS arquivos controles saves video audio)
    execute_process(COMMAND "${EXE}" ${base_${mode}} --${opt} legado
                    RESULT_VARIABLE rc OUTPUT_VARIABLE out ERROR_VARIABLE err TIMEOUT 60)
    if(NOT rc EQUAL 2 OR NOT "${out}${err}" MATCHES "--${opt} legado exige .*nao existe no produto")
      list(APPEND bad "${mode} --${opt} legado: saida ${rc}")
    endif()
    execute_process(COMMAND "${EXE}" ${base_${mode}} --${opt} nativo RESULT_VARIABLE rc OUTPUT_QUIET ERROR_QUIET TIMEOUT 60)
    if(rc EQUAL 2)
      list(APPEND bad "${mode} --${opt} nativo tambem sai com 2 (o teste nao distinguiria a recusa)")
    endif()
  endforeach()
endforeach()
if(bad)
  string(REPLACE ";" "\n  " text "${bad}")
  message(FATAL_ERROR "o produto nao recusa os provedores legados:\n  ${text}")
endif()
message(STATUS "produto recusa os 5 provedores legados nos 4 modos")
