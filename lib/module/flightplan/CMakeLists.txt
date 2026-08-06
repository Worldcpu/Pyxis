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
