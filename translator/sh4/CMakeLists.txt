# Nucleo SH-4 (SH7091) - decodificador declarativo + executor. HH03.
add_library(sh4_core STATIC
  src/decoder.cpp
  src/cpu.cpp
)
target_include_directories(sh4_core PUBLIC include)
target_compile_features(sh4_core PUBLIC cxx_std_20)
# Executor de passo único = interpretador: só ferramentas, testes e oráculos.
hh_mark_tool_only(sh4_core)

source_group(TREE ${CMAKE_CURRENT_SOURCE_DIR} PREFIX "translator/sh4" FILES
  src/decoder.cpp src/cpu.cpp src/decoder_table.inc include/sh4/ops.hpp
  include/sh4/bus.hpp include/sh4/cpu.hpp)
