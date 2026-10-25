# SPDX-License-Identifier: MIT
# Guarda do produto (ARCHITECTURE.md, "Critério operacional final"):
# bibliotecas marcadas com hh_mark_tool_only() (interpretador SH-4,
# hardware guest, legado) não podem ser ligadas a HeadHunter.exe.
# O alvo do produto chama hh_assert_product_clean(<alvo>) depois de
# declarar suas dependências; a configuração falha se alguma biblioteca
# marcada for alcançável pelas dependências de link diretas ou transitivas.

function(hh_mark_tool_only target)
  set_property(TARGET ${target} PROPERTY HH_TOOL_ONLY TRUE)
endfunction()

function(_hh_collect_links target out_var)
  set(seen ${${out_var}})
  get_target_property(libs ${target} LINK_LIBRARIES)
  if(NOT libs)
    set(libs "")
  endif()
  get_target_property(ilibs ${target} INTERFACE_LINK_LIBRARIES)
  if(ilibs)
    list(APPEND libs ${ilibs})
  endif()
  foreach(lib IN LISTS libs)
    if(TARGET ${lib} AND NOT lib IN_LIST seen)
      list(APPEND seen ${lib})
      set(${out_var} ${seen})
      _hh_collect_links(${lib} ${out_var})
      set(seen ${${out_var}})
    endif()
  endforeach()
  set(${out_var} ${seen} PARENT_SCOPE)
endfunction()

# A verificacao roda no FIM da configuracao do projeto (cmake_language DEFER no diretorio
# raiz): todos os alvos ja existem, inclusive os definidos depois do produto (revisao Codex
# HH06: verificar na hora so via as dependencias ja criadas).
function(hh_assert_product_clean target)
  # Os argumentos de um DEFER so sao avaliados na chamada adiada: o nome vai embutido no codigo.
  cmake_language(EVAL CODE
    "cmake_language(DEFER DIRECTORY [[${CMAKE_SOURCE_DIR}]] CALL _hh_check_product_clean [[${target}]])")
endfunction()

function(_hh_check_product_clean target)
  set(deps "")
  _hh_collect_links(${target} deps)
  foreach(dep IN LISTS deps)
    get_target_property(tool_only ${dep} HH_TOOL_ONLY)
    if(tool_only)
      message(FATAL_ERROR "Produto '${target}' alcança a biblioteca de ferramenta '${dep}' (HH_TOOL_ONLY)")
    endif()
  endforeach()
endfunction()
