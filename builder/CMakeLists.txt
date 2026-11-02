# Builder (HH18, arquitetura do Builder do Blue Stinger, MIT): dos dois CDIs do usuario ao HeadHunter.exe nativo
# (extracao e juncao dos discos, analise, geracao, compilacao com o compilador do usuario, verificacao e instalacao).
# Nao contem nada do jogo. Um nucleo (hh_builder_core) para tres executaveis:
#   - hh-builder: linha de comando (automacao, regressao);
#   - HeadHunter-Recompiler.exe: janela do usuario final (os dois CDIs, pasta, idioma, modo portatil) -> jogo instalado;
#   - HeadHunter-Developer.exe: exportar o projeto C++ editavel (sem compilador), compilar o projeto (mods) e
#     inspecionar recursos, com o log tecnico.
add_library(hh_builder_core STATIC builder_core.cpp toolchain.cpp child_process.cpp project_export.cpp resource_inspect.cpp
            video/fourxm_header.cpp video/fourxm_convert.cpp video/movie_build.cpp)
target_include_directories(hh_builder_core PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(hh_builder_core PUBLIC hh_recomp_lib hh_extract_core hh_native_video PRIVATE hh_native_install tomlplusplus::tomlplusplus)
target_compile_features(hh_builder_core PUBLIC cxx_std_20)
target_compile_definitions(hh_builder_core PUBLIC UNICODE _UNICODE)

# Kit de fontes padrao no repositorio (a distribuicao do Builder traz kit\ ao lado dos executaveis): o proprio projeto
# e as fontes do SDL3 baixadas pelo CMake do app (o toml++ fica ao lado delas, em _deps). So no desenvolvimento: a
# distribuicao (tools/dist/make_builder.ps1) compila com HH_BUILDER_DEV_KIT=OFF, sem caminhos desta maquina no binario.
option(HH_BUILDER_DEV_KIT "Builder usa este repositorio como kit padrao (desenvolvimento)" ON)
set(hh_builder_defs)
if(HH_BUILDER_DEV_KIT)
  set(hh_builder_defs HH_KIT_DIR="${PROJECT_SOURCE_DIR}")
  include(FetchContent)
  FetchContent_GetProperties(SDL3 SOURCE_DIR hh_sdl3_src)  # declarado no CMake do app (propriedade global)
  if(hh_sdl3_src)
    list(APPEND hh_builder_defs HH_SDL3_DIR="${hh_sdl3_src}")
  endif()
endif()

add_executable(hh-builder main_cli.cpp builder.manifest)
target_link_libraries(hh-builder PRIVATE hh_builder_core hh_native_audio_core)
target_compile_definitions(hh-builder PRIVATE ${hh_builder_defs})
hh_icone(hh-builder)

# Textos da interface por chave (pt-BR, en-US; um arquivo por idioma)
add_library(hh_builder_ui STATIC ui/i18n.cpp ui/lang_pt_br.cpp ui/lang_en_us.cpp)
target_include_directories(hh_builder_ui PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_compile_features(hh_builder_ui PUBLIC cxx_std_20)
target_compile_definitions(hh_builder_ui PUBLIC UNICODE _UNICODE)

# Janelas Win32 nativas (controles comuns v6, DPI por monitor), sobre o mesmo hh_builder_core
add_executable(HeadHunter-Recompiler WIN32 main_recompiler.cpp builder.manifest)
target_link_libraries(HeadHunter-Recompiler PRIVATE hh_builder_core hh_builder_ui hh_native_install comctl32)
target_compile_definitions(HeadHunter-Recompiler PRIVATE ${hh_builder_defs})
hh_icone(HeadHunter-Recompiler)
add_executable(HeadHunter-Developer WIN32 main_developer.cpp builder.manifest)
target_link_libraries(HeadHunter-Developer PRIVATE hh_builder_core hh_builder_ui tomlplusplus::tomlplusplus comctl32)
target_compile_definitions(HeadHunter-Developer PRIVATE ${hh_builder_defs})
hh_icone(HeadHunter-Developer)
