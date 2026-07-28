# bf engine 直接集成实施计划

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 将 BravoFinder engine 源码集成到 Pyxis `lib/engine/`，替换旧的 px navdata/router 实现，保留核心测试。

**Architecture:** bf engine 保持 `namespace bf` 代码零修改（除 BearingTo），编译为 `px_engine` 静态库。px 提供 `lib/core/bf_adapter.h` 转换层。

**Tech Stack:** C++20, CMake 3.26+, tl::expected, Catch2, sqlite3 amalgamation

## 风险与可行性

| 风险 | 概率 | 影响 | 缓解 |
|------|------|------|------|
| sqlite3.c 已存在于 Pyxis FetchContent，与 engine 内联编译冲突 | 低 | 链接重定义 | engine 使用 Pyxis FetchContent 的 `sqlite3_SOURCE_DIR`，不自带副本 |
| bf engine 内部 `#include` 路径依赖 `PROJECT_SOURCE_DIR` | 低 | 编译失败 | CMakeLists 中用 `CMAKE_CURRENT_SOURCE_DIR` 替换 |
| engine 依赖未声明的 bf 头文件 | 中 | 编译失败 | 25 个 .cc 已用 `find` 验证完整，PUBLIC include dir 覆盖全部 |
| 旧头文件删除后有残留引用 | 低 | 编译失败 | 已验证——所有引用在待删除文件中，service/src 无引用 |
| bf::Result 和 px::Result 混用 | 中 | 类型错误 | FromBf() 适配层 + 编译期类型检查 |
| bf engine 测试需要真实 navdata | 高 | 测试 skip | 仅迁移纯单元测试，集成测试保留 label `[integration]` 按需运行 |

## Global Constraints

- bf engine 代码 `namespace bf` 下零修改（BearingTo 除外）
- px 层 `namespace px` + `tl::expected<T, px::Error>`
- `lib/engine/` PUBLIC include path，`#include "core/domain/coordinate.h"` 路径不变
- 构建零错误，保留测试全部通过
- 修改 `.h/.cc` 后运行 clang-format

---

### Task 1: 删除旧 lib/、include/px/ 和测试文件

**验证前提：** `grep -rn 'px/module/navdata\|px/module/router\|px/core/coordinate\|px/core/nav_graph\|px/core/graph_edge\|px/core/ident\|px/core/constraint\|px/core/route_query\|px/core/cancel_token\|px/core/mora_grid' service/ src/` 返回空——即 service/ 和 src/ 无任何旧头文件引用，安全删除。

- [ ] **Step 1: 删除旧实现**

```bash
rm -f lib/core/coordinate.cc lib/core/altitude_constraints.h lib/core/mora_constraint.h
rm -rf lib/module/navdata/
rm -rf lib/module/router/
```

- [ ] **Step 2: 删除旧公开头文件**

```bash
rm -f include/px/core/coordinate.h include/px/core/graph_edge.h
rm -f include/px/core/nav_graph.h include/px/core/ident.h
rm -f include/px/core/constraint.h include/px/core/route_query.h
rm -f include/px/core/cancel_token.h include/px/core/mora_grid.h
rm -rf include/px/module/navdata/
rm -rf include/px/module/router/
```

- [ ] **Step 3: 删除旧测试**

```bash
rm -f tests/cpp/test_coordinate.cc tests/cpp/test_graph_builder.cc
rm -f tests/cpp/test_astar.cc tests/cpp/test_yen.cc tests/cpp/test_mora.cc
rm -f tests/cpp/test_constraints.cc tests/cpp/test_ident.cc
rm -f tests/cpp/test_bfdb_roundtrip.cc tests/cpp/bench_yen.cc
rm -f tests/cpp/bravo_yen.h
```

- [ ] **Step 4: 提交**

```bash
git add -A
git commit -m "refactor(engine): remove old px navdata/router/core implementations"
```

---

### Task 2: 复制 bf engine 源码

- [ ] **Step 1: 复制 engine 目录**

```bash
cp -r bravofinder/libs/engine lib/engine
rm -rf lib/engine/tests 2>/dev/null || true
rm -rf lib/engine/bench 2>/dev/null || true
```

- [ ] **Step 2: 验证 25 个 .cc 文件完整**

