# CLAUDE.md

> This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## 项目概述

Pyxis 是一个面向模拟飞行的航空工具箱，以 Bravofinder 为航路研究底层，支持导入 PMDG / Fenix 导航数据、生成合规航路、制作飞行计划和查阅航图。

## 技术栈

- C++20、CMake 3.26+、Google C++ Style（clang-format）
- 第三方（通过 FetchContent 获取，不提交源码）：RapidJSON v1.1.0、Catch2 v3.7.1、tl::expected v1.1.0、libuv v1.49.2、llhttp v9.2.1、SQLite3 amalgamation 3.46.0、fmt 11.1.4
- 前端（规划）：React 19、TypeScript、Vite、Tailwind CSS、Tauri
- 包管理：CMake + FetchContent（C++），pnpm（前端）

## 语言

- 对话、注释、文档和提交信息默认使用**简体中文**；文档优先使用中文。
- 对于我向你提出的任何一个项目 phase 任务，工作流使用自定义的工作流，详情如下：
  0. 先同步 submodule 的仓库保持最新。
  1. 从 `/grill-me` 开始 — 永远不要跳过需求分析阶段
  2. 使用 `writing-plans` 转化设计 — 设计必须转化为可执行计划
  3. 用 `using-git-worktrees` 隔离环境 — 实现前必须创建干净的工作空间
  4. `subagent-driven-development` 是主力 — 推荐使用两阶段审查确保质量
  5. `verification-before-completion` 是纪律 — 证据在断言之前，永不跳过
  6. 代码审查requesting-code-review不可省略 — 早审查、常审查
- 例外（保留原文）：代码标识符、现有代码风格、技术专有名词/命令/API。
- **代码注释使用中文**，对于注释的编写优先对于一整个函数，注释，对于函数内部的注释除非及其难以理解否则不用注释。
- **Markdown 绝不硬换行（铁律）。** 每个段落——无论是在 `README.md`、`docs/*.md` 还是 GitHub release notes 中——都是**一个逻辑行**；让渲染器自动软换行。不要在段落内部手动折行到 80 列。硬换行的段落在同步到 GitHub releases 时会渲染为破碎/异常的换行，并且会让 diff 变得嘈杂。这与 git 提交信息正文的规则一致（参见 Git）。代码块、表格和 ASCII/框图是预格式化的——保留其原始换行。
- 对于实现方式的梗概，见 `doc/` 文件夹。对于文件编写禁止用 `cat` 来写入。

## 目录结构与架构

```text
.
├── bravofinder/            # [已移入 lib/ 子模块] 历史参考，由 .gitignore 排除
├── lib/bravofinder/        # BravoFinder v3 引擎 (git submodule, heads/v3, 命名空间 bf)
│   └── libs/engine/        #   静态库 bravofinder：图算法、I/O、cache、SQLite
├── build/                  # 构建产物 (Git 忽略)
├── doc/BravoFinder/        # 设计文档集——作为 Pyxis 和 bravofinder 的共同架构蓝图
├── include/px/             # [C++] 公开头文件
│   ├── core/               #   px::Result、px::Error、px::ErrorCode
│   └── module/
│       └── flightplan/     #   高度规划器
├── third_party/            # [C++] 外部第三方源码 (通过 FetchContent 获取)
├── lib/                    # [C++] Pyxis 核心引擎 (严禁 JSON/网络/UI 依赖)
│   ├── core/               #   px_core (INTERFACE 库)：bf_adapter.h (bf→px 错误桥接)
│   └── module/             #   领域模块 (navdata/router 已由 bravofinder 子模块替代)
├── service/                # [C++] px_service (STATIC 库)：RapidJSON 翻译、查询 handler
├── src/
│   ├── server/             # [C++] px_server 可执行文件 (libuv + llhttp WebSocket)
│   ├── web/                # [React] 前端项目 (独立 pnpm workspace，待实现)
│   └── desktop/            # [Tauri/Rust] 桌面端外壳 (仅窗口控制/子进程管理)
├── tests/cpp/              # [C++] Catch2 测试套件 + Google Benchmark
├── tools/                  # 工具脚本（navdata 探查、MORA 提取、bf 子模块同步）
└── navdata/                # 本地导航数据文件 (Git 忽略——版权数据严禁提交)
```

