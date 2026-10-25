# Builder (HH06, inicial): CLI que converte o CDI do usuario no Preview HeadHunter.exe.
# Usa as bibliotecas do extrator e do tradutor diretamente; o CMake roda como processo externo.
add_executable(hh-builder builder_main.cpp)
target_link_libraries(hh-builder PRIVATE hh_recomp_lib hh_extract_core)
target_compile_definitions(hh-builder PRIVATE HH_KIT_DIR="${PROJECT_SOURCE_DIR}")
