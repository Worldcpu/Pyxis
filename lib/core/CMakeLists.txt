# lib/core/ — domain foundation: Coordinate, GraphEdge, Ident, RouteQuery,
#              NavGraph (CSR), MoraGrid, Constraint interface, Result, Error,
#              and header-only Constraint implementations.

add_library(px_core STATIC
  core.cc
  coordinate.cc
)

target_include_directories(px_core PUBLIC
  ${CMAKE_CURRENT_SOURCE_DIR}
  ${CMAKE_SOURCE_DIR}/include
)

target_link_libraries(px_core PUBLIC tl::expected)
target_compile_features(px_core PUBLIC cxx_std_17)
