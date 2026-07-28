# bf engine 直接集成设计

## 背景

原方案通过 FetchContent 从 Worldcpu/BravoFinder fork 拉取 engine 库链接。现改为直接将 bf engine（`libs/engine/`）源码复制到 Pyxis `lib/engine/` 下，放弃当前 Pyxis `lib/` 中自实现的 navdata/router 代码。

原因：用户是上游仓库贡献者，bf engine 改动极小（仅 BearingTo），直接集成方便开发、调试和命名空间隔离。

## 目录结构

```
lib/
├── engine/                    # bf engine 完整复制，代码零修改（除 BearingTo）
│   ├── CMakeLists.txt         # 编译为 px_engine 静态库
│   ├── core/
│   │   ├── domain/            # coordinate, ident, mora_grid, procedure
│   │   ├── graph/             # nav_graph, astar, yen_kshortest
│   │   ├── query/             # query_types
│   │   ├── util/              # small_vec, string_util
│   │   ├── result.h           # bf::Result<T>（variant 实现，不改）
│   │   └── env.h
│   └── io/
│       ├── loaders/           # loader 接口 + dfd1/dfd2/fenix/xplane12
│       ├── cache/             # bfdb 容器、codec、unified_cache
│       ├── graph_builder      # NavData IR → CSR 图
│       ├── nav_data.h         # NavData IR
│       ├── nav_database       # NavDatabase 门面
│       └── procedure_connector
├── core/                      # px 自持薄层
│   ├── result.h               # px::Result = tl::expected<T, px::Error>
│   ├── error.h                # px::ErrorCode（11 种，含 kCancelled 等）
│   └── bf_adapter.h           # FromBf() / ToBf() 转换函数
├── module/
│   └── flightplan/            # Phase 8 原创飞行计划引擎
└── CMakeLists.txt
```

**须删除的旧文件**：`lib/core/coordinate.cc`、`lib/core/altitude_constraints.h`、`lib/core/mora_constraint.h`、`lib/module/navdata/`（全部）、`lib/module/router/`（全部）。

**须删除的旧头文件**：`include/px/core/coordinate.h`、`include/px/core/graph_edge.h`、`include/px/core/nav_graph.h`、`include/px/core/ident.h`、`include/px/core/constraint.h`、`include/px/core/route_query.h`、`include/px/core/cancel_token.h`、`include/px/core/mora_grid.h`、`include/px/module/navdata/`（全部）、`include/px/module/router/`（全部）。

**保留的头文件**：`include/px/core/result.h`、`include/px/core/error.h`。

## 命名空间与 include

- bf engine 所有代码保持 `namespace bf`，内部 include 路径不变（如 `"core/domain/coordinate.h"`）
- px 层使用 `namespace px`
- CMake 将 `lib/engine/` 加入 PUBLIC include path，px 代码可直接 `#include "core/domain/coordinate.h"` 使用 bf 类型
- px 公开头文件放在 `include/px/`

## 异常处理

- bf engine 使用 `bf::Result<T>`（std::variant 实现），不做修改
- px 层使用 `px::Result<T>` = `tl::expected<T, px::Error>`
- 边界通过 `lib/core/bf_adapter.h` 中的 `px::FromBf()` 薄函数转换

## CMake 构建

```
lib/engine/   → px_engine 静态库（内部链接 sqlite3.c）
lib/core/     → px_core（result, error, bf_adapter）
lib/module/flightplan/ → px_flightplan
```

`px_core` PUBLIC link `px_engine`，上游消费者自动获得所有 bf 类型。

## BearingTo

唯一对 bf engine 源码的修改：在 `lib/engine/core/domain/coordinate.h/.cc` 中添加 `BearingTo()` 方法（~5 行）。

## 测试

- 删除旧的 px 测试：`test_coordinate.cc`、`test_graph_builder.cc`、`test_astar.cc`、`test_yen.cc`、`test_mora.cc`、`test_constraints.cc`、`test_ident.cc`、`test_bfdb_roundtrip.cc`、`bench_yen.cc`
- 保留 `test_result.cc`（7 个用例，测试 px 自持 Result）
- bf engine 自带测试保留在 `lib/engine/` 内，由 CI 统一运行
- 新增 `test_bf_integration.cc` 验证 FromBf 转换