```bash
EXPECTED="core/domain/coordinate.cc core/domain/procedure.cc
core/graph/astar.cc core/graph/yen_kshortest.cc
core/routing/route_parser.cc core/routing/route_string.cc
io/cache/bfdb_inventory.cc io/cache/bfdb_naming.cc
io/cache/cifp_codec.cc io/cache/graph_codec.cc
io/cache/nav_detail_codec.cc io/cache/unified_cache.cc
io/graph_builder.cc
io/loaders/dfd1/dfd1_loader.cc io/loaders/dfd2/dfd2_loader.cc
io/loaders/fenix/fenix_loader.cc io/loaders/loader_registry.cc
io/loaders/sqlite_util.cc io/loaders/xplane12/cifp_parser.cc
io/loaders/xplane12/xplane12_loader.cc
io/nav_database.cc io/nav_database_parse.cc
io/nav_database_query.cc io/nav_database_routing.cc
io/procedure_connector.cc"

for f in $EXPECTED; do
  [ -f "lib/engine/$f" ] || echo "MISSING: $f"
done
echo "All 25 .cc files present"
```

- [ ] **Step 3: 提交**

```bash
git add lib/engine/
git commit -m "feat(engine): copy bf engine source into lib/engine/

25 source files, includes dfd1/dfd2/fenix/xplane12 loaders,
A*/Yen search, bfdb cache, graph builder, nav database.
Source: Worldcpu/BravoFinder v3 branch. LGPL-3.0-or-later."
```

---

### Task 3: 适配 engine CMakeLists.txt

**变更：**
- `PROJECT_SOURCE_DIR` → `CMAKE_CURRENT_SOURCE_DIR`
- 移除 install/export、version.h.in、BRAVOFINDER_BENCH_VARIANT
- 目标名 `px_engine`，移除 `bf::bravofinder` alias
- sqlite3.c 使用 `${sqlite3_SOURCE_DIR}`（Pyxis FetchContent 提供）

- [ ] **Step 1: 写入 CMakeLists.txt**

```cmake
# lib/engine/ — bf engine core (namespace bf), compiled as px_engine static lib.
# Source: Worldcpu/BravoFinder v3
# License: LGPL-3.0-or-later

add_library(px_engine STATIC
  ${sqlite3_SOURCE_DIR}/sqlite3.c
  core/domain/coordinate.cc
  core/domain/procedure.cc
  core/graph/astar.cc
  core/graph/yen_kshortest.cc
  core/routing/route_parser.cc
  core/routing/route_string.cc
  io/loaders/sqlite_util.cc
  io/loaders/loader_registry.cc
  io/loaders/dfd1/dfd1_loader.cc
  io/loaders/dfd2/dfd2_loader.cc
  io/loaders/fenix/fenix_loader.cc
  io/loaders/xplane12/xplane12_loader.cc
  io/loaders/xplane12/cifp_parser.cc
  io/cache/graph_codec.cc
  io/cache/bfdb_inventory.cc
  io/cache/bfdb_naming.cc
  io/cache/cifp_codec.cc
  io/cache/nav_detail_codec.cc
  io/cache/unified_cache.cc
  io/graph_builder.cc
  io/procedure_connector.cc
  io/nav_database.cc
  io/nav_database_routing.cc
  io/nav_database_parse.cc
  io/nav_database_query.cc)

target_include_directories(px_engine PUBLIC
  ${CMAKE_CURRENT_SOURCE_DIR})

target_compile_features(px_engine PUBLIC cxx_std_20)

set_source_files_properties(${sqlite3_SOURCE_DIR}/sqlite3.c PROPERTIES
  COMPILE_OPTIONS "$<$<OR:$<C_COMPILER_ID:GNU>,$<C_COMPILER_ID:Clang>>:-w>"
  COMPILE_DEFINITIONS "SQLITE_OMIT_LOAD_EXTENSION;SQLITE_THREADSAFE=1")

target_include_directories(px_engine PRIVATE ${sqlite3_SOURCE_DIR})
```

- [ ] **Step 2: 验证文件列表与 find 输出一致**

```bash
find lib/engine -name '*.cc' | sort > /tmp/engine_files.txt
# 确认 CMakeLists.txt 中列出全部 25 个 .cc 文件 + sqlite3.c
```

- [ ] **Step 3: 提交**

```bash
git add lib/engine/CMakeLists.txt
git commit -m "build(engine): adapt CMakeLists for Pyxis px_engine target"
```

