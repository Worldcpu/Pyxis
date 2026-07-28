# lib/core/ — px thin layer: Result, Error, bf_adapter

add_library(px_core INTERFACE)

target_include_directories(px_core INTERFACE
  ${CMAKE_SOURCE_DIR}/include)

target_link_libraries(px_core INTERFACE
  tl::expected
  bravofinder)

target_compile_features(px_core INTERFACE cxx_std_20)
