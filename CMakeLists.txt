cmake_minimum_required(VERSION 3.20)
project(HeadHunterRecompiled LANGUAGES CXX)

# Default build type
if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Release)
endif()

# Enable C++20
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
if(MSVC)
  # MSVC define __cplusplus como 199711L por padrao; reportar o valor real.
  add_compile_options(/Zc:__cplusplus)
endif()

list(APPEND CMAKE_MODULE_PATH ${CMAKE_CURRENT_SOURCE_DIR}/cmake)
include(HHProductGuard)
include(CTest)

if(MSVC)
  add_compile_options(/utf-8)
endif()

# toml++ (MIT): configuracao do tradutor (HH04), como no Blue Stinger.
include(FetchContent)
FetchContent_Declare(tomlplusplus
  GIT_REPOSITORY https://github.com/marzer/tomlplusplus.git
  GIT_TAG        v3.4.0
  GIT_SHALLOW    TRUE)
FetchContent_MakeAvailable(tomlplusplus)
# C++ gerado pelo hh-recomp a partir do executavel do usuario (privado, generated/).
# Vazio: nao compila o jogo nem os testes que dependem dele.
set(HH_GENERATED_DIR "" CACHE PATH "C++ gerado pelo hh-recomp (vazio = nao compilar)")

option(BUILD_TESTS "Compilar testes" ON)
# HH18: o Builder (Recompiler/Developer/hh-builder) compila so o jogo a partir do kit ou do projeto exportado:
# HH_BUILD_TOOLS=OFF deixa de fora o proprio Builder e a documentacao (o resto e preciso para o HeadHunter.exe).
option(HH_BUILD_TOOLS "Compilar o Builder e a documentacao" ON)
add_subdirectory(app)
if(HH_BUILD_TOOLS)
  add_subdirectory(builder)
  add_subdirectory(docs)
endif()
add_subdirectory(config)
add_subdirectory(legacy)
add_subdirectory(native)
add_subdirectory(runtime)
if(BUILD_TESTS)
  add_subdirectory(tests)
endif()
add_subdirectory(tools)
add_subdirectory(transitional)
add_subdirectory(translator)

