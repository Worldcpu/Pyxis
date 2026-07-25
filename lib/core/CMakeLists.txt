# lib/core/ — foundation: domain types, Result, CSR graph, A*, constraints

add_library(px_core STATIC
  core.cc
  coordinate.cc
  astar.cc
)

target_include_directories(px_core PUBLIC
  ${CMAKE_CURRENT_SOURCE_DIR}
  ${CMAKE_SOURCE_DIR}/include
)

target_link_libraries(px_core PUBLIC tl::expected)
target_compile_features(px_core PUBLIC cxx_std_17)
