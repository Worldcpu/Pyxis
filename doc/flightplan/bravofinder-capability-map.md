# BravoFinder v3 能力地图（Pyxis 侧集成参考）

> 版本：bf196e3（2026-08-05 同步，程序版本 3.24.12）。本文件是飞行计划模块设计的引擎侧事实基础；能力基于 submodule 磁盘检出分析（libs/engine + libs/service），`apps/`、`docs/`、`tests/`、`libs/http_server` 不在检出（http_server 为集成必需，见第七节）。namespace `bf`，engine 为 LGPL-3.0-or-later，service 为 MIT。

## 一、公共 API 面（libs/engine，target `bf::bravofinder`）

引擎自包含、无 JSON/网络依赖。**唯一公开入口是 `bf::NavDatabase`**（`libs/engine/io/nav_database.h`）：

| 能力 | 签名 |
|---|---|
| 源数据构建 | `static Result<NavDatabase> Open(source_dir, loader_name="xplane12")` |
| 缓存加载 | `static Result<NavDatabase> OpenCached(bfdb_path, CifpLoad::kOnDemand\|kEager)` |
| 写缓存 | `Result<uint32_t> WriteUnified(out_path)`（`bf build` 用） |
| 算路 | `Result<std::vector<Route>> FindRoutes(const RouteRequest&)` |
| 校验航路串 | `Result<Route> ParseRoute(route_str)` |
| 终端 MSA | `std::vector<MsaSector> MsaForAirport(icao)` |
| 批量查询 | `LookupWaypoints / LookupAirports / LookupProcedures / LookupProcedureDetail / LookupAirways / LookupNavaidDetails / LookupHolds`（结果与输入平行，miss 为 nullopt/空） |

其余按层划分：

