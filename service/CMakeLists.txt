# service/ — app-layer: RapidJSON translation, query handlers, registry

add_library(px_service STATIC
  service.cc
  json_module.cc
  flightplan_json_module.cc
  plan_json.cc
  pln_export.cc
  rpc_dispatch.cc
  bf_rpc.cc
  http_rpc.cc
  plan_handlers.cc
  airframe_json.cc
  profile_json.cc
)

target_include_directories(px_service PUBLIC
  ${CMAKE_CURRENT_SOURCE_DIR}
  ${CMAKE_SOURCE_DIR}/include
  ${rapidjson_SOURCE_DIR}/include
)

# bf_adapter.h（bf→px 错误桥接）供 plan_handlers 错误映射使用。
target_include_directories(px_service PRIVATE ${CMAKE_SOURCE_DIR}/lib/core)

target_link_libraries(px_service PUBLIC
  px_core
  px_flightplan   # plan handler 编排域层（FromBf/配载/高度/校验）
  bf_service_lib  # 决策 15 薄桥接：bf handler 经 bf_rpc 透传，不再死代码
)

target_compile_features(px_service PUBLIC cxx_std_20)

# 同 px_flightplan：禁用异常（项目契约 + 消除 gcov 幻影异常边）。
# rapidjson 1.1.0 默认配置无异常（PARSE_ERROR 走 assert），无需额外宏。
# MSVC 排除：cl 无此选项（D9002 未知选项警告），且 /WX 下会升级为错误。
target_compile_options(px_service PRIVATE
  $<$<NOT:$<COMPILE_LANG_AND_ID:CXX,MSVC>>:-fno-exceptions>)
