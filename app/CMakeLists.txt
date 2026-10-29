# Host e executaveis do jogo.
# C++ gerado pelo hh-recomp (privado, -DHH_GENERATED_DIR=<pasta>): compilado uma vez numa
# biblioteca e ligado pelas ferramentas de desenvolvimento e pelo Preview (fase hibrida).
if(HH_GENERATED_DIR)
  file(GLOB hh_generated_srcs CONFIGURE_DEPENDS "${HH_GENERATED_DIR}/*.cpp")
  add_library(hh_generated STATIC ${hh_generated_srcs})
  target_include_directories(hh_generated PUBLIC "${HH_GENERATED_DIR}")
  target_link_libraries(hh_generated PUBLIC hh_runtime_cpu)
  # HH07.6: slots em todas as funcoes (o codigo gerado chama pelo slot; mods e provedores
  # nativos trocam a funcao sem editar o gerado). Desligado: so as fronteiras de [native] slots.
  option(HH_MODS "Compilar o codigo gerado com slots em todas as funcoes" OFF)
  if(HH_MODS)
    target_compile_definitions(hh_generated PUBLIC HH_MODS=1)
  endif()
  if(MSVC)
    target_compile_options(hh_generated PRIVATE /bigobj)
  endif()

  # Nucleo de execucao comum (HH06): boot, dados, entrada, VBlank, parada.
  add_library(hh_host STATIC hh_host.cpp hh_discs.cpp hh_verified_files.cpp)
  target_include_directories(hh_host PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
  target_link_libraries(hh_host PUBLIC hh_generated hh_runtime hh_transitional)

  # hh-run: execucao sem janela.
  add_executable(hh-run hh_run.cpp)
  target_link_libraries(hh-run PRIVATE hh_host)

  # hh-diff: testes diferenciais contra estados do Flycast (tools/dev/flycast_oracle.py).
  add_executable(hh-diff ${CMAKE_SOURCE_DIR}/tools/hh-diff/main.cpp)
  target_link_libraries(hh-diff PRIVATE hh_generated hh_runtime)

  # HeadHunter (Preview, HH06): janela SDL3. O SDL3 (zlib) fica restrito a este alvo: o runtime
  # e as ferramentas nao dependem dele.
  option(HH_BUILD_WINDOW "Compilar o Preview com janela (SDL3)" ON)
  if(HH_BUILD_WINDOW)
    include(FetchContent)
    set(SDL_SHARED       OFF CACHE BOOL "" FORCE)
    set(SDL_STATIC       ON  CACHE BOOL "" FORCE)
    set(SDL_TEST_LIBRARY OFF CACHE BOOL "" FORCE)
    set(SDL_EXAMPLES     OFF CACHE BOOL "" FORCE)
    # sem asserts internos: nao gravar caminhos do build no executavel (como no Blue Stinger)
    set(SDL_ASSERTIONS   "disabled" CACHE STRING "" FORCE)
    FetchContent_Declare(SDL3
      GIT_REPOSITORY https://github.com/libsdl-org/SDL.git
      GIT_TAG        release-3.2.30
      GIT_SHALLOW    TRUE)
    FetchContent_MakeAvailable(SDL3)
    # HH15.2: o stub dos diagnosticos de validacao e fonte do proprio produto; como os simbolos ja estao definidos aqui,
    # o linker nao puxa hw_probe.obj nem validacao.obj do hh_runtime (a catraca hh15_produto_papeis confere no mapa).
    add_executable(HeadHunter WIN32 hh_window.cpp ${CMAKE_SOURCE_DIR}/runtime/debug/validacao_stubs.cpp)
    target_link_libraries(HeadHunter PRIVATE hh_host hh_native_install SDL3::SDL3-static shell32 ole32)  # shell32/ole32: pasta Documentos (saves)
    # HH18: identificacao (--version) e mods em C++ do projeto exportado (com HH_MODS: slots em todas as funcoes)
    set(HH_VERSION_OVERRIDE "" CACHE STRING "Versao gravada no HeadHunter.exe (vazio = arquivo VERSION)")
    if(HH_VERSION_OVERRIDE)
      set(hh_version "${HH_VERSION_OVERRIDE}")
    elseif(EXISTS "${PROJECT_SOURCE_DIR}/VERSION")
      file(STRINGS "${PROJECT_SOURCE_DIR}/VERSION" hh_version LIMIT_COUNT 1)
    else()
      set(hh_version "dev")
    endif()
    target_compile_definitions(HeadHunter PRIVATE "HH_VERSION_TEXT=\"${hh_version}\"")
    set(HH_MODS_DIR "" CACHE PATH "Pasta com mods em C++ (*.cpp; projeto exportado, exige HH_MODS)")
    if(HH_MODS AND HH_MODS_DIR)
      file(GLOB hh_mod_srcs CONFIGURE_DEPENDS "${HH_MODS_DIR}/*.cpp")
      target_sources(HeadHunter PRIVATE ${hh_mod_srcs})
    endif()
    if(MSVC)
      # mapa do linker (so no diretorio de build; o Builder copia apenas o .exe): o teste
      # hh07_produto_sem_cdi confere que a leitura de CDI nao entrou no produto
      target_link_options(HeadHunter PRIVATE "/MAP:$<TARGET_FILE_DIR:HeadHunter>/HeadHunter.map")
    endif()
    # Recompilar, nunca emular: nem o executor de passo unico (sh4_core), nem o tradutor, nem o
    # stub sem hardware podem ser alcancados pelo executavel do jogo (falha na configuracao).
    hh_assert_product_clean(HeadHunter)
  endif()
endif()
