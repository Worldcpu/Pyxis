# tests/cpp/ — C++ test suite (Catch2)

add_executable(px_tests
  main.cc  # 接管 Catch2 main：unitbuf 即时冲刷（CI 管道下挂死可定位）
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
  test_profile_json.cc
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
  Catch2::Catch2
)

target_compile_features(px_tests PRIVATE cxx_std_20)

include(Catch)
if(WIN32)
  # Windows 上 catch_discover_tests 的中文测试名经 ctest 传参时编码损坏
  # （chcp 65001 无效），改用 ASCII 标签分区注册：既保留逐分区粒度，
  # 又绕开中文参数传递（诊断证实：llhttp.dll 路径补齐后两分区全绿）。
  # [server] 为唯一 libuv/socket 集成测试，超时收紧到 30s；其余 300s。
  add_test(NAME px_tests_non_server COMMAND px_tests "~[server]")
  add_test(NAME px_tests_server COMMAND px_tests "[server]")
  set_tests_properties(px_tests_server PROPERTIES TIMEOUT 30)
  set_tests_properties(px_tests_non_server PROPERTIES TIMEOUT 300)
else()
  catch_discover_tests(px_tests)
endif()

# Google Benchmark — disabled pending re-enable in Phase 8
# if(CMAKE_VERSION VERSION_GREATER_EQUAL 3.20)
#   find_package(benchmark QUIET ...)
# endif()
