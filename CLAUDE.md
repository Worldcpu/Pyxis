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
- 对于我向你提出的任何一个项目 phase 任务，工作流使用 Superpower skills 工作流，详情如下：
  1. 从 brainstorming 开始 — 永远不要跳过需求分析阶段
  2. 使用 writing-plans 转化设计 — 设计必须转化为可执行计划
  3. 用 using-git-worktrees 隔离环境 — 实现前必须创建干净的工作空间
  4. subagent-driven-development 是主力 — 推荐使用两阶段审查确保质量
  5. verification-before-completion 是纪律 — 证据在断言之前，永不跳过
  6. 代码审查requesting-code-review不可省略 — 早审查、常审查
  7. dispatching-parallel-agents 提升效率 — 独立任务并行处理
- 例外（保留原文）：代码标识符、现有代码风格、技术专有名词/命令/API。
- **代码注释使用中文**，对于注释的编写优先对于一整个函数，注释，对于函数内部的注释除非及其难以理解否则不用注释。
- **Markdown 绝不硬换行（铁律）。** 每个段落——无论是在 `README.md`、`docs/*.md` 还是 GitHub release notes 中——都是**一个逻辑行**；让渲染器自动软换行。不要在段落内部手动折行到 80 列。硬换行的段落在同步到 GitHub releases 时会渲染为破碎/异常的换行，并且会让 diff 变得嘈杂。这与 git 提交信息正文的规则一致（参见 Git）。代码块、表格和 ASCII/框图是预格式化的——保留其原始换行。
- 对于实现方式的梗概，见 `doc/` 文件夹。

## 目录结构与架构

```text
.
├── bravofinder/            # BravoFinder v3 航路寻路引擎参考实现（C++20，命名空间 bf）
│   ├── apps/cli/           #   CLI 工具（bf 命令）
│   ├── apps/http/          #   REST JSON 服务端
│   ├── apps/mcp/           #   MCP 协议服务端（stdio / HTTP 双传输）
│   ├── lib/                #   核心引擎（无 JSON/网络依赖）
│   ├── service/            #   服务适配层（RapidJSON、多周期注册表）
│   ├── http_server/        #   手写 HTTP/1.1 传输层（libuv+llhttp）
│   ├── tests/              #   测试套件（Catch2，含 tsan preset）
│   └── docs/               #   公开文档（算法文章、架构说明）
├── build/                  # 构建产物 (Git 忽略)
├── doc/BravoFinder/        # 设计文档集——作为 Pyxis 和 bravofinder 的共同架构蓝图
├── include/px/             # [C++] 公开头文件
│   ├── core/               #   领域类型：Coordinate、Ident、Result、NavGraph、Constraint 等
│   ├── module/
│   │   ├── navdata/        #   导航数据 IR（RawWaypoint、RawSegment）
│   │   └── router/         #   寻路算法
│   │       ├── astar/      #     A* 单路径搜索
│   │       └── yen/        #     Yen K-最短路径
│   └── service/            #   服务适配层头文件（待实现）
├── third_party/            # [C++] 外部第三方源码 (通过 FetchContent 获取，不提交进仓库)
├── lib/                    # [C++] 核心引擎实现 (严禁 JSON/网络/UI 依赖)
│   ├── core/               #   领域基础：坐标计算、高度约束、MORA 约束
│   └── module/
│       ├── navdata/        #   SQLite→IR 解析器 + GraphBuilder（IR→CSR 图）
│       └── router/         #   寻路算法实现
│           ├── astar/      #     A* 搜索 + SearchWorkspace
│           └── yen/        #     Yen K-最短路径（Lawler 优化）
├── service/                # [C++] 服务适配层（RapidJSON 翻译、查询 handler，骨架）
├── src/
│   ├── server/             # [C++] WebSocket 服务端可执行文件（骨架）
│   ├── web/                # [React] 前端项目 (独立 pnpm workspace，待实现)
│   └── desktop/            # [Tauri/Rust] 桌面端外壳 (仅窗口控制/子进程管理，不含业务逻辑)
├── tests/cpp/              # [C++] Catch2 测试套件（111 个用例）+ Google Benchmark 性能基准
├── tools/                  # 工具脚本（navdata 探查、MORA 提取）
└── navdata/                # 本地导航数据文件 (Git 忽略——版权数据不得提交)
```

### 分层纪律

1. `lib/` 必须保持**纯 C++20，严禁引入 JSON、网络和 UI 依赖**。只依赖标准库和通过 FetchContent 获取的 `third_party/` 库。
2. `service/` 是 app 层库，使用 RapidJSON 将领域结果翻译为 JSON，与 `lib/` 引擎分开编译。
3. `src/web/` 是独立的 React 项目，通过 WebSocket (`ws://127.0.0.1:port`) 与 C++ 后端进程通信。
4. Tauri (Rust) **仅负责**窗口控制、系统托盘、自动更新检查以及启动/关停 C++ 后端子进程。**严禁在 Rust 端编写任何业务逻辑或算法**。所有桌面原生交互统一使用 `@tauri-apps/api` 在 React 前端调用。
5. 一些测试用指令可在 `.note/command.md` 下查阅。

## 编码规范

### C++

- 现代 C++20；**Google C++ Style**（格式化基础，clang-format `BasedOnStyle: Google`）
  + **C++ Core Guidelines**（语义正确性：RAII、`enum class`、span/view 优先于裸指针）。
- 文件名 `snake_case`，扩展名 **`.h` / `.cc`**；头文件保护用 `#pragma once`；命名空间 `px`。
- 类型 `PascalCase`，变量 `snake_case`，常量 `kPascalCase`，成员变量尾下划线 `member_`。
- 错误处理使用 `px::Result<T, E>`（封装 `tl::expected`），对于 Bravofinder 的库内容使用 `tobf` 函数进行转化。
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


