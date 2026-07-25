# tests/cpp/ — C++ test suite (Catch2)

add_executable(px_tests
  test_result.cc
  test_coordinate.cc
  test_ident.cc
  test_graph_builder.cc
  test_constraints.cc
  test_astar.cc
)

target_link_libraries(px_tests PRIVATE
  px_core
  px_service
  Catch2::Catch2WithMain
)

target_compile_features(px_tests PRIVATE cxx_std_17)

include(Catch)
catch_discover_tests(px_tests)
