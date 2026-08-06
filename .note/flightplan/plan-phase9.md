# Phase 9 实施计划（最小可用闭环）

> 状态：设计定稿（2026-08-06，flightplan 31 决策 + fuel 14 决策互锁）。输入：[design.md](design.md)、[glossary.md](glossary.md)（决策权威）、[ui-spec.md](ui-spec.md)、[fuel/design.md](../fuel/design.md)、[bravofinder-capability-map.md](bravofinder-capability-map.md)。本计划按 CLAUDE.md 工作流在 worktree 中实施；每个 Task 完成须 clang-format + 测试绿。

## 目标

一条合规航路从"查询"到"显示"到"生成计划"到"导出 .PLN"完整可用的垂直切片：KLAX→KJFK 查询 → 候选 → 选定 → 完整计划（含燃油接口位）→ 导出。验收：`px_tests` 全绿 + 前端可完成闭环。

## 前置（一次性，非 Task）

1. `git -C lib/bravofinder sparse-checkout add libs/http_server`（bf::http_status 头）
2. `lib/CMakeLists.txt`：engine 后追加 `add_subdirectory(bravofinder/libs/http_server)` + `(bravofinder/libs/service)`
3. 子模块指针提交（bf196e3，已暂存）

## Task 分解（依赖序）

**T1. px_server JSON-RPC 框架 + bf::service 薄桥接**
- libuv+llhttp WS 传输骨架 → JSON-RPC 2.0 消息模型（method/params/id，错误码分区 -32000/422/404/400，决策 18）
- 透传 9 个 bf handler（lookup_* / parse_route / list_cycles；find_routes 不暴露，决策 16）；status ≥400 → JSON-RPC error
- 测试：协议集成（决策 31⑤：分派→错误码映射）
- 依赖：前置 1-2

**T2. FlightPlan 域（lib/module/flightplan）**
- `FlightSegment` 独立值类型：kind（kSid/kEnroute/kStar/kApproach/kAlternate）/from/to ident + 坐标/top_fl/distance_nm（决策 2-5 + 互锁 ①）
- `FlightPlan` 聚合根：meta/segments/点序列（含 segment_index，点 1=爬升段 0）/altitude 三元组/payload/weights/checks/fuel 字段位
- `FromBf()` 转换：bf::Route → FlightPlan（legs→segments kind 映射、点序列含程序内点、top_fl 提取含 transition 键——决策 31①）
- 测试：FromBf 转换六项（kind 边界/点完整/transition 顶高/segment_index）

**T3. 配载计算**
- pax_count + cargo_kg → payload/ZFW（单位旅客/行李重量）；zfw_kg 直接输入跳过（决策 13）
- 测试：配载（决策 31②）

**T4. altitude_planner 规则层**
- 半球层集（东单西双）+ 中国 RVSM 米制层集 + altitude_rule 三态（auto/icao/china，决策 27）
- 升限硬过滤 + 巡航层<场高报错；顺风最优接口位（WeatherSource 依赖 Phase 11，先行无风降级）
- 测试：规则层六项（决策 31③）

**T5. plan.routes / plan.generate / plan.alternates handler**
- routes：10 字段请求 → bf FindRoutes(k=5) → 候选数组（index/route_string/距离/sid/star/connection_kind/点序列/seed，决策 7/9）
- generate：高度→配载→燃油接口位→校验 → FlightPlan JSON（决策 8/14；mora_checked 决策 26；SI 公制）
- alternates：距离+跑道过滤 + 排除（决策 12）
- 测试：JSON golden 三端点（决策 31④）+ seed 确定性（31⑥）

**T6. airframe 档案 + 四端点**
- data_dir/airframes.json；list/get/upsert/delete；upsert 校验（DOW≤MZFW≤MTOW、MLW≤MTOW、单位重量>0、升限>0、perf_source 必填，决策 21/28）
- 测试：校验链

**T7. 缓存架构**
- 先服务后建缓存（决策 19）：BfdbInventory 扫描 → OpenCached / Open+后台 WriteUnified；provenance 校验
- --cache-dir/--data-dir/--navdata-dir 参数；多 AIRAC list_cycles + cycle 参数
- 测试：临时目录运行时传参（不暴露本地环境）

**T8. plan.export .PLN**
- 回传 FlightPlan JSON 校验 → 查机场详情 → MSFS/FSX/P3D .PLN XML（决策 17）
- 测试：golden 文件

**T9. 前端骨架（ui-spec）**
- 单页三区 + Leaflet 图层系统（LayerManager，决策 29）+ 设置面板骨架 + 候选列表 + 调度单渲染（JSON 真源）+ 单位换算（O(1) 前端）
- 交互流：生成候选→选候选→生成计划→导出（决策 6/13-14）

## 验收

1. `px_tests` 全绿（T1-T8 各测试面 + fuel 契约测试衔接）
2. 前端闭环：KLAX→KJFK 查询→候选→选定→生成→.PLN 导出
3. 燃油接口位就绪：generate 响应含 fuel 阶梯字段位 + engine/experimental 字段（Phase 10 填充）

## 依赖与后续

- Phase 10：FuelEngine 设计迭代（doc/fuel/ 14 决策）→ 燃油阶梯填充 + Navlog 数值（决策 22 字段位）
- 前置 3（子模块指针）与 T1 的前置 1-2 需先落地
