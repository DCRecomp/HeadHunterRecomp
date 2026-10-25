# Interoperabilidade da fase hibrida (docs/planos/HH07-PLANO.md, HH07.6): handles de 32 bits
# com geracao, adaptadores Sh4Context <-> native/, registro de fronteiras com provedor
# legado/nativo. Some quando os chamadores forem elevados (HH15).
add_library(hh_transitional STATIC boundary_registry.cpp gdfs_adapter.cpp time_adapter.cpp)
target_include_directories(hh_transitional PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(hh_transitional PUBLIC hh_legacy hh_native_api hh_native_files hh_native_time hh_dcmap)
