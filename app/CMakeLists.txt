# Host e executaveis do jogo.
# C++ gerado pelo hh-recomp (privado, -DHH_GENERATED_DIR=<pasta>): compilado uma vez numa
# biblioteca e ligado pelas ferramentas de desenvolvimento e pelo Preview (fase hibrida).
if(HH_GENERATED_DIR)
  file(GLOB hh_generated_srcs CONFIGURE_DEPENDS "${HH_GENERATED_DIR}/*.cpp")
  add_library(hh_generated STATIC ${hh_generated_srcs})
  target_include_directories(hh_generated PUBLIC "${HH_GENERATED_DIR}")
  target_link_libraries(hh_generated PUBLIC hh_runtime_cpu)
  if(MSVC)
    target_compile_options(hh_generated PRIVATE /bigobj)
  endif()

  # Nucleo de execucao comum (HH06): boot, dados, entrada, VBlank, parada.
  add_library(hh_host STATIC hh_host.cpp)
  target_include_directories(hh_host PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
  target_link_libraries(hh_host PUBLIC hh_generated hh_runtime)

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
    add_executable(HeadHunter WIN32 hh_window.cpp)
    target_link_libraries(HeadHunter PRIVATE hh_host SDL3::SDL3-static)
    # Recompilar, nunca emular: nem o executor de passo unico (sh4_core), nem o tradutor, nem o
    # stub sem hardware podem ser alcancados pelo executavel do jogo (falha na configuracao).
    hh_assert_product_clean(HeadHunter)
  endif()
endif()
