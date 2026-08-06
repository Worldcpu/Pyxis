# Pyxis × Simple Flight Planner 差距分析与后续开发蓝图

> 本文是竞品分析的第二部分：Pyxis 现行架构快照、与 [simple-flight-planner.md](simple-flight-planner.md)（Simple Flight Planner，下称 SFP）的逐功能对照、差距结论与分阶段开发路线。面向后续 phase 开发，作为需求分析（/grill-me）与计划撰写（writing-plans）的输入。编写日期：2026-08-04。

## 1. Pyxis 现行架构快照（2026-08-04）

### 1.1 分层与构建依赖链

```text
lib/bravofinder (STATIC, 子模块 heads/v3, 命名空间 bf)
  └─ 图算法 (A* + Yen 多路径 + 约束框架)、程序建模 (SID/STAR)、
     I/O (dfd1/dfd2/fenix/xplane12 loaders)、binary cache、SQLite
        ↓
px_core (INTERFACE, 仅头文件)
  └─ px::Result/Error/ErrorCode、bf_adapter.h (FromBf 桥接 + static_assert 哨兵)
        ↓
px_service (STATIC, RapidJSON)
  └─ JsonModuleRegistry 组合框架: route / flightplan 模块已实现,
     fuel 模块在计划中; PTF JSON 解析器计划中
        ↓
px_server (EXE, libuv + llhttp)   ← 当前仅 stub (main 返回 0)
        ↓
src/web (React 19 + TS + Vite + Tailwind)   ← 待实现
src/desktop (Tauri/Rust 外壳)              ← 待实现
```

### 1.2 已具备的引擎能力（bf v3 + px 层）

| 能力域 | 现状 | 落点 |
|--------|------|------|
| 合规航路搜索 | **完整**：方向性建图、高度带约束、MORA 地形安全、可插拔约束框架（硬过滤 + 软代价） | `lib/core/constraints/`、`lib/io/graph_builder.cc` |
| 多路径 | 完整：Yen-Lawler k 最短路 + 多因子评分 | `lib/core/graph/yen_kshortest`、doc/yen-lawler-optimization.zh-CN.md |
| 程序建模 | 完整：SID/STAR/进近，FAA CIFP 级完整性 | `lib/core/domain/procedure`、doc/procedure-modeling.zh-CN.md |
| 航路串 | 完整：ICAO Doc 4444 折叠压缩、并线航路累积交集 | `lib/core/routing/route_string.{h,cc}` |
| 数据源 | **强**：AIRAC 级（dfd1/dfd2/Fenix/PMDG/X-Plane 12 loaders） | `lib/io/loaders/` |
| 缓存 | binary cache + SQLite | `lib/io/cache/` |
| 错误体系 | px::Result（tl::expected），11 个 ErrorCode，无异常控制流 | `include/px/core/` |
| 高度规划 | **骨架**：altitude_planner.h 仅有设计注释（双模式 auto/manual、对流层顶、风温巡航层推荐） | `include/px/module/flightplan/altitude_planner.h` |
| 燃油计算 | **计划就绪未实现**：FuelEngine（OpenAP v2.2 PTF 查表、ISA 大气、WindProfile、三阶段积分、fuel_factor） | doc/superpowers/plans/2026-07-31-fuel-engine.md |
| JSON 输出 | 框架完整：JsonModule 注册表、ctx 跳过语义、同名覆盖 | `include/px/service/json_module.h` |

### 1.3 尚未实现的层

| 层 | 状态 | 说明 |
|----|------|------|
| px_server | stub | WebSocket 服务（libuv + llhttp）入口存在，无任何 handler 逻辑 |
| src/web | 骨架 | 依赖已装（pnpm workspace），React 源码待实现 |
| src/desktop | 待实现 | Tauri 外壳（窗口/托盘/子进程管理） |
| WindSource | 计划中 | Open-Meteo HTTP + TTL 缓存 + 离线降级（FuelEngine 计划 Task 10+） |
| 前端 ↔ 后端协议 | 未定 | JsonContext 已定义服务端 JSON 形状，消息协议未定 |

### 1.4 设计宪法（开发任何新功能都必须遵守）

1. `lib/` 纯 C++20，严禁 JSON/网络/UI 依赖；只依赖标准库 + FetchContent 第三方。
2. 无 `static`/全局可变状态、无裸 new/delete、无 goto；预期失败走 `Result`。
3. 领域类型是不可变值类型，复制即安全；并发只读天然安全。
4. 跨命名空间边界一律 `FromBf()`/`FromBfError()` 转换，px 不暴露 bf 类型。
5. `service/` 负责 RapidJSON 翻译；JSON 输出用 Writer（SAX 流式），不手写字符串。
6. Tauri (Rust) 仅做窗口/托盘/子进程，严禁业务逻辑。
7. 测试不暴露本地开发环境，外部文件以运行时传参方式传入。