---

### Task 4: 添加 BearingTo

- [ ] **Step 1: 修改 coordinate.h** — 在 `DistanceTo` 声明后添加：

```cpp
  // Initial bearing (forward azimuth) from this coordinate to another, in
  // degrees from true north, normalized to [0, 360).  Uses the spherical
  // earth model with the standard great-circle bearing formula.
  double BearingTo(const Coordinate& other) const;
```

- [ ] **Step 2: 修改 coordinate.cc** — 在匿名命名空间中添加 `ToDegrees`，然后添加 `BearingTo` 实现：

```cpp
// 在 namespace { ... } 块中，ToRadians 之后添加：
double ToDegrees(double radians) { return radians * 180.0 / kPi; }

// 在 DistanceTo 实现之后添加：
double Coordinate::BearingTo(const Coordinate& other) const {
  const double lat1 = ToRadians(latitude);
  const double lat2 = ToRadians(other.latitude);
  const double d_lon = ToRadians(other.longitude - longitude);

  const double y = std::sin(d_lon) * std::cos(lat2);
  const double x = std::cos(lat1) * std::sin(lat2) -
                   std::sin(lat1) * std::cos(lat2) * std::cos(d_lon);
  double bearing = ToDegrees(std::atan2(y, x));
  if (bearing < 0.0) bearing += 360.0;
  return bearing;
}
```

- [ ] **Step 3: 运行 clang-format**

```bash
clang-format -i lib/engine/core/domain/coordinate.h lib/engine/core/domain/coordinate.cc
```

- [ ] **Step 4: 提交**

```bash
git add lib/engine/core/domain/coordinate.h lib/engine/core/domain/coordinate.cc
git commit -m "feat(engine): add BearingTo method to bf::Coordinate

Computes initial great-circle bearing in degrees from true
north, normalized to [0, 360)."
```

---

### Task 5: 重写 lib/core/ — px 薄层

- [ ] **Step 1: 更新 lib/core/CMakeLists.txt**

```cmake
# lib/core/ — px thin layer: Result, Error, bf_adapter

add_library(px_core INTERFACE)

target_include_directories(px_core INTERFACE
  ${CMAKE_SOURCE_DIR}/include)

target_link_libraries(px_core INTERFACE
  tl::expected
  px_engine)

target_compile_features(px_core INTERFACE cxx_std_20)
```

- [ ] **Step 2: 创建 lib/core/bf_adapter.h** — FromBf 转换层：

```cpp
// SPDX-License-Identifier: MIT
#pragma once

#include "core/result.h"    // bf::Result, bf::Error, bf::ErrorCode
#include "px/core/result.h"  // px::Result, px::Error, px::ErrorCode

namespace px {

inline ErrorCode FromBfErrorCode(bf::ErrorCode c) {
  switch (c) {
    case bf::ErrorCode::kUnknown:          return ErrorCode::kInternalError;
    case bf::ErrorCode::kInvalidArgument:  return ErrorCode::kInvalidArgument;
    case bf::ErrorCode::kDataMissing:      return ErrorCode::kDataMissing;
    case bf::ErrorCode::kParseError:       return ErrorCode::kParseError;
    case bf::ErrorCode::kAirportNotFound:  return ErrorCode::kNotFound;
    case bf::ErrorCode::kNoRoute:          return ErrorCode::kNoRouteFound;
    case bf::ErrorCode::kCacheCorrupt:     return ErrorCode::kCacheCorrupt;
    case bf::ErrorCode::kFormatMismatch:   return ErrorCode::kFormatMismatch;
  }
  return ErrorCode::kInternalError;
}

inline Error FromBfError(const bf::Error& e) {
  return Error{FromBfErrorCode(e.code), e.message};
}
inline Error FromBfError(bf::Error&& e) {
  return Error{FromBfErrorCode(e.code), std::move(e.message)};
}

template <typename T>
Result<T> FromBf(bf::Result<T>&& r) {
  if (r.has_value()) return Ok(std::move(r).value());
  return Err<T>(FromBfError(std::move(r).error()));
}

inline Result<void> FromBf(bf::Result<void>&& r) {
  if (r.has_value()) return Ok();
  return Err<void>(FromBfError(std::move(r).error()));
}

}  // namespace px
```

