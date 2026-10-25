# SPDX-License-Identifier: MIT
# Teste negativo da guarda do produto: este projeto DEVE falhar ao configurar.
cmake_minimum_required(VERSION 3.20)
project(hh_guard_negative LANGUAGES CXX)
list(APPEND CMAKE_MODULE_PATH ${CMAKE_CURRENT_SOURCE_DIR}/../../cmake)
include(HHProductGuard)
file(WRITE ${CMAKE_CURRENT_BINARY_DIR}/a.cpp "int a() { return 0; }\n")
file(WRITE ${CMAKE_CURRENT_BINARY_DIR}/m.cpp "int main() { return 0; }\n")
add_library(tool_lib STATIC ${CMAKE_CURRENT_BINARY_DIR}/a.cpp)
hh_mark_tool_only(tool_lib)
add_library(mid_lib INTERFACE)
target_link_libraries(mid_lib INTERFACE tool_lib)
add_executable(product ${CMAKE_CURRENT_BINARY_DIR}/m.cpp)
target_link_libraries(product PRIVATE mid_lib)
if(HH_GUARD_LATE)
  # Variante negativa da ordem: o produto e verificado ANTES de a dependencia transitiva existir
  # (late_lib so e definida depois da chamada). A guarda precisa enxergar mesmo assim.
  add_executable(product_late ${CMAKE_CURRENT_BINARY_DIR}/m.cpp)
  target_link_libraries(product_late PRIVATE late_lib)
  hh_assert_product_clean(product_late)
  add_library(late_lib INTERFACE)
  target_link_libraries(late_lib INTERFACE tool_lib)
elseif(HH_GUARD_EXPECT_CLEAN)
  # Variante positiva: produto sem a biblioteca marcada.
  add_executable(product_clean ${CMAKE_CURRENT_BINARY_DIR}/m.cpp)
  hh_assert_product_clean(product_clean)
else()
  hh_assert_product_clean(product)
endif()
