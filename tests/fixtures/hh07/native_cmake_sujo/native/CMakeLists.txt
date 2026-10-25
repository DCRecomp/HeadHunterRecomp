# Fixture HH07: native/ ligado ao runtime legado (caso negativo de hh07_native_limpo).
add_library(hh_native_api INTERFACE)
target_link_libraries(hh_native_api INTERFACE hh_runtime)