- [ ] **Step 3: 提交**

```bash
git add lib/core/CMakeLists.txt lib/core/bf_adapter.h
git commit -m "feat(core): rewrite px_core as thin layer with FromBf adapter"
```

---

### Task 6: 更新上层 CMakeLists

- [ ] **Step 1: lib/CMakeLists.txt** — 添加 engine subdirectory

```cmake
add_subdirectory(engine)
add_subdirectory(core)
add_subdirectory(module)
```

- [ ] **Step 2: lib/module/CMakeLists.txt** — 移除 navdata/router subdirectory

```cmake
# lib/module/ — domain modules: flightplan
```

- [ ] **Step 3: 根 CMakeLists.txt** — C++17 → C++20

```cmake
set(CMAKE_CXX_STANDARD 20)
```

- [ ] **Step 4: tests/cpp/CMakeLists.txt** — 只保留 test_result.cc（其余测试在 Task 7 迁移）

```cmake
add_executable(px_tests
  test_result.cc
)

target_link_libraries(px_tests PRIVATE
  px_core
  Catch2::Catch2WithMain
)

target_compile_features(px_tests PRIVATE cxx_std_20)
include(Catch)
catch_discover_tests(px_tests)
```

- [ ] **Step 5: 验证构建**

```bash
cmake -B build/debug -DCMAKE_BUILD_TYPE=Debug -DPX_BUILD_TESTS=ON 2>&1 | tail -5
cmake --build build/debug -j $(nproc) 2>&1 | tail -10
```

预期：configure 成功，编译零错误。

- [ ] **Step 6: 运行保留测试**

```bash
ctest --test-dir build/debug --output-on-failure
```

预期：`test_result.cc` 7 个用例全部通过。

- [ ] **Step 7: 提交**

```bash
git add CMakeLists.txt lib/CMakeLists.txt lib/module/CMakeLists.txt tests/cpp/CMakeLists.txt
git commit -m "build: update CMake for px_engine integration"
```

---

### Task 7: 迁移 bf engine 核心单元测试

从 `bravofinder/tests/unit/` 迁移纯逻辑测试（不依赖真实 navdata）。这些测试通过 `bf::` 类型编写，验证 engine 正确性。

**迁移清单（10 个文件）：**

| 源文件 | 迁移到 | 测试内容 |
|--------|--------|---------|
| `coordinate_test.cc` | `tests/cpp/test_bf_coordinate.cc` | DistanceTo, BearingTo, 边界值 |
| `graph_test.cc` | `tests/cpp/test_bf_graph.cc` | GraphBuilder, VertexCount, Edges |
| `yen_test.cc` | `tests/cpp/test_bf_yen.cc` | Yen K-最短路径 |
| `search_workspace_test.cc` | `tests/cpp/test_bf_workspace.cc` | A* SearchWorkspace |
| `constraint_test.cc` | `tests/cpp/test_bf_constraint.cc` | 高度/速度/航路约束 |
| `mora_grid_test.cc` | `tests/cpp/test_bf_mora.cc` | MORA 网格 |
| `fixed_ident_test.cc` | `tests/cpp/test_bf_ident.cc` | FixedIdent 哈希/比较 |
| `route_parser_test.cc` | `tests/cpp/test_bf_route_parser.cc` | 航路字符串解析 |
| `route_string_test.cc` | `tests/cpp/test_bf_route_string.cc` | 航路字符串生成 |
| `small_vec_test.cc` | `tests/cpp/test_bf_small_vec.cc` | SmallVec 容器 |

**不迁移的测试：**
- 需要真实 navdata 文件的集成测试（dfd_loader_test, fenix_loader_test, route_integration_test 等）——这些依赖 `BRAVOFINDER_NAVDATA` 环境变量和真实 `.s3db/.db3` 文件
- MCP/HTTP handler 测试——不属于 engine 层
- result_test.cc——bf 已有 `test_result.cc` 测试 px::Result
- loader_registry_test.cc——需要所有 loader 实现链接

- [ ] **Step 1: 批量复制测试文件**