### 分层纪律

1. `lib/` 必须保持**纯 C++20，严禁引入 JSON、网络和 UI 依赖**。只依赖标准库和通过 FetchContent 获取的 `third_party/` 库。
2. `service/` 是 app 层库，使用 RapidJSON 将领域结果翻译为 JSON，与 `lib/` 引擎分开编译。
3. `src/web/` 是独立的 React 项目，通过 WebSocket (`ws://127.0.0.1:port`) 与 C++ 后端进程通信。
4. Tauri (Rust) **仅负责**窗口控制、系统托盘、自动更新检查以及启动/关停 C++ 后端子进程。**严禁在 Rust 端编写任何业务逻辑或算法**。所有桌面原生交互统一使用 `@tauri-apps/api` 在 React 前端调用。
5. `tests` 文件夹是测试项目文件夹，其内部如果在调用外部文件，一定不能暴露本地开发环境。以运行时传参方式进行 test。

### 关键架构关系

**BravoFinder 子模块集成：** `lib/bravofinder` 是 git submodule（跟踪 `heads/v3`），提供 `bravofinder` 静态库（命名空间 `bf`）。`lib/core/bf_adapter.h` 是薄桥接层，提供 `bf::Result<T>` → `px::Result<T>` 的单向转换以及 `bf::ErrorCode` → `px::ErrorCode` 的映射。**px 层不直接暴露 bf 类型**——所有跨命名空间边界都通过 `FromBf()` / `FromBfError()` 转换。`bf_adapter.h` 内含编译期哨兵（`static_assert`）检测 bf 枚举变更，若 bf 新增错误码导致断言失败，需同步更新 `FromBfErrorCode()` 映射表。

**构建依赖链：** `bravofinder` (STATIC, 含 sqlite3.c) → `px_core` (INTERFACE, 仅头文件 + bf_adapter) → `px_service` (STATIC, RapidJSON) → `px_server` (EXE, libuv + llhttp)。`tests/cpp/px_tests` 直接链接 `px_core` + `Catch2::Catch2WithMain`。

**错误处理体系：** `px::Result<T>` 是 `tl::expected<T, Error>` 的别名。`px::Error` 包含 `ErrorCode` 枚举（11 个值：`kNotFound` 到 `kInternalError`）+ 人类可读 `message`。工厂函数 `Ok(value)` / `Err<T>(error)` 构造结果值。bf 层的错误码通过 `FromBfErrorCode()` 映射到 px 层。严禁使用异常进行控制流。

**设计文档集：** `doc/BravoFinder/` 包含 15 篇架构与算法文档（域设计、合规航路、Yen-Lawler 优化、地形安全、线程安全等），是 Pyxis 和 bravofinder 的共同设计蓝图。`doc/superpowers/` 存放 phase 任务的 specs 和 plans。

## 编码规范

### C++

- 现代 C++20；**Google C++ Style**（格式化基础，clang-format `BasedOnStyle: Google`）
  + **C++ Core Guidelines**（语义正确性：RAII、`enum class`、span/view 优先于裸指针）。
