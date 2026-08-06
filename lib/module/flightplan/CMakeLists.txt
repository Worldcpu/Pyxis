# lib/module/flightplan/ — flight planning engine
# 域类型头在 include/px/module/flightplan/（公共面），实现在此。

add_library(px_flightplan STATIC
  airframe.cc
  altitude_planner.cc
  cache_policy.cc
  from_bf.cc
  payload.cc
)

target_include_directories(px_flightplan PUBLIC
  ${CMAKE_SOURCE_DIR}/include
)

# px_core (INTERFACE) 传递 bravofinder 头/库；域实现经 bf_adapter 桥接。
target_link_libraries(px_flightplan PUBLIC px_core)
target_compile_features(px_flightplan PUBLIC cxx_std_20)

# 项目契约禁用异常（px::Result 错误体系）。除语义约束外，-fno-exceptions 消除
# gcov 给每个调用行配的幻影异常抛出边（永远 0 命中），避免 Codecov 行覆盖口径失真。
target_compile_options(px_flightplan PRIVATE -fno-exceptions)