## 2. 与 SFP 的逐功能对照

> SFP 功能清单详见 [simple-flight-planner.md](simple-flight-planner.md) 第 2 节。对照结论分四档：✅ 已有 / 🟡 部分（有计划或骨架）/ ❌ 缺失 / ➕ 超越。

### 2.1 航路规划核心

| SFP 功能 | Pyxis 对照 | 说明 |
|----------|-----------|------|
| 交互式地图规划 | ❌ 缺失 | 前端未实现；需要地图瓦片方案 |
| 八种规划方式 | ❌ 缺失 | Create Your Own / 预置观光航线 / Waypoints First / Shape Route / Surprise Me! / Plan by Time 等；Pyxis 无规划交互层 |
| SID/STAR/进近 | ➕ 超越 | Pyxis 全 AIRAC 程序建模；SFP 为三层（美国 CIFP / 20 国际精选 / APB 算法生成，后者明示"不保证障碍物净空、仅供模拟器"） |
| 航路搜索（V/J） | ➕ 超越 | bf 完整高空/低空航路网络 + 约束搜索；SFP 为"吸附到航路"（snap-to-airway） |
| 多航段行程 | 🟡 部分 | FlightPlan 域（Phase 8）规划中，多航段聚合未定义 |
| 航路串/飞行计划文本 | ➕ 超越 | ICAO Doc 4444 折叠压缩已实现 |
| 航线优化器 | ❌ 缺失 | SFP 用 nearest-neighbor + 2-opt 重排航路点；Pyxis 未规划（区别于 Yen-Lawler 多路径生成） |
| 智能建议引擎 | ❌ 缺失 | SFP 按风/高度/航路匹配/IFR 规则/燃油最多 3 条建议；Pyxis 未规划 |
| IFR 高度规则检查 | 🟡 部分 | altitude_planner（Phase 8）规划含巡航层推荐；半球规则（东单西双）未定义 |

### 2.2 性能与燃油

| SFP 功能 | Pyxis 对照 | 说明 |
|----------|-----------|------|
| 机型性能数据库（1000+ 机型） | 🟡 部分 | FuelEngine 计划支持 OpenAP 表；覆盖面取决于生成器 |
| 燃油计算器 | 🟡 部分 | 三阶段积分器计划就绪；SFP 为查表 + 简化模型 |
| 密度高度/TAS 换算 | 🟡 部分 | ISA + CAS/TAS 换算已计划（Task 2）；密度高度 API 未定义 |
| 起降滑跑距离 | ❌ 缺失 | FuelEngine 计划未覆盖地面段（TO/landing run） |
| 风估计 | 🟡 部分 | SFP 由地面 METAR 推导每段高空风；Pyxis WindProfile + Open-Meteo 计划中（数值风场优于推导，但离线降级语义需定） |
| 成本指数优化 | ❌ 缺失 | SFP 支持 0–999；未规划 |
| ETOPS/等时点 | ❌ 缺失 | SFP 地图 ETOPS 圆；未规划 |
| 按时间规划航路 | ❌ 缺失 | SFP Plan by Time；未规划 |
| 航司燃油政策分解 | 🟡 部分 | FuelEngine 的 fuel_factor 是乘性修正；SFP 的逐项分解（5% 应急/30 分钟储备/备降/滑行）未建模 |
| 签派报告 | ❌ 缺失 | SFP Flight Release（航线概览/逐项燃油/METAR 简报/安全清单）；未规划 |

### 2.3 安全与航路检查

| SFP 功能 | Pyxis 对照 | 说明 |
|----------|-----------|------|
| 地形剖面 | 🟡 部分 | MORA 网格已实现（terrain-safety.zh-CN.md）；剖面可视化在前端 |
| 空域警告 | ❌ 缺失 | SFP 用 OpenAIP（~27,000 多边形，CTR/TMA/Class A–G/禁限区）+ 美国 Class B 精选 + 5 NM 缓冲 + critical/warning/advisory 分级；空域数据不在 bf 当前数据源 |
| 日出日落 | ❌ 缺失 | SFP 用 NOAA 太阳算法；未规划 |
| 备降规划器 | ❌ 缺失 | SFP 150 NM 内按机型跑道长度过滤；未规划 |
| CFIT 风险分析 | 🟡 部分 | MORA 约束已实现；SFP 的"建议最低安全高度"前端呈现未规划 |