- 文件名 `snake_case`，扩展名 **`.h` / `.cc`**；头文件保护用 `#pragma once`；命名空间 `px`。
- 类型 `PascalCase`，变量 `snake_case`，常量 `kPascalCase`，成员变量尾下划线 `member_`。
- 错误处理使用 `px::Result<T, E>`（封装 `tl::expected`），对于 Bravofinder 的库内容使用 `FromBf` 函数进行转化。
- JSON 输出使用 **RapidJSON `Writer`**（SAX 流式输出，自动转义）；不使用手写字符串，不使用 nlohmann。
- 当 `.h`,`.cc`出现更改必须使用 clang-format 进行格式化。
- 第三方依赖通过 CMake `FetchContent` 获取，**不提交源码进仓库**。
- **禁止裸 `new`/`delete`，禁止 `goto`，禁止按值捕获异常，禁止 `static`/全局可变状态**。
- 默认本地构建时使用 DEBUG 模式开启所有警告，调试时使用 fsanitize：`address`、`thread`、`undefined`，当进行性能测试时必须使用 RELEASE 方式来编译。

### React

- 使用函数组件 + Hooks，禁止 Class 组件。
- Props 类型定义在组件上方，使用 `interface` 而非 `type`。
- 组件内部顺序：Props 定义 → 组件函数 → 导出。

## Git

- 提交使用英文 **Conventional Commits**（`feat`、`fix`、`docs`、`refactor`、`test`、`build`、`perf`、`style`、`chore`）；scope 取自模块分层（`core`、`navdata`、`router`、`constraints` 等）。
- 正文段落为单行不折行（与所有 markdown 一样遵循不硬换行规则——参见语言）；保留 Claude 共同作者签名（`Co-Authored-By: Claude <noreply@anthropic.com>`）。
- 对于复杂里程碑，**先对齐方案，再实施**；每个主要任务结束时，做一次文档 / memory 交接。
- `lib/bravofinder` 是 git submodule，更新后需提交 submodule 指针变更。使用 `tools/sync-bf-engine.sh` 同步子模块。

## 构建与命令

### C++ 后端

首次配置需下载第三方依赖：

```bash
# 配置（默认 Debug，开启测试）
cmake -S . -B build -DPX_BUILD_TESTS=ON

# 构建
cmake --build build -j $(nproc)

# 运行全部测试
ctest --test-dir build --output-on-failure
```

**按名称运行单个测试（以下为示例）：**

```bash
# ctest 正则匹配
ctest --test-dir build -R "Yen: k=1" --output-on-failure

# 或直接运行测试二进制（支持 Catch2 标签过滤）
./build/tests/cpp/px_tests "A* 线形链 A→D"
./build/tests/cpp/px_tests "[benchmark]"    # 仅运行含 [benchmark] 标签的测试
```

**构建变体：**

```bash
# Release
cmake -S . -B build/release -DCMAKE_BUILD_TYPE=Release -DPX_BUILD_TESTS=ON
cmake --build build/release

# TSAN (Thread Sanitizer)
cmake -S . -B build/tsan -DCMAKE_BUILD_TYPE=RelWithDebInfo \
  -DCMAKE_CXX_FLAGS="-O1 -g -fsanitize=thread -fno-omit-frame-pointer" \
  -DCMAKE_EXE_LINKER_FLAGS="-fsanitize=thread"
cmake --build build/tsan
```

本地调试时使用 sanitizer：`address`、`thread`、`undefined`。性能测试必须使用 Release 模式。

**代码格式化：**

```bash
clang-format -i path/to/file.h                           # 单个文件
find include lib service src tests -name '*.h' -o -name '*.cc' | xargs clang-format -i   # 全部文件
```

`.clang-format` 配置：Google 风格基础，2 空格缩进，左指针对齐，Include 排序。*

### React 前端

```bash
cd src/web
pnpm install
pnpm dev              # 开发模式 → localhost:5173
pnpm build            # 生产构建
pnpm test             # 前端测试 (Vitest)
```

### Tauri 桌面端

非交互式 shell 需先加载 Rust 环境：

```bash
export PATH="$HOME/.cargo/bin:$PATH"
cd src/desktop/src-tauri
cargo check           # 类型检查
cargo build           # 编译
```

Tauri v2 Linux 系统依赖（首次需一次性安装）：

```bash
sudo apt install -y pkg-config libgtk-3-dev libwebkit2gtk-4.1-dev librsvg2-dev patchelf
```