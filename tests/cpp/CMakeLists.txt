# tests/cpp/ — C++ test suite (Catch2)

add_executable(px_tests
  test_airframe.cc
  test_bf_rpc.cc
  test_cache_policy.cc
  test_flightplan_domain.cc
  test_json_module.cc
  test_plan_json.cc
  test_pln_export.cc
  test_result.cc
  test_rpc_dispatch.cc
)

target_link_libraries(px_tests PRIVATE
  px_core
  px_flightplan
  px_service
  Catch2::Catch2WithMain
)

target_compile_features(px_tests PRIVATE cxx_std_20)

include(Catch)
catch_discover_tests(px_tests)

# Google Benchmark — disabled pending re-enable in Phase 8
# if(CMAKE_VERSION VERSION_GREATER_EQUAL 3.20)
#   find_package(benchmark QUIET ...)
# endif()
