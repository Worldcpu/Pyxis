# tests/cpp/ — C++ test suite (Catch2)

add_executable(px_tests
  test_result.cc
  test_coordinate.cc
  test_ident.cc
  test_graph_builder.cc
  test_constraints.cc
  test_astar.cc
  test_mora.cc
  test_yen.cc
)

target_link_libraries(px_tests PRIVATE
  px_core
  px_navdata
  px_router
  px_service
  Catch2::Catch2WithMain
)

target_compile_features(px_tests PRIVATE cxx_std_17)

include(Catch)
catch_discover_tests(px_tests)

# Google Benchmark（系统安装于 ~/.local）
if(CMAKE_VERSION VERSION_GREATER_EQUAL 3.20)
  find_package(benchmark QUIET
    PATHS $ENV{HOME}/.local
    NO_DEFAULT_PATH)
endif()

if(benchmark_FOUND)
  add_executable(px_bench bench_yen.cc)
  target_link_libraries(px_bench PRIVATE px_core px_navdata px_router benchmark::benchmark)
  target_compile_features(px_bench PRIVATE cxx_std_17)
  message(STATUS "Google Benchmark found — px_bench target enabled")
else()
  message(STATUS "Google Benchmark not found — px_bench target disabled")
endif()
