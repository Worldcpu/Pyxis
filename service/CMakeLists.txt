# service/ — app-layer: RapidJSON translation, query handlers, registry

add_library(px_service STATIC
  service.cc
  json_module.cc
  flightplan_json_module.cc
  plan_json.cc
  pln_export.cc
  rpc_dispatch.cc
  bf_rpc.cc
)

target_include_directories(px_service PUBLIC
  ${CMAKE_CURRENT_SOURCE_DIR}
  ${CMAKE_SOURCE_DIR}/include
  ${rapidjson_SOURCE_DIR}/include
)

target_link_libraries(px_service PUBLIC
  px_core
  bf_service_lib  # 决策 15 薄桥接：bf handler 经 bf_rpc 透传，不再死代码
)

target_compile_features(px_service PUBLIC cxx_std_20)