- **routing/**：`route_request.h`（`RouteRequest`、`AirwayRule`、`FlRange`、`LevelPreference`）、`route.h`（`Route`、`RouteLeg`、`RoutePoint`、`ConnectionKind`）、`route_parser.h`（`TokenizeRoute`）、`route_string.h`（`SplitDesignators`、`BuildRouteString`，压缩 filed-plan 串）、`route_metrics.h`（`CumulativeDistances`）。
- **graph/**：`astar.h`（`SearchOptions`、`ShortestPath`、`SeededEndpoint`、`NodeFilter`/`EdgeFilter`、`TurnPenalty`、`SearchWorkspace`、`MultiGoalHeuristic`、`FindShortestPath[Multi]`、`SelectEdge`、`BuildSeedTable`/`BuildBearingTable`）、`yen_kshortest.h`（`FindKShortestPaths[Multi]`）、`nav_graph.h`（CSR 图、`GraphEdge` 16B）。
- **constraints/**：`constraint.h`（`Constraint::Evaluate(EdgeContext, RouteRequest) → EdgeVerdict{allowed, extra_cost}`，三态 Allow/Block/Penalize）、六个内置约束（见第三节）。
- **domain/**：`procedure.h`（`PathTerminator` 全 23 种、`ProcedureLeg` 36B、`Procedure`、`CifpData`、`Runway`）、`airport.h`（`Airport`）、`airway.h`（`AirwayDirection`、`AirwayLevel`、`AirwaySegment`）、`waypoint.h`（`Waypoint`、`WaypointKind`）、`ident.h`/`fixed_ident.h`（`Ident` 对偶 (ident, 区域码) / 12B `FixedIdent`）、`msa.h`/`mora_grid.h`（`MsaSector`、`MoraGrid` 1° 网格）、`hold_fix.h`、`navaid_detail.h`、`query_types.h`、`result.h`（`Result<T,E>`、`Error`、`ErrorCode` 11 值）。
- **io/**：`nav_data.h`、`loaders/loader.h`（`Loader` 抽象）、`loader_registry.h`（`MakeLoader`）、`build/graph_builder.h`、`build/procedure_connector.h`（`ProcedureConnector`、`Connection`、`ProcedureRef`）、`cache/unified_cache.h`（`UnifiedCache`/`UnifiedData`/`UnifiedHeader`）、`cache/bfdb_naming.h`、`cache/bfdb_inventory.h`（`BfdbInventory`）、`cache/cifp_codec.h`、`cache/nav_detail_codec.h`、`cache/graph_snapshot.h`。

**可扩展接缝**：实现 `Loader` 子类 + 注册即支持新数据源；实现 `Constraint` 子类即新增路由规则。

## 二、路由能力

**算法**：多源/多目标 A*（弦长启发，`MultiGoalHeuristic` 记忆化）之上叠 **Yen K-shortest**（Lawler 优化 + heuristic memoization，官方称提速 ~2.5× 且结果不变）。多源形态支持**每条候选走不同的 SID/STAR 衔接 fix**（`FindKShortestPathsMulti`）。`SearchWorkspace` 用 generation stamp O(1) 逻辑清空。

**`RouteRequest` 字段**（`core/routing/route_request.h`）：
- `departure`/`arrival`：机场 ICAO 或航路点 ident，大小写不敏感
- `altitude`：`FlRange{min_fl,max_fl}` 巡航高度带（百英尺）；**设置后启用高度带 + MORA 约束**
- `level`：`kNone/kLow/kHigh` 高低空偏好（软惩罚）
- `k`：候选数（Yen），默认 1，服务端上限 15
- `departure_runway`/`arrival_runway`：SID/STAR 跑道过滤
- `departure_sid`/`arrival_star`：按名固定程序（`DEEZZ5` 或 `DEEZZ5.TOWIN` 钉 transition），不匹配则报错不静默回退
- `avoid_waypoints`：避让点（`BOTON` 或 `BOTON/LF`）
- `airway_rules`：区域×designator 航路规则（≤32 条）
- `random_seed`：确定性随机化（同 seed 同结果）
- `forced_points`：有序途经点（分段搜索后拼接）

**输出 `Route`**（`route.h`）：`route_string`（ICAO filed-plan 串）、`points`/`legs`（`via` 为航路名/DCT/SID/STAR，并线航路带 `concurrent_airways`）、按阶段拆分距离 `dep/enroute/arr_distance_nm`、`sid/star/dep_runway/arr_runway`、互换程序列表 `sid_options/star_options`、`dep_connection/arr_connection`（4 种连接语义）、终端过渡元数据（`approach/approach_iaf/approach_bearing/approach_options`）、`forced_points`。

另有**转弯惩罚** `TurnPenalty`（A* 内自动启用）：路径相关软惩罚，45° 以下为零、90°≈20NM、180°=200NM。

## 三、约束层

接口：`Constraint::Evaluate(EdgeContext, RouteRequest) → EdgeVerdict{allowed, extra_cost}`。`EdgeContext` 按值持有 `GraphEdge` + 两端坐标 + `from` 顶点索引。组合语义：**任一约束 block 则禁边，软惩罚累加**；软惩罚非负保证 A* 可采纳性。约束无状态、互相不知情。`io/nav_database_routing.cc` 按请求字段组装约束链（未设字段不进链）。

| 约束 | 文件 | 类型 | 判据 / 激活 |
|---|---|---|---|
| `AltitudeBandConstraint` | `altitude_constraints.h` | 硬 | 巡航高度带与航段 `[base_fl, top_fl]` 重叠；`altitude` 已设 |
| `MoraConstraint` | `mora_constraint.h` | 硬 | 高度带顶部不低于沿线 MORA（1° 网格沿大圆采样取最大）；`altitude` 已设 |
| `LevelPreferenceConstraint` | `altitude_constraints.h` | 软 | 不匹配 level 的边加边长比例惩罚（默认 0.5）；`level != kNone` |
| `AvoidWaypointConstraint` | `avoid_waypoint_constraint.h` | 硬 | 封锁所有进入避让顶点的边；排序 vector + 二分；`avoid_waypoints` 非空 |
| `AirwayRuleConstraint` | `airway_rule_constraint.h` | 硬+软 | 两张位掩码表（vertex_mask ≈1.1MB + airway_mask ≈48KB），热路径两次数组读；block 短路、penalize 累加；`airway_rules` 非空 |
| `RandomizeConstraint` | `randomize_constraint.h` | 软 | `Hash01(seed, to, airway_id)` 确定性抖动，kDefaultEps=0.05；`random_seed` 已设 |

扩展纪律：匹配语义前置到构造期（热路径零字符串）；解析器住 io/ 层，约束在 core/ 只收解析产物；路径相关规则进搜索循环而非约束层；软惩罚优先距离比例。

## 四、程序衔接（`io/build/procedure_connector.h`）

机场**必须靠真实程序接入航路网**（机场附近 96 个最近航点只有 1 个在网）。`ProcedureConnector` 四个静态入口：

- `BuildDeparture`（SID 侧）：出口 = **最后一个定点 leg 的 fix**（SID 的 IF 是 transition 分叉点，是记录的"另一端"，不能用）
- `BuildArrival`（STAR 侧）：入口 = **path_term == IF 的 Initial Fix**
- `BuildApproachArrival`（**DCT-to-IAF**）：机场无 STAR 且未指定 STAR 时，把进近 IAF（gate-only，path_term==IF）接入到达候选；在网 IAF 作普通 goal，脱网 IAF 用最近 K 个在网代理 goal（seed 含 |代理→IAF| + IAF→MAPT 程序体，不做 CSR 突变）；**filed string 诚实**：串仍以 `<fix> DCT ARR` 结尾、star 为空，进近名/IAF/磁航向只进元数据
- `BuildDctFallback`：按大圆接最近在网航路点，**分方向**（离场要求有出边，进场要求有入边）

产出 `Connection{fix_vertex, seed_distance_nm（沿程序折线累计）, bearing, approach_bearing, procedures[]}`，`ToEndpoints` 转带 seed 的 A* 端点。衔接 fix 按 vertex 去重聚合所有共享该 fix 的程序（`ProcedureRef`），一次搜索得到全部可互换 SID/STAR。`Route::ConnectionKind` 四语义：`kProcedure`（真用 SID/STAR）/ `kTerminalTransition`（DCT-to-IAF）/ `kRadarVectors`（发布了程序但都到不了在网 fix）/ `kDirect`（无程序数据）。关键设计点：**只接发布衔接点**，机场级回退兜底（cycle 2601 到达侧仅 30 个机场无 STAR）。

## 五、导航数据与缓存

**可插拔 `Loader` 接口**：`LoadNavData`（轻量，不含程序）、`LoadProcedures`（重路径，全周期 ~100MB）、`LoadProcedure`（单机场按需）、`name()`。四个 loader（`loader_registry.h` 的 `MakeLoader` 按名构造）：

| loader | 名称 | 数据源 |
|---|---|---|
| X-Plane 12 | `xplane12`（默认） | 原生 .dat（earth_fix/nav/awy/aptmeta/mora/msa/hold + CIFP/<ICAO>.dat），仅 1200 Version |
| DFD v1.0 | `dfd1` | RealTraffic / SimToolkitPro / PMDG MSFS `navdb.s3db` 等 |
| DFD v2 | `dfd2` | Inibuilds A350 `NG_FWDFD` `db.s3db` |
| Fenix | `fenix` | Fenix A320 `fenix_navdata.db3`（周期从 `CycleName` 配置键读） |

**AIRAC 周期**：从源数据解析（如 2601），`NavDatabase::cycle()` 暴露，无来源时 0；写入 .bfdb 头（`UnifiedHeader{cycle, program_version, source_loader, data_dir}`）做 provenance，**头部才是权威**。

**.bfdb 统一缓存**（`kFormatVersion = 17`）：单文件三段（graph / CIFP / detail）+ 全局字符串池，magic "BFDB"，固定宽度小端，每区段 CRC-32C。规范命名 `nav_<cycle>.bfdb`，零周期回退 `nav.bfdb`；`BfdbInventory::Scan(dir)` 编目多周期。CIFP 段按机场分片、on-demand 用 pread 拉取，eager 模式打开时冻结、之后无锁读。**线程安全契约**：`Open()` 成功后实例只读，所有 const 查询可并发；程序缓存双检锁。数据合规：Navigraph/Jeppesen 版权数据严禁入库/分发。

## 六、bf::service 层（libs/service，namespace `bf::service`，MIT，target `bf_service_lib`）

**registry.h — `NavDatabaseRegistry`**（多 AIRAC cycle 注册表，懒打开 + 线程安全）：`NavDatabaseRegistry(BfdbInventory, CifpLoad = kOnDemand)`（构造零 I/O）、`Result<const NavDatabase*> Get(std::optional<uint32_t> cycle)`（nullopt=最新）、`const BfdbInventory& inventory()`。mutex 只守卫 map 查找/插入，指针跨 rehash 稳定。

**queries.h — 10 个类型化查询入口**（返回 `HandlerResult`，第二参 `OutputFormat fmt`）：
`FindRoutes`、`ParseRoute`、`LookupWaypoints`、`LookupAirports`、`LookupProcedures`、`LookupAirways`、`LookupNavaidDetails`、`LookupHolds`、`LookupProcedureLegs`（airport+procedure）、`LookupProceduresMixed`（CLI 专用，`ProcedureSelector{airport, procedure}`）。

**handlers.h — 传输无关 JSON-args 适配层**：`HandlerResult{std::string body; int status; uint32_t elapsed_ms}`（body 为 JSON 字符串，status 为 HTTP 风格码 200/400/404/422）；服务端上限 `kMaxIdListSize=256`、`kMaxFl=600`、`kMaxK=15`；`QueryHandler = std::function<HandlerResult(const rapidjson::Value& args, const bf::NavDatabase& db)>`；`MakeHandlers()` 返回 **9 个 NamedHandler**（find_routes、parse_route、lookup_waypoints、lookup_airports、lookup_procedures、lookup_procedure_legs、lookup_airways、lookup_navaid_detail、lookup_holds），按值返回、无全局状态。

**render.h**：`OutputFormat{kJson, kText}`；`RenderRoutes`/`RenderRoute`/6 个批量 `RenderXxx`/`RenderProcedureDetail`/`RenderProceduresMixed`/`RenderError`/`JsonError`（RapidJSON Writer SAX，自动转义）。

**api_keys.h**：JSON 键名单一来源（`inline constexpr std::string_view`）：请求参数 `departure/arrival/min_fl/max_fl/level/k/departure_runway/arrival_runway/departure_sid/arrival_star/avoid_waypoints/airway_rules/random_seed/forced_points/ids/airport/procedure/route`；airway_rules 五字段 `region_prefixes/designators/match/action/penalty_fraction`；枚举值 `exact/prefix`、`block/penalize`、`none/low/high`。

**依赖与许可证**：仅 RapidJSON + 标准库（含 <format>）；内部依赖 `bf::bravofinder` + **`bf::http_status`（header-only INTERFACE target，定义在 libs/http_server/CMakeLists.txt）**——`queries.cc`/`handlers.cc` 引用 `http_status.h` 的状态码常量，不链接 bf_http_server、不依赖 libuv。**该头当前不在磁盘检出**，构建 bf_service_lib 前须 `sparse-checkout add libs/http_server`。12 个文件全部 SPDX: MIT。

## 七、Pyxis 集成路径（service 层）

```text
当前：add_subdirectory(bravofinder/libs/engine)          [lib/CMakeLists.txt]
目标：add_subdirectory(bravofinder/libs/http_server)     # 提供 bf::http_status（必需，先于 service）
      add_subdirectory(bravofinder/libs/service)         # 提供 bf_service_lib
```

1. **sparse-checkout 增补**：`git -C lib/bravofinder sparse-checkout add libs/http_server`（cone 模式）。
2. **lib/CMakeLists.txt**：engine 之后追加 http_server → service（顺序即依赖顺序）。
3. **依赖冲突**：http_server 的 CMakeLists 会 FetchContent libuv + llhttp；Pyxis 根已声明**同版本**（libuv v1.49.2、llhttp v9.2.1），FetchContent 同名复用，无版本冲突（需落盘后实测）。
4. **px 层消费两种路径**：
   - **薄桥接**（推荐先行）：px_service 直接 link bf_service_lib，从 .bfdb 目录构造 NavDatabaseRegistry，MakeHandlers() 按名分派；JSON 往返完全在 bf 内部，px 只透传。
   - **类型化直调**：px 侧用 queries.h 的 10 个类型化入口（OutputFormat::kJson），px::Result 桥接只发生在 registry 层（FromBf() 转换）。
5. **分层纪律**：bf_service_lib 是 app 层库（含 RapidJSON），只能被 service/px_service 链接，不可进 lib/px_core。px_service 的"RapidJSON 翻译"职责与 bf_service_lib 的"JSON body 生成"职责重叠，集成时需决定以谁为准。
6. **许可证**：MIT service 层无新增义务；engine LGPL 静态链接合规（随发布附带 libs/engine/LICENSE 与源码）现有集成已成立。

## 八、新增能力：`--airway-filter`（c322618，v3.23.0，breaking）

**背景**：航路用法国别差异（中国的 J 是终端过渡航路，美国的 J 是合法 Jet 航路），源数据无类型字段区分。`RouteRequest` 增 `airway_rules`（≤32 条 `AirwayRule`）；**删除** `avoid_airways`/`--avoid-awy`（新能力是真子集：`--avoid-awy J60` → `--airway-filter='*:J60=block'`）。

**语法**：`<区域>:<航路名>[=block|penalize[:<比例>]]`——尾随 `*` 前缀、无 `*` 精确、裸 `*` 任意，逗号列举；可重复传。示例：`*:J60=block`、`ZB,ZG,...,ZY:J*=block`（中国十 FIR 所有 J 航路）、`VI,VA,VO,VE:J*=penalize:0.3`。`AirwayRule{region_prefixes, designators, Match::kExact|kPrefix, Action::kBlock|kPenalize, penalty_fraction}`（默认 0.5）。

**关键语义**：匹配**逐 leg** 而非按航路名（cycle 2601 有 29.6% 航路名被不连通实例复用，最多 18 个）；leg 命中 = designator 匹配 **且** 任一端点区域命中；区域永远前缀匹配；多条规则命中 block 短路、penalize 累加。两坑：block 是**不可逆连通性断裂**（可能整座机场失联无解，批量区域规则优先 penalize）。

**实现**：`ResolveAirwayRules`（io/nav_database_routing.cc）把名字匹配前置为两张位掩码表（vertex_mask ≈1.1MB + airway_mask ≈48KB），热路径两次数组读；`EdgeContext` 为此新增 `from` 成员；并线航路名（`A14-M1`）经 `SplitDesignators` 拆分后匹配。纯读侧特性，format_version 未 bump。

## 九、引擎版本纪律

任何 C++ 变更须 bump 程序版本；磁盘布局变更才 bump format_version（当前 17）。bf 自带的 HTTP/MCP/CLI 三个前端共享 bf::service 层——Pyxis 复用 service 层时天然获得这三个前端同源的行为一致性。
