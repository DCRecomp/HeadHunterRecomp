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

# HH07: hh-hwscan (varredura, sdkid, fronteira/fechamento, dependencias de hardware declaradas,
# catraca, sonda), com programas sinteticos; portado do N0 do Blue Stinger (MIT).
add_executable(hh07_hwscan_tests unit/main.cpp unit/test_hh07_hwscan.cpp)
target_include_directories(hh07_hwscan_tests PRIVATE unit)
target_link_libraries(hh07_hwscan_tests PRIVATE hh_hwscan hh_runtime)
add_test(NAME hh07_hwscan_tests COMMAND hh07_hwscan_tests WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR})

# HH07.3/HH07.4: fechamento + catraca sobre o executavel real com a sonda (dados privados; sem = SKIP).
find_package(Python3 COMPONENTS Interpreter REQUIRED)
add_test(NAME hh07_fechamento
  COMMAND ${Python3_EXECUTABLE} -I ${CMAKE_CURRENT_SOURCE_DIR}/diff/hh07_fechamento.py
          $<TARGET_FILE:hh-hwscan> ${PROJECT_SOURCE_DIR})
set_tests_properties(hh07_fechamento PROPERTIES SKIP_RETURN_CODE 77 TIMEOUT 600)

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

# HH07.5: interfaces nativas compilam sozinhas e nao dependem do Dreamcast (com caso negativo).
add_test(NAME hh07_native_api COMMAND hh_native_api_check)
add_test(NAME hh07_native_limpo
  COMMAND ${CMAKE_COMMAND} -DROOT=${PROJECT_SOURCE_DIR} -P ${CMAKE_CURRENT_SOURCE_DIR}/hh07/check_native_clean.cmake)
add_test(NAME hh07_native_limpo_negativo
  COMMAND ${CMAKE_COMMAND} -DROOT=${CMAKE_CURRENT_SOURCE_DIR}/fixtures/hh07/native_sujo
          -P ${CMAKE_CURRENT_SOURCE_DIR}/hh07/check_native_clean.cmake)
set_tests_properties(hh07_native_limpo_negativo PROPERTIES PASS_REGULAR_EXPRESSION "native/ depende do Dreamcast")
# so o CMake liga native/ ao runtime (os cabecalhos estao limpos): tambem precisa falhar
add_test(NAME hh07_native_limpo_cmake_negativo
  COMMAND ${CMAKE_COMMAND} -DROOT=${CMAKE_CURRENT_SOURCE_DIR}/fixtures/hh07/native_cmake_sujo
          -P ${CMAKE_CURRENT_SOURCE_DIR}/hh07/check_native_clean.cmake)
set_tests_properties(hh07_native_limpo_cmake_negativo PROPERTIES
  PASS_REGULAR_EXPRESSION "native/CMakeLists.txt: 'hh_runtime'")
# e a ligacao efetiva do alvo, conferida no configure (pega a vinda de fora de native/)
get_target_property(_hh_native_links hh_native_api INTERFACE_LINK_LIBRARIES)
if(_hh_native_links)
  foreach(_l IN LISTS _hh_native_links)
    if(_l MATCHES "^hh_(runtime|legacy|transitional|dcmap|generated|translator)")
      message(FATAL_ERROR "hh_native_api liga ${_l}: native/ nao pode depender do Dreamcast")
    endif()
  endforeach()
endif()

# HH07.6: handles com geracao, ABI guest e registro de fronteiras (legado/nativo).
add_executable(hh07_transitional_tests unit/main.cpp unit/test_hh07_transitional.cpp)
target_include_directories(hh07_transitional_tests PRIVATE unit)
target_link_libraries(hh07_transitional_tests PRIVATE hh_transitional)
add_test(NAME hh07_transitional_tests COMMAND hh07_transitional_tests)

# HH08.2: sistema de arquivos nativo (data\disc + mods\assets).
add_executable(hh08_files_tests unit/main.cpp unit/test_hh08_files.cpp)
target_include_directories(hh08_files_tests PRIVATE unit)
target_link_libraries(hh08_files_tests PRIVATE hh_native_files)
add_test(NAME hh08_files_tests COMMAND hh08_files_tests)

# HH08.3: adaptador gdFs 1.09 (contratos, legado repassado, guardas, slots obrigatorios).
add_executable(hh08_gdfs_tests unit/main.cpp unit/test_hh08_gdfs.cpp)
target_include_directories(hh08_gdfs_tests PRIVATE unit)
target_link_libraries(hh08_gdfs_tests PRIVATE hh_transitional)
add_test(NAME hh08_gdfs_tests COMMAND hh08_gdfs_tests)

