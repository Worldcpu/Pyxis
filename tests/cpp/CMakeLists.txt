# tests/cpp/ — C++ test suite (Catch2)

add_executable(px_tests
  test_airframe.cc
  test_airframe_json.cc
  test_airport_index.cc
  test_bf_rpc.cc
  test_cache_policy.cc
  test_flightplan_domain.cc
  test_http_rpc.cc
  test_json_module.cc
  test_plan_json.cc
  test_plan_handlers.cc
  test_pln_export.cc
  test_result.cc
  test_rpc_dispatch.cc
  test_server_smoke.cc
)

target_link_libraries(px_tests PRIVATE
  px_core
  px_flightplan
  px_navdata
  px_service
  bf_http_server  # 集成冒烟：真实 HTTP 传输（S4b，决策 50）
  Catch2::Catch2WithMain
)

target_compile_features(px_tests PRIVATE cxx_std_20)

include(Catch)
if(WIN32)
  # Windows 上 catch_discover_tests 的中文测试名经 ctest 传参时编码损坏
  # （chcp 65001 无效），直接运行测试二进制绕开名字传递；
  # 代价是 Windows 失去逐用例粒度，换取编码无关的确定性。
  add_test(NAME px_tests COMMAND px_tests)
else()
  catch_discover_tests(px_tests)
endif()

# Google Benchmark — disabled pending re-enable in Phase 8
# if(CMAKE_VERSION VERSION_GREATER_EQUAL 3.20)
#   find_package(benchmark QUIET ...)
# endif()
