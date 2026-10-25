# Ferramentas por etapa do ROADMAP.
add_subdirectory(hh-extract)

# Mapa de regioes do Dreamcast (portado do Blue Stinger): sonda de hardware e metricas.
add_library(hh_dcmap STATIC dcmap/dc_regions.cpp)
target_include_directories(hh_dcmap PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/dcmap)
