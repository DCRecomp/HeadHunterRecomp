# Runtime hibrido temporario (runtime/: CPU SH-4 recompilada, memoria, PowerVR, AICA, Maple,
# GD-ROM e BIOS HLE). No HH07 os arquivos ficam onde estao; este alvo so nomeia o conjunto
# legado para que o produto e os testes declarem a dependencia. Cada subsistema sai daqui
# quando migrar (HH08-HH14); o alvo desaparece no HH15.
add_library(hh_legacy INTERFACE)
target_link_libraries(hh_legacy INTERFACE hh_runtime)
