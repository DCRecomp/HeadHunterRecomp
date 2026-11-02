# hh-extract: ferramenta de extracao de GD-ROM (CDI) - HH02.
add_library(hh_extract_core STATIC
  src/cdi.cpp
  src/extract.cpp
  src/hash.cpp
  src/iso9660.cpp
  src/ip_bin.cpp
  src/known_versions.cpp
  src/scramble.cpp
  src/runtime_data.cpp
  src/runtime_data_cdi.cpp
  src/runtime_data_merge.cpp
)
target_include_directories(hh_extract_core PUBLIC include)
target_compile_features(hh_extract_core PUBLIC cxx_std_20)

add_executable(hh-extract src/main.cpp)
target_link_libraries(hh-extract PRIVATE hh_extract_core)
hh_icone(hh-extract)

source_group(TREE ${CMAKE_CURRENT_SOURCE_DIR} PREFIX "hh-extract" FILES
  src/cdi.cpp src/extract.cpp src/hash.cpp src/iso9660.cpp src/ip_bin.cpp
  src/known_versions.cpp src/scramble.cpp src/main.cpp
  include/hh_extract/cdi.hpp include/hh_extract/extract.h
  include/hh_extract/hash.hpp include/hh_extract/iso9660.hpp
  include/hh_extract/ip_bin.hpp include/hh_extract/known_versions.hpp
  include/hh_extract/scramble.hpp)