### 2.4 数据与生态

| SFP 功能 | Pyxis 对照 | 说明 |
|----------|-----------|------|
| AIRAC 数据 | ➕ 超越 | Pyxis 支持 PMDG/Fenix/dfd/X-Plane 多源导入；SFP 为内置云端数据集（OurAirports + AIRAC 导航台两套体系） |
| 程序数据 | ➕ 超越 | Pyxis 全 AIRAC 程序；SFP 三层（CIFP/国际精选/APB 算法） |
| SimBrief 导入 | ❌ 缺失 | 未规划 |
| 分享链接 | ❌ 缺失 | 依赖后端服务（SFP 为云端永久链接） |
| 导出 .PLN/.FMS | ❌ 缺失 | 未规划（航路串已具备，格式导出未做；SFP 有 99 点智能简化与拆分导出） |
| VATSIM/IVAO 在线网络 | ❌ 缺失 | 未规划 |
| Simple Flight Tracker 遥测 | ❌ 缺失 | 未规划（SFP 为 SimConnect → 本地 WebSocket ws://localhost:29112 → 浏览器） |
| NOTAM | ❌ 缺失 | SFP 经 AutoRouter 合作方实时获取；未规划 |
| 天气与航图 | ❌ 缺失 | SFP 有 METAR/TAF、伪 ATIS、SIGMET/AIRMET 叠加、FAA d-TPP 24,000+ 航图内嵌 PDF（28 天刷新）；Pyxis 未规划 |
| 重量与平衡 CG 包线 | ❌ 缺失 | 未规划 |
| 飞行员日志 | ❌ 缺失 | 未规划 |
| PWA 离线 | ❌ 缺失 | 前端未实现 |

### 2.5 其他

| SFP 功能 | Pyxis 对照 | 说明 |
|----------|-----------|------|
| 检查点时刻表 | 🟡 部分 | FuelEngine 时间/距离输出可支撑；UI 未定 |
| 打印 | ❌ 缺失 | 未规划（SFP 支持计划与地图打印） |

## 3. 差距结论

### 3.1 SFP 的可借鉴点（应仿照）

1. **功能闭环顺序**：SFP 的演进是先让"简单规划可用"（地图 + 导出 .PLN），再叠加性能/燃油/风/安全功能。Pyxis 恰好相反——引擎层（合规航路、程序建模）远超 SFP，但"最简单的可用闭环"（服务器 + 前端 + 导出 + 飞行计划）尚未打通。
2. **数据分层策略**：SFP 是"云端数据库（机场/导航台按需拉取）+ 浏览器 LocalStorage 缓存 + PWA 离线可用 + 少量在线增强（METAR 公共 API、VATSIM/IVAO、NOTAM、OpenAIP 空域）"，数据链路全部免费公开（OurAirports、OpenAIP、FAA CIFP/d-TPP、Open-Meteo）。Pyxis 已选 Open-Meteo + 离线降级的同类路线（FuelEngine 计划），应扩展到气象与空域整体（空域可评估 OpenAIP 等免费源）。
3. **燃油/性能模型可对标**：SFP 燃油模型是闭源黑盒（公开信息仅参数规则，推断为经验公式）；Pyxis FuelEngine 计划基于 OpenAP 查表 + 三阶段积分（对标 4.29% QAR 偏差），精度有差异化空间，UI 暴露方式（密度高度、TAS、滑跑距离、航司燃油政策分解）可对标。
4. **分享与导出是传播杠杆**：SFP 的永久分享链接与 .PLN 导出使其成为社区默认工具之一。Pyxis 应尽早定义导出格式。
5. **IFR 程序三层策略**：真实 CIFP / 精选国际 / APB 算法生成（明示"仅供模拟器"）——用免费数据覆盖全部机场。Pyxis 全 AIRAC 程序更强，但 APB 式降级策略可启发"数据缺失机场"的处理方案。

### 3.2 Pyxis 的差异化优势（应进一步）

1. **合规航路引擎**是 SFP 不具备的（SFP 不做约束搜索，只做点线拼接 + 吸附式航路）。这是"进一步"的第一落点。
2. **多数据源 AIRAC**（PMDG/Fenix 数据导入）是 SFP 内置数据集不具备的灵活性。
3. **模块化 JSON 框架**使功能可组合、可测试，比 SFP 单体前端更易扩展。
4. **多路径 + 多因子评分**（Yen-Lawler）可支撑"候选航路对比"——SFP 有航路对比功能但无自动生成。
5. **程序数据真实性与精度**：Pyxis 全 AIRAC 程序 vs SFP 三层策略中算法生成的"不保证净空"程序。

