cmake_minimum_required(VERSION 3.26)
project(px VERSION 0.1.0 LANGUAGES CXX C)

# C++20
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# MSVC 需要 /Zc:preprocessor 才能识别 __VA_OPT__（C++20）。
# bf engine 的 logging 宏使用了 __VA_OPT__。
if(MSVC)
  add_compile_options(/Zc:preprocessor)
endif()

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Options
option(PX_BUILD_TESTS "Build tests" ON)
option(PX_BUILD_SERVER "Build WebSocket server" ON)

# FetchContent for third-party deps
include(FetchContent)

# --- RapidJSON ---
set(RAPIDJSON_BUILD_EXAMPLES OFF CACHE INTERNAL "")
set(RAPIDJSON_BUILD_TESTS OFF CACHE INTERNAL "")
set(RAPIDJSON_BUILD_DOC OFF CACHE INTERNAL "")
FetchContent_Declare(
  rapidjson
  GIT_REPOSITORY https://github.com/Tencent/rapidjson.git
  GIT_TAG        v1.1.0
  GIT_SHALLOW    TRUE
  GIT_SUBMODULES ""
)
FetchContent_MakeAvailable(rapidjson)

# bf 子模块的 libs/http_server 与 libs/service 引用 rapidjson::rapidjson
# 导入目标（命名空间形式）。rapidjson v1.1.0 不定义 CMake target——补
# 别名并直接携带 include 目录。
if(NOT TARGET rapidjson::rapidjson)
  add_library(rapidjson::rapidjson INTERFACE IMPORTED)
  target_include_directories(rapidjson::rapidjson INTERFACE
                             ${rapidjson_SOURCE_DIR}/include)
endif()

# --- Catch2 (tests) ---
if(PX_BUILD_TESTS)
  FetchContent_Declare(
    Catch2
    GIT_REPOSITORY https://github.com/catchorg/Catch2.git
    GIT_TAG        v3.7.1
  )
  FetchContent_MakeAvailable(Catch2)
endif()

# --- tl::expected (C++11 std::expected backport, header-only) ---
set(EXPECTED_BUILD_TESTS OFF CACHE INTERNAL "")
FetchContent_Declare(
  tl_expected
  GIT_REPOSITORY https://github.com/TartanLlama/expected.git
  GIT_TAG        v1.1.0
  GIT_SHALLOW    TRUE
)
FetchContent_MakeAvailable(tl_expected)

# --- libuv ---
FetchContent_Declare(
  libuv
  GIT_REPOSITORY https://github.com/libuv/libuv.git
  GIT_TAG        v1.49.2
)
FetchContent_MakeAvailable(libuv)

# --- llhttp ---
FetchContent_Declare(
  llhttp
  GIT_REPOSITORY https://github.com/nodejs/llhttp.git
  GIT_TAG        release/v9.2.1
)
FetchContent_MakeAvailable(llhttp)

# --- SQLite3 ---
FetchContent_Declare(
  sqlite3
  URL https://www.sqlite.org/2024/sqlite-amalgamation-3460000.zip
)
FetchContent_MakeAvailable(sqlite3)

# --- fmt (C++17 std::format replacement) ---
FetchContent_Declare(
  fmt
  GIT_REPOSITORY https://github.com/fmtlib/fmt.git
  GIT_TAG        11.1.4
  GIT_SHALLOW    TRUE
)
FetchContent_MakeAvailable(fmt)

# Subdirectories
add_subdirectory(lib)
add_subdirectory(service)

if(PX_BUILD_SERVER)
  add_subdirectory(src/server)
endif()

if(PX_BUILD_TESTS)
  enable_testing()
  add_subdirectory(tests/cpp)
endif()