# HH07.8: a leitura de CDI fica fora do produto; conferido no mapa do linker (com caso negativo).
add_test(NAME hh07_produto_sem_cdi_negativo
  COMMAND ${CMAKE_COMMAND} -DMAP=${CMAKE_CURRENT_SOURCE_DIR}/fixtures/hh07/mapa_com_cdi.map
          -P ${CMAKE_CURRENT_SOURCE_DIR}/hh07/check_product_map.cmake)
set_tests_properties(hh07_produto_sem_cdi_negativo PROPERTIES PASS_REGULAR_EXPRESSION "o produto contem a leitura de CDI: CdiImage;cdi.obj")
if(TARGET HeadHunter AND MSVC)
  add_test(NAME hh07_produto_sem_cdi
    COMMAND ${CMAKE_COMMAND} -DMAP=$<TARGET_FILE_DIR:HeadHunter>/HeadHunter.map
            -P ${CMAKE_CURRENT_SOURCE_DIR}/hh07/check_product_map.cmake)
endif()

# HH07.7: ordem de eventos (VBlank, interrupcao, threads, longjmp) identica em duas execucoes do
# roteiro newgame_area1 (dados privados: HH_DATA_DIR; sem ele = SKIP).
if(HH_GENERATED_DIR)
  add_test(NAME hh07_ordem
    COMMAND ${Python3_EXECUTABLE} -I ${CMAKE_CURRENT_SOURCE_DIR}/diff/hh07_ordem.py
            $<TARGET_FILE:hh-run> ${PROJECT_SOURCE_DIR}/tools/dist/roteiros/newgame_area1.txt)
  set_tests_properties(hh07_ordem PROPERTIES SKIP_RETURN_CODE 77 TIMEOUT 1200)
endif()

# HH08.4: portoes P1/P2 do provedor nativo de arquivos (legado x nativo no roteiro, com a
# mutacao negativa; dados privados: HH_DATA_DIR; sem ele = SKIP).
if(HH_GENERATED_DIR)
  add_test(NAME hh08_arquivos
    COMMAND ${Python3_EXECUTABLE} -I ${CMAKE_CURRENT_SOURCE_DIR}/diff/hh08_arquivos.py
            $<TARGET_FILE:hh-run> ${PROJECT_SOURCE_DIR})
  set_tests_properties(hh08_arquivos PROPERTIES SKIP_RETURN_CODE 77 TIMEOUT 1800)
  # HH08.5/.6: discos juntos (sem troca de disco) e falhas seguras (HH_DATA_DIR + HH_DATA_DIR2)
  add_test(NAME hh08_discos
    COMMAND ${Python3_EXECUTABLE} -I ${CMAKE_CURRENT_SOURCE_DIR}/diff/hh08_discos.py
            $<TARGET_FILE:hh-run> $<TARGET_FILE:hh-extract> ${PROJECT_SOURCE_DIR})
  set_tests_properties(hh08_discos PROPERTIES SKIP_RETURN_CODE 77 TIMEOUT 1800)
endif()

# HH08 (revisao Codex): instalacao sem o marcador do Builder e recusada e fica intacta.
if(TARGET hh-builder)
  add_test(NAME hh08_builder_sem_marcador
    COMMAND ${CMAKE_COMMAND} -DBUILDER=$<TARGET_FILE:hh-builder> -DWORK=${CMAKE_CURRENT_BINARY_DIR}/hh08_builder_sem_marcador
            -P ${CMAKE_CURRENT_SOURCE_DIR}/hh08/check_builder_unmarked.cmake)
endif()
# HH18: troca da instalacao a partir do Builder antigo (marca .hh-builder e data\ nao passam como do usuario).
if(TARGET hh_builder_core)
  add_executable(hh18_builder_tests unit/main.cpp unit/test_hh18_builder.cpp)
  target_include_directories(hh18_builder_tests PRIVATE unit)
  target_link_libraries(hh18_builder_tests PRIVATE hh_builder_core)
  add_test(NAME hh18_builder_tests COMMAND hh18_builder_tests)
  # HH12: catalogo dos filmes, cabecalho 4X, MP4 sintetico pelo Media Foundation, reproducao e Builder sem FFmpeg.
  add_executable(hh12_video_tests unit/main.cpp unit/test_hh12_video.cpp)
  target_include_directories(hh12_video_tests PRIVATE unit)
  target_link_libraries(hh12_video_tests PRIVATE hh_builder_core hh_native_video)
  add_test(NAME hh12_video_tests COMMAND hh12_video_tests)
