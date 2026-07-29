# service/ — app-layer: RapidJSON translation, query handlers, registry

add_library(px_service STATIC
  service.cc
  json_module.cc
  route_json_module.cc
  flightplan_json_module.cc
)

target_include_directories(px_service PUBLIC
  ${CMAKE_CURRENT_SOURCE_DIR}
  ${CMAKE_SOURCE_DIR}/include
  ${rapidjson_SOURCE_DIR}/include
)

target_link_libraries(px_service PUBLIC
  px_core
)

target_compile_features(px_service PUBLIC cxx_std_20)