### 3.3 风险提示

- bf 数据源不含空域（Class B/TMA/限制区）与气象（风温廓线、METAR），这两类能力需要新数据源与 lib 边界决策（空域可评估 OpenAIP；气象按 FuelEngine 计划的 Open-Meteo 路线）。
- px_server 与前端是最大空白；协议设计（JsonContext 之外的请求/响应模型）需要尽早定稿，否则 fuel/flightplan 模块的 JSON 形状会被推翻。
- SFP 是 PWA 纯前端 + 轻后端；Pyxis 是 C++ 后端 + React 前端 + Tauri 桌面，形态不同——"仿照"应聚焦功能与交互，而非架构形态。
- SFP 的免费模式靠捐赠支撑服务器成本（地图瓦片、天气、NOTAM、机场库）；Pyxis 本地优先（navdata 本地文件）在成本上更稳健，但在线增强（METAR/空域/分享）同样需要服务成本决策。

## 4. 后续开发蓝图（草案，供 /grill-me 细化）

> 各 phase 遵循 CLAUDE.md 工作流：需求分析（/grill-me）→ 设计转计划（writing-plans）→ 隔离环境（using-git-worktrees）→ 两阶段审查 → 验证纪律。以下为路线草案，不代表已定稿。

### Phase 9 — 打通最小可用闭环（飞行计划 + 服务器 + 前端骨架）

- 目标：一条合规航路从"查询"到"显示"到"导出"完整可用的垂直切片。
- 范围：FlightPlan 域定稿（含航段/高度层）；px_server WebSocket 协议与 handler；前端地图（leaflet 或 MapLibre）+ 航路详情；.PLN 导出。
- 依赖：FuelEngine 计划（进行中）；altitude_planner（Phase 8 骨架）落地。
- 验收：`./px_tests` 全绿；`px_server` 启动后前端可完成 KLAX→KJFK 查询并导出 .PLN。

### Phase 10 — 燃油与性能上屏

- 范围：FuelEngine 各任务落地（ISA/WindProfile/查表/积分/门面/PTF 解析/FuelJsonModule）；前端性能页面（三阶段明细、密度高度、TAS/CAS、成本指数输入）。
- 对标 SFP：燃油计算器、密度高度、TAS、成本指数（0-999）。
- 验收：与 OpenAP 参考实现对拍 <1%；golden 基线 ±3%。

### Phase 11 — 气象与安全增强

- 范围：WindSource（Open-Meteo + TTL 缓存 + 离线降级）接入；风温影响巡航层推荐（altitude_planner 双模式）；地形剖面图（MORA 网格前端渲染）；日出日落。
- 对标 SFP：风估计、地形感知、日出日落指示器。
- 验收：离线时功能降级可用；剖面渲染正确。

### Phase 12 — 航路检查与生态

- 范围：空域数据源评估（OpenAIP 候选：CTR/TMA/禁限区多边形 + 5 NM 缓冲 + 分级警告）；SID/STAR 程序选择器（多程序对比，对标 SFP 三层策略）；SimBrief 导入；.FMS 导出；.PLN 99 点智能简化与拆分导出；分享链接（如引入轻后端）；备降规划器（对标 150 NM 过滤）；NOTAM 评估；签派报告（Flight Release 打印）；ETOPS 圆。
- 对标 SFP：航路检查、备降规划器、SimBrief 导入、分享、NOTAM、签派报告。
- 验收：每项独立验收。

### 远期候选（差异化，SFP 无）

- 多路径候选航路对比（Yen-Lawler 多路径 + 多因子评分上屏）。
- PMDG/Fenix 数据版本管理（多 AIRAC 并装、diff、更新提醒）。
- 合规约束搜索（SFP 无约束搜索，只能吸附航路）——作为营销叙事与功能差异点。
- Tracker 类本地遥测桥（SimConnect → WebSocket，对标 SFP 但形态不同：Pyxis 已有 C++ 后端，可直接做集成）。
- ETOPS/等时点、按时间规划、飞行日志。

## 5. 附：本文证据来源

- Pyxis 侧：CLAUDE.md、doc/BravoFinder/*.zh-CN.md（domain-design、compliant-routing、route-string、procedure-modeling、terrain-safety、yen-lawler-optimization 等）、doc/superpowers/specs/2026-07-28-bf-engine-integration-design.md、doc/superpowers/specs/2026-07-29-phase7-json-module-design.md、doc/superpowers/plans/2026-07-31-fuel-engine.md、include/px/service/json_module.h、src/server/main.cc。
- SFP 侧：见 simple-flight-planner.md 的来源列表。