endif()

# HH09: tempo e eventos nativos (relogio, filas, provedor nativo x legado, monitor de progresso).
add_executable(hh09_tempo_tests unit/main.cpp unit/test_hh09_tempo.cpp)
target_include_directories(hh09_tempo_tests PRIVATE unit)
target_link_libraries(hh09_tempo_tests PRIVATE hh_transitional hh_translator)
add_test(NAME hh09_tempo_tests COMMAND hh09_tempo_tests)

# HH09: portoes do tempo nativo (A1/A3/A5 estavel x legado, negativo de mutacao, B7/B8 experimental).
if(HH_GENERATED_DIR)
  add_test(NAME hh09_tempo
    COMMAND ${Python3_EXECUTABLE} -I ${CMAKE_CURRENT_SOURCE_DIR}/diff/hh09_tempo.py
            $<TARGET_FILE:hh-run> ${PROJECT_SOURCE_DIR})
  set_tests_properties(hh09_tempo PROPERTIES SKIP_RETURN_CODE 77 TIMEOUT 2400)
endif()

# HH10.1: matriz do contrato do decodificador do pd 1.50 contra o codigo gerado (f_8C15D900).
if(HH_GENERATED_DIR AND TARGET hh_host)
  add_executable(hh10_pad_matrix unit/main.cpp unit/test_hh10_pad_matrix.cpp)
  target_include_directories(hh10_pad_matrix PRIVATE unit)
  target_link_libraries(hh10_pad_matrix PRIVATE hh_host)
  add_test(NAME hh10_pad_matrix COMMAND hh10_pad_matrix)
endif()

# HH10.2/10.3: nucleo nativo de entrada e adaptador do pd 1.50.
add_executable(hh10_input_tests unit/main.cpp unit/test_hh10_input.cpp unit/test_hh10_pad_adapter.cpp)
target_include_directories(hh10_input_tests PRIVATE unit)
target_link_libraries(hh10_input_tests PRIVATE hh_transitional)
add_test(NAME hh10_input_tests COMMAND hh10_input_tests)

# HH11.1: store nativo de saves (formato VMS, envelope, escrita atomica, recuperacao, falhas).
add_executable(hh11_saves_tests unit/main.cpp unit/test_hh11_saves.cpp unit/test_hh11_bu_machine.cpp)
target_include_directories(hh11_saves_tests PRIVATE unit)
target_link_libraries(hh11_saves_tests PRIVATE hh_transitional)
add_test(NAME hh11_saves_tests COMMAND hh11_saves_tests WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR})

# HH11: portoes dos saves nativos contra o oraculo do Flycast (HH_DATA_DIR, HH11_ORACULO).
if(HH_GENERATED_DIR)
  add_test(NAME hh11_saves
    COMMAND ${Python3_EXECUTABLE} -I ${CMAKE_CURRENT_SOURCE_DIR}/diff/hh11_saves.py
            $<TARGET_FILE:hh-run> ${PROJECT_SOURCE_DIR} ${HH_GENERATED_DIR})
  set_tests_properties(hh11_saves PROPERTIES SKIP_RETURN_CODE 77 TIMEOUT 3600)
endif()

# HH12: portoes dos filmes nativos (V0-V4; HH_DATA_DIR, FFmpeg no PATH ou HH_FFMPEG).
if(HH_GENERATED_DIR AND TARGET hh-builder)
  add_test(NAME hh12_video
    COMMAND ${Python3_EXECUTABLE} -I ${CMAKE_CURRENT_SOURCE_DIR}/diff/hh12_video.py
            $<TARGET_FILE:hh-run> $<TARGET_FILE:hh-builder> ${PROJECT_SOURCE_DIR} ${HH_GENERATED_DIR})
  set_tests_properties(hh12_video PROPERTIES SKIP_RETURN_CODE 77 TIMEOUT 3600)
endif()

# HH10: portoes dos controles nativos (C1/C2/C3/C8 e negativos; HH_DATA_DIR).
if(HH_GENERATED_DIR)
  add_test(NAME hh10_controles
    COMMAND ${Python3_EXECUTABLE} -I ${CMAKE_CURRENT_SOURCE_DIR}/diff/hh10_controles.py
            $<TARGET_FILE:hh-run> ${PROJECT_SOURCE_DIR} ${HH_GENERATED_DIR})
  set_tests_properties(hh10_controles PROPERTIES SKIP_RETURN_CODE 77 TIMEOUT 3000)
endif()
