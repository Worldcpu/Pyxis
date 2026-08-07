# lib/module/navdata/ — px 导航数据只读视图（决策 48）
# 读 bfdb（链接 bravofinder 库经公开 API），自建机场索引 + 备降两阶段过滤；
# 未来地图航路图数据基础。不动 bf 源码（fork 可干净更新）。

add_library(px_navdata STATIC
  airport_index.cc
)

target_include_directories(px_navdata PUBLIC
  ${CMAKE_SOURCE_DIR}/include
)

# px_core (INTERFACE) 传递 bravofinder 头/库；距离计算复用 bf::Coordinate。
target_link_libraries(px_navdata PUBLIC px_core)

# bf_adapter.h（bf→px 错误桥接，lib/core/）首次接线点——仅本模块实现使用。
target_include_directories(px_navdata PRIVATE ${CMAKE_SOURCE_DIR}/lib/core)
target_compile_features(px_navdata PUBLIC cxx_std_20)

# 项目契约禁用异常（与 px_flightplan 一致，避免 gcov 幻影边失真）。
target_compile_options(px_navdata PRIVATE
  $<$<NOT:$<COMPILE_LANG_AND_ID:CXX,MSVC>>:-fno-exceptions>)
