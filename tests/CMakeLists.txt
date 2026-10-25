# Testes.
add_executable(hh_smoke smoke.cpp)
add_test(NAME hh_smoke COMMAND hh_smoke)

add_executable(hh_extract_tests hh_extract_tests.cpp)
target_link_libraries(hh_extract_tests PRIVATE hh_extract_core)
add_test(NAME hh_extract_tests COMMAND hh_extract_tests)

add_executable(hh03_decoder_tests hh03_decoder_tests.cpp)
target_link_libraries(hh03_decoder_tests PRIVATE sh4_core)
add_test(NAME hh03_decoder_tests COMMAND hh03_decoder_tests)

add_executable(hh03_semantics_tests hh03_semantics_tests.cpp)
target_link_libraries(hh03_semantics_tests PRIVATE sh4_core)
add_test(NAME hh03_semantics_tests COMMAND hh03_semantics_tests)

add_executable(hh03_probe hh03_probe.cpp)
target_link_libraries(hh03_probe PRIVATE sh4_core)

# Guarda do produto: biblioteca HH_TOOL_ONLY alcançável (transitiva) falha a configuração.
add_test(NAME hh_product_guard_rejects
  COMMAND ${CMAKE_COMMAND} -S ${CMAKE_CURRENT_SOURCE_DIR}/cmake_guard
          -B ${CMAKE_CURRENT_BINARY_DIR}/cmake_guard_neg)
set_tests_properties(hh_product_guard_rejects PROPERTIES WILL_FAIL TRUE)
add_test(NAME hh_product_guard_rejects_late
  COMMAND ${CMAKE_COMMAND} -S ${CMAKE_CURRENT_SOURCE_DIR}/cmake_guard
          -B ${CMAKE_CURRENT_BINARY_DIR}/cmake_guard_late -DHH_GUARD_LATE=ON)
set_tests_properties(hh_product_guard_rejects_late PROPERTIES WILL_FAIL TRUE)
add_test(NAME hh_product_guard_accepts
  COMMAND ${CMAKE_COMMAND} -S ${CMAKE_CURRENT_SOURCE_DIR}/cmake_guard
          -B ${CMAKE_CURRENT_BINARY_DIR}/cmake_guard_pos -DHH_GUARD_EXPECT_CLEAN=ON)

# HH04: testes do tradutor portados do Blue Stinger (decodificador, analise, operacoes).
add_executable(hh_translator_tests
  unit/main.cpp
  unit/test_decoder.cpp
  unit/test_analysis.cpp
  unit/test_analysis_m4.cpp
  unit/test_ops.cpp
  hh04_decoder_cross.cpp
  hh04_runtime_m3_tests.cpp
  hh06_analysis_tests.cpp
)
target_include_directories(hh_translator_tests PRIVATE unit)
target_link_libraries(hh_translator_tests PRIVATE hh_translator hh_runtime_hw_m3 sh4_core)
add_test(NAME hh_translator_tests COMMAND hh_translator_tests)

# HH04: gerar C++ de um programa SH-4 sintetico em tempo de build, compilar e executar.
add_executable(hh_fixture_gen codegen/fixture_gen.cpp)
target_link_libraries(hh_fixture_gen PRIVATE hh_translator)
foreach(inline 0 1)
  set(gen_dir "${CMAKE_CURRENT_BINARY_DIR}/fixture_gen_${inline}")
  add_custom_command(
    OUTPUT "${gen_dir}/funcs_000.cpp" "${gen_dir}/dispatch.cpp" "${gen_dir}/funcs.h"
    COMMAND hh_fixture_gen "${gen_dir}" ${inline}
    DEPENDS hh_fixture_gen
    COMMENT "gerando C++ do programa sintetico (inline_pool=${inline})")
  add_executable(hh_codegen_exec_${inline} codegen/test_exec.cpp "${gen_dir}/funcs_000.cpp" "${gen_dir}/dispatch.cpp")
  target_include_directories(hh_codegen_exec_${inline} PRIVATE "${gen_dir}")
  target_link_libraries(hh_codegen_exec_${inline} PRIVATE hh_runtime_hw_m3)
  add_test(NAME hh_codegen_exec_${inline} COMMAND hh_codegen_exec_${inline})
endforeach()

# HH04.8: compilar e linkar o C++ gerado do executavel real (privado, em generated/).
# Opcional: -DHH_GENERATED_DIR=<pasta do hh-recomp --out>.
if(HH_GENERATED_DIR)
  add_executable(hh_generated_link hh04_generated_link.cpp)
  # Desde o HH05 o codigo gerado registra pontos de reentrada ([threads]): runtime real.
  target_link_libraries(hh_generated_link PRIVATE hh_generated hh_runtime)
  add_test(NAME hh_generated_link COMMAND hh_generated_link)
endif()

# HH05: testes do runtime hibrido (setjmp/longjmp entre threads do guest, portado do Blue Stinger).
add_executable(hh_runtime_tests unit/main.cpp unit/test_longjmp.cpp hh05_bios_exit_tests.cpp hh05_gdrom_getscd_tests.cpp hh06_service_hook_tests.cpp)
target_include_directories(hh_runtime_tests PRIVATE unit)
target_link_libraries(hh_runtime_tests PRIVATE hh_translator hh_runtime)
add_test(NAME hh_runtime_tests COMMAND hh_runtime_tests)

# HH05.5: trace do boot contra o Flycast (dados privados; sem HH_SNAPSHOT_DIR/HH_DATA_DIR = SKIP).
if(HH_GENERATED_DIR)
  find_package(Python3 COMPONENTS Interpreter REQUIRED)
  add_test(NAME hh_boot_regression
    COMMAND ${Python3_EXECUTABLE} -I ${CMAKE_CURRENT_SOURCE_DIR}/diff/hh_boot_regression.py
            $<TARGET_FILE:hh-diff> ${PROJECT_SOURCE_DIR}/tools/dev/hh05_boot_check.py
            ${CMAKE_CURRENT_SOURCE_DIR}/diff/hh_boot_accepted.tsv)
  set_tests_properties(hh_boot_regression PROPERTIES SKIP_RETURN_CODE 77 TIMEOUT 900)
endif()

# HH05.6: primeiro quadro (logo SEGA) contra o Flycast (dados privados; sem eles = SKIP).
if(HH_GENERATED_DIR)
  add_test(NAME hh_first_frame
    COMMAND ${Python3_EXECUTABLE} -I ${CMAKE_CURRENT_SOURCE_DIR}/diff/hh_first_frame.py
            $<TARGET_FILE:hh-run> ${PROJECT_SOURCE_DIR}/tools/dev/hh05_fb.py)
  set_tests_properties(hh_first_frame PROPERTIES SKIP_RETURN_CODE 77 TIMEOUT 900)
endif()

# HH05: comparadores dos portoes com dados sinteticos (sempre; casos positivos e negativos).
find_package(Python3 COMPONENTS Interpreter REQUIRED)
add_test(NAME hh_gate_comparators
  COMMAND ${Python3_EXECUTABLE} -I ${CMAKE_CURRENT_SOURCE_DIR}/diff/test_gates.py
          ${PROJECT_SOURCE_DIR}/tools/dev/hh05_boot_check.py ${PROJECT_SOURCE_DIR}/tools/dev/hh05_fb.py
          ${CMAKE_CURRENT_SOURCE_DIR}/diff/hh_boot_accepted.tsv)
