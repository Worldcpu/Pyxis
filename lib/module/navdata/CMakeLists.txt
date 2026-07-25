# lib/module/navdata/ — navigation data parsers (SQLite→IR) + graph builder
add_library(px_navdata STATIC
  nav_data_parser.cc
  graph_builder.cc
)

target_include_directories(px_navdata PUBLIC
  ${CMAKE_CURRENT_SOURCE_DIR}
  ${CMAKE_SOURCE_DIR}/include
)

target_link_libraries(px_navdata PUBLIC px_core sqlite3)
target_compile_features(px_navdata PUBLIC cxx_std_17)
