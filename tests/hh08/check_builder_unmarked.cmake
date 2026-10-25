# SPDX-License-Identifier: MIT
# HH08 (revisao Codex): o Builder recusa uma pasta HeadHunter sem o marcador dele, mesmo com um
# HeadHunter.exe dentro, antes de ler o CDI; e nao apaga nada dela.
# Uso: cmake -DBUILDER=<hh-builder.exe> -DWORK=<pasta temporaria> -P check_builder_unmarked.cmake
file(REMOVE_RECURSE "${WORK}")
file(MAKE_DIRECTORY "${WORK}/HeadHunter")
file(WRITE "${WORK}/HeadHunter/HeadHunter.exe" "nao e do builder")
file(WRITE "${WORK}/HeadHunter/meu_arquivo.txt" "dado do usuario")
execute_process(COMMAND "${BUILDER}" --disc "${WORK}/nao_existe.cdi" --out "${WORK}"
                RESULT_VARIABLE rc OUTPUT_VARIABLE out ERROR_VARIABLE err)
if(rc EQUAL 0)
  message(FATAL_ERROR "o Builder aceitou uma instalacao sem marcador")
endif()
if(NOT EXISTS "${WORK}/HeadHunter/meu_arquivo.txt")
  message(FATAL_ERROR "o Builder apagou arquivos de uma pasta que nao e dele")
endif()
string(FIND "${out}${err}" "nao foi criada pelo Builder" pos)
if(pos EQUAL -1)
  message(FATAL_ERROR "mensagem inesperada: ${out}${err}")
endif()
file(REMOVE_RECURSE "${WORK}")
message(STATUS "pasta sem marcador recusada e intacta")
