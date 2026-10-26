# SPDX-License-Identifier: MIT
# HH08 (revisao Codex) / HH18: uma pasta HeadHunter que nao foi criada pelo Builder nunca perde nada. Com o nucleo do
# Builder do Blue Stinger (builder_core.install_swap) ela nao e mais recusada: numa instalacao bem-sucedida o
# conteudo atual vai para HeadHunter.anterior e os arquivos do usuario passam para a nova (a janela avisa antes).
# Aqui o build falha nas entradas (CDIs inexistentes) e a pasta tem de ficar intacta.
# Uso: cmake -DBUILDER=<hh-builder.exe> -DWORK=<pasta temporaria> -P check_builder_unmarked.cmake
file(REMOVE_RECURSE "${WORK}")
file(MAKE_DIRECTORY "${WORK}/HeadHunter")
file(WRITE "${WORK}/HeadHunter/HeadHunter.exe" "nao e do builder")
file(WRITE "${WORK}/HeadHunter/meu_arquivo.txt" "dado do usuario")
execute_process(COMMAND "${BUILDER}" --disc "${WORK}/nao_existe.cdi" --disc2 "${WORK}/nao_existe2.cdi" --out "${WORK}"
                RESULT_VARIABLE rc OUTPUT_VARIABLE out ERROR_VARIABLE err)
if(rc EQUAL 0)
  message(FATAL_ERROR "o Builder terminou com sucesso sem os CDIs")
endif()
if(NOT EXISTS "${WORK}/HeadHunter/meu_arquivo.txt" OR NOT EXISTS "${WORK}/HeadHunter/HeadHunter.exe")
  message(FATAL_ERROR "o Builder mexeu numa pasta que nao e dele")
endif()
string(FIND "${out}${err}" "imagem nao encontrada" pos)
if(pos EQUAL -1)
  message(FATAL_ERROR "mensagem inesperada: ${out}${err}")
endif()
file(REMOVE_RECURSE "${WORK}")
message(STATUS "pasta alheia intacta depois de um build que falhou nas entradas")
