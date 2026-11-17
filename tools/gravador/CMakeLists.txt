# SPDX-License-Identifier: MIT
# HH16.1: a tela da gravacao da campanha (inicia o monitor tools/dev/campanha.py, que abre o jogo gravando; fechar o jogo
# para a gravacao). Ferramenta do workspace, fora do produto e da distribuicao.
add_executable(HeadHunter-Gravador WIN32 gravador.cpp gravador.manifest)
target_compile_definitions(HeadHunter-Gravador PRIVATE UNICODE _UNICODE)
target_link_libraries(HeadHunter-Gravador PRIVATE comctl32 shell32)
find_package(Python3 COMPONENTS Interpreter QUIET)
if(Python3_FOUND)
  # o Python deste build, se o PATH nao tiver um (HH_CAMPANHA_PYTHON vale antes dos dois)
  file(TO_CMAKE_PATH "${Python3_EXECUTABLE}" hh_python_padrao)
  target_compile_definitions(HeadHunter-Gravador PRIVATE HH_PYTHON_PADRAO="${hh_python_padrao}")
endif()
hh_icone(HeadHunter-Gravador)
# copia em dist/ (ignorada), ao lado da Preview: o gravador acha o workspace subindo da propria pasta
add_custom_command(TARGET HeadHunter-Gravador POST_BUILD
  COMMAND ${CMAKE_COMMAND} -E make_directory "${PROJECT_SOURCE_DIR}/dist"
  COMMAND ${CMAKE_COMMAND} -E copy_if_different "$<TARGET_FILE:HeadHunter-Gravador>" "${PROJECT_SOURCE_DIR}/dist/HeadHunter-Gravador.exe"
  VERBATIM)
