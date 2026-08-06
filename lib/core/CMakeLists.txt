# lib/core/ — px thin layer: Result, Error, bf_adapter

add_library(px_core INTERFACE)

target_include_directories(px_core INTERFACE
  ${CMAKE_SOURCE_DIR}/include)

target_link_libraries(px_core INTERFACE
  tl::expected
  bravofinder)

target_compile_features(px_core INTERFACE cxx_std_20)

# MSVC 警告全开 + 视警告为错误，只作用于 px 自有目标（经 INTERFACE 传播到
# px_flightplan/px_service/px_server/px_tests）。不能走全局 CMAKE_CXX_FLAGS：
# 那样会 (a) 覆盖 CMake 默认的 /EHsc /GR 引发 C4530，(b) 误伤 FetchContent 依赖
# （fmt/Catch2 自带 /W4）与 bf 子模块，把第三方代码的警告也升级为错误。
target_compile_options(px_core INTERFACE
  $<$<COMPILE_LANG_AND_ID:CXX,MSVC>:/W3 /WX>)