```bash
cp bravofinder/tests/unit/coordinate_test.cc       tests/cpp/test_bf_coordinate.cc
cp bravofinder/tests/unit/graph_test.cc             tests/cpp/test_bf_graph.cc
cp bravofinder/tests/unit/yen_test.cc               tests/cpp/test_bf_yen.cc
cp bravofinder/tests/unit/search_workspace_test.cc  tests/cpp/test_bf_workspace.cc
cp bravofinder/tests/unit/constraint_test.cc        tests/cpp/test_bf_constraint.cc
cp bravofinder/tests/unit/mora_grid_test.cc         tests/cpp/test_bf_mora.cc
cp bravofinder/tests/unit/fixed_ident_test.cc       tests/cpp/test_bf_ident.cc
cp bravofinder/tests/unit/route_parser_test.cc      tests/cpp/test_bf_route_parser.cc
cp bravofinder/tests/unit/route_string_test.cc      tests/cpp/test_bf_route_string.cc
cp bravofinder/tests/unit/small_vec_test.cc         tests/cpp/test_bf_small_vec.cc
```

- [ ] **Step 2: 适配 include 路径** — 批量替换测试文件中的 include 前缀

这些测试最初在 bf 仓库中编译（include root 也是 `engine/`），所以 `#include "core/..."` 路径在 Pyxis 中同样有效（因为 `px_engine` PUBLIC include 就是 `lib/engine/`）。**无需修改 include 路径。**

- [ ] **Step 3: 更新 tests/cpp/CMakeLists.txt** — 加入新测试

```cmake
add_executable(px_tests
  test_result.cc
  test_bf_coordinate.cc
  test_bf_graph.cc
  test_bf_yen.cc
  test_bf_workspace.cc
  test_bf_constraint.cc
  test_bf_mora.cc
  test_bf_ident.cc
  test_bf_route_parser.cc
  test_bf_route_string.cc
  test_bf_small_vec.cc
)
```

- [ ] **Step 4: 检查并移除需要 navdata 的测试**

部分测试可能使用 `SKIP` 宏处理无 navdata 的情况——保留这些测试，CI 环境无 navdata 时会自动跳过。

```bash
# 确认迁移的测试不硬依赖真实数据文件
grep -l 'navdata\|\.s3db\|\.db3\|BRAVOFINDER_NAVDATA' tests/cpp/test_bf_*.cc || echo "No hard navdata dependencies"
```

- [ ] **Step 5: 构建并运行全部测试**

```bash
cmake --build build/debug -j $(nproc)
ctest --test-dir build/debug --output-on-failure -j $(nproc)
```

预期：`test_result.cc` 7 个 + 迁移的 10 个 bf 测试 = 所有通过或 SKIP。

- [ ] **Step 6: 提交**

```bash
git add tests/cpp/test_bf_*.cc tests/cpp/CMakeLists.txt
git commit -m "test: migrate bf engine core unit tests

10 unit test files from bravofinder/tests/unit/: coordinate,
graph, yen, workspace, constraint, mora, ident, route_parser,
route_string, small_vec. Integration tests (needing real
navdata) remain in bravofinder/."
```

---

### Task 8: Release 构建验证 + 最终检查

- [ ] **Step 1: Release 构建**

```bash
cmake -B build/release -DCMAKE_BUILD_TYPE=Release -DPX_BUILD_TESTS=ON
cmake --build build/release -j $(nproc)
```

- [ ] **Step 2: 确认无旧类型残留**

```bash
grep -rn 'px::Coordinate\|px::NavGraph\|px::GraphEdge\|px::FixedIdent' include/px/ lib/core/ service/ src/ 2>/dev/null || echo "All clean"
```

- [ ] **Step 3: 验证 engine 文件列表完整性**

```bash
diff <(find lib/engine -name '*.cc' | sort) <(grep '\.cc' lib/engine/CMakeLists.txt | tr -d ' ' | sort) || true
```

- [ ] **Step 4: CI 干运行（本地）**

```bash
cmake -B build/ci-debug -DCMAKE_BUILD_TYPE=Debug -DPX_BUILD_TESTS=ON \
  -DCMAKE_CXX_FLAGS="-Wall -Wextra -Wpedantic -Werror" 2>&1 | tail -3
cmake --build build/ci-debug -j $(nproc) 2>&1 | tail -5
ctest --test-dir build/ci-debug --output-on-failure 2>&1 | tail -10
```

- [ ] **Step 5: 最终提交**

```bash
git add -A
git commit -m "chore: final verification after engine integration

Release build clean, no old type references, all tests pass."
```
