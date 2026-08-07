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
# 警告全开 + 视警告为错误（GCC/Clang 与 MSVC 对齐）——本地构建行为 =
# CI 门禁，杜绝本地绿远程红的漏网（如 unused-function/unused-parameter）。
target_compile_options(px_core INTERFACE
  $<$<COMPILE_LANG_AND_ID:CXX,MSVC>:/W3 /WX>
  $<$<COMPILE_LANG_AND_ID:CXX,GNU,Clang,AppleClang>:-Wall -Wextra -Werror>)

# Windows：uv.h 引入 windows.h 的 min/max 宏，破坏 rapidjson/bf 头中的
# numeric_limits::max()（fixed_string.h:61/136、document.h:958 C2589）。
# 回归自 WS 线 17d1cad 修复（force push 覆盖后需随 HTTP 线重带）——
# 放 INTERFACE 使全部 px 目标（px_server/px_tests/...）统一继承。
target_compile_definitions(px_core INTERFACE
  $<$<PLATFORM_ID:Windows>:NOMINMAX>)
