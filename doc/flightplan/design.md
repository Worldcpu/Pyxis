# 飞行计划模块设计

> 状态：grill 收敛（2026-08-06，22 项决策）。决策权威：[glossary.md](glossary.md)；引擎事实：[bravofinder-capability-map.md](bravofinder-capability-map.md)；竞品输入：[competitor-analysis](../../.note/competitor-analysis/)。实施按 Phase 9-12 分阶段（见 §11），本文件是实施计划（writing-plans）的输入。燃油引擎（FuelEngine 计划）与本设计衔接点见 §9，燃油域细节留后续专门设计。

## 1. 定位与范围

飞行计划模块 = 从"用户设置"到"完整飞行计划（OFP 数据）"的垂直切片：领域层（FlightPlan 域）→ service 层（JSON 形状）→ px_server（WS 协议）→ 前端（SimBrief 式设置界面规格）。全栈设计、实施分阶段、本次止步设计定稿。

依赖：bf v3（bf196e3，能力见能力地图）、FuelEngine 计划（衔接见 §9）、bf::service 层（MIT，薄桥接）。

## 2. 领域模型

**航段一等模型**（ADR-0001）：`FlightSegment` 为一等不可变值类型，航路点序列为派生视图。

```cpp
enum class SegmentKind { kSid, kEnroute, kStar, kApproach, kAlternate };  // 无 taxi 段

struct FlightSegment {          // 独立值类型，不复用 bf::RouteLeg
  SegmentKind kind;
  std::string from_ident, to_ident;
  std::string via;              // 航路名 / "DCT" / 程序名
  double distance_nm;
};

struct FlightPlan {             // 聚合根，不可变
  // meta: 机型/呼号/起降场/跑道/ETD/AIRAC cycle/fuel engine 标识+experimental
  // altitude: (FL, 米制等价, 规则名) 三元组 + 推荐依据
  // segments: vector<FlightSegment>   ← 真源（程序聚合段：SID/STAR/进近各 1 段）
  // points: 完整点序列视图（含程序内点；generate 带 cum_nm/wind/gs/ete/utc 字段位）
  // alternate: {icao, route("DCT" 默认), distance_nm}
  // payload: pax/cargo/payload/ZFW
  // fuel: 燃油阶梯（FuelResult 直出，字段位预留）
  // weights: DOW/ZFW/TOW/LW + 限制 + 超限标记
  // checks: 通过 / 超限警告 / 任务不可执行
};
```

转换：`FromBf()` 在转换层一次性完成 `bf::Route → FlightPlan`（segments 与 points 两视图均派生自 bf::Route，互不推导）。

## 3. 模块边界

```text
lib/module/flightplan/     纯 C++20：FlightPlan/FlightSegment/配载/altitude_planner/校验
  └─ 依赖 bf_adapter（FromBf 桥接），无 JSON/网络/UI
lib/module/navdata/        纯 C++20：导航数据只读视图（px_navdata）——链接 bravofinder 库经
  └─ 公开 API 读 bfdb（UnifiedCache graph/CIFP），自建机场索引 + alternates 两阶段过滤；
     未来地图航路图数据基础（决策 48）；不动 bf 源码（fork 可干净更新）
service/px_service         plan.routes / plan.generate / plan.alternates / plan.export /
                           airframe 四端点渲染；raw 查询透传 bf::service（薄桥接，ADR-0002）
px_server                  HTTP/1.1 传输（复用 bf_http_server）+ JSON-RPC over POST 分派
                           （ADR-0003/0004；决策 43：全 200 + error body，QueueWork offload 决策 44）
src/web                    SimBrief 式设置界面（§7）；调度单文本/打印前端渲染
src/desktop (Tauri)        目录解析（appDataDir）+ 子进程 + 写盘；无业务逻辑
```

构建：lib/CMakeLists.txt 补 `add_subdirectory(bravofinder/libs/http_server)` + `(bravofinder/libs/service)`；sparse-checkout 补 `libs/http_server`。bf_service_lib 只能被 px_service 链接（app 层库）。

## 4. 计算流水线（两步式）

```text
plan.routes（第一步，快）：
  请求 10 字段 → bf FindRoutes(k=5) → 候选数组
  [index, route_string, distance_nm(总分+分阶段), sid, star, dep/arr_runway,
   connection_kind, points(ident/lat/lon/via), seed]
  → 用户选定 → plan.generate

plan.generate（第二步，秒级，异步等待）：
  输入 = routes 参数重发 + route_string（统一：候选选中或手写，决策 23，ParseRoute 校验）
        + airframe + 业载 + 油量政策 + 风源 + taxi_out/in + cruise_fl|auto
        + altitude_rule（auto/icao/china，决策 27）+ etd + cycle
        （协议 SI 公制基准，无 units——前端换算，决策 24）
  流水线：巡航高度（auto：规则层→升限过滤→顺风时间最优；候选层集 ∩ 高度带，决策 25）
        → 配载（pax/cargo → ZFW；zfw_kg 直接输入则跳过）
        → 燃油（fuel 域接口位，TODO §9）→ 校验 → FlightPlan JSON
  一致性：手动 cruise_fl 时 routes 高度带联动锁 [FL,FL] 单点带（决策 25）；
         响应 meta 带 mora_checked（手写航路 false，决策 26）

plan.alternates（独立）：请求 = arr + max_distance_nm(400) + avoid_icaos
  → px_navdata 过滤（graph 坐标大圆距离 → 4 字 ICAO → 排除，决策 12 修订：跑道过滤砍掉）
  → 距离为先排序 → [{icao, distance_nm, route}]（name 省略——bf 无机场名称数据）

plan.export：输入 = 前端回传 FlightPlan JSON + format(msfs2024/fsx/…) → 校验 → 查机场详情
  → 生成 .PLN XML → {format, filename, content}；前端 blob 下载 / Tauri 写盘
```

失败语义：bf 无解（422）/查无（404）/参数错（400）映射 JSON-RPC error code；风源失败降级不报错（`wind_source` 标注实际来源）。

## 5. 错误模型

JSON-RPC error code 分区：`-32000` 内部 / `422` 合法但无解 / `404` 查无 / `400` 参数错。复用 `px::ErrorCode` 11 值 + `FromBfErrorCode()`，不新建枚举。bf handler 透传时 `status ≥ 400 → error`，`200 → result`。**传输层（决策 43，ADR-0004）：全部 RPC 响应（含错误）HTTP 200**，错误语义全在 body error 对象；传输层错误（非 JSON body）HTTP 400；非 POST 400。

## 6. 数据与缓存

多 AIRAC：`list_cycles` 透传，请求带 `cycle` 参数（默认最新）。缓存架构（决策 19/49）：**先服务后建缓存**——有 .bfdb 且 provenance（cycle/loader/data_dir）匹配 → OpenCached（毫秒）+ px_navdata 读索引（毫秒）；缺失/失配 → Open(源数据)（立即服务 routes/generate）→ **同步 WriteUnified 落盘（首启秒级，服务上线前完成，决策 49）** → UnifiedCache::Open 建 px 机场索引 → alternates 可用；实例不热替换（收益下次启动）。**缓存目录用户可配**（设置界面），默认平台数据目录（Tauri appDataDir/bfdb，跨平台解析归 Tauri）；路径持久化在 appdata 设置文件。px_server 参数：`--cache-dir / --data-dir / --navdata-dir`（测试运行时传参临时目录）。

## 7. 在线适配（WeatherSource）

Open-Meteo **批量采样**（单请求多坐标，按航路点 ≤80 点等比例抽样，参考实现逐点串行的教训）。**失败降级链不报错**：在线风温 → 手动风 → 无风；altitude auto 同步降级规则层经验。ETD 决定采样时刻，缺省当前 UTC。消费方：altitude auto（顺风时间最优）、燃油（WindProfile，fuel 域）、Navlog 字段位（决策 22）。

## 8. 协议消息集（JSON-RPC 2.0 over HTTP/1.1）

传输：单路径 `POST /rpc`，全 200 + error body（决策 43，ADR-0004）；重 handler QueueWork offload（决策 44）。

```text
px 专属: plan.routes / plan.generate / plan.alternates / plan.export
        airframe.list / airframe.get / airframe.upsert / airframe.delete
透传:   lookup_waypoints / lookup_airports / lookup_procedures / lookup_procedure_legs /
        lookup_airways / lookup_navaid_detail / lookup_holds / parse_route / list_cycles
不暴露: find_routes（与 plan.routes 双入口漂移）
```

generate 秒级任务不做进度通知（异步请求-响应即可）。

## 9. 与 FuelEngine 衔接（接口位预留 + TODO）

预留接口位（响应 meta 已含 engine 标识 + experimental 标志；FlightPlan.fuel 阶梯字段位）。**TODO（燃油引擎专门设计时处理）**：FuelInput 契约修订（`taxi_time_min → taxi_out_min + taxi_in_min` 默认 20/8；alternate 备降段使用 flightplan 域 FlightSegment）、三 profile 性能参数、fuel_factor、双引擎选择（OpenAP 实验性 / lnmperf 经验法则，方向已定）、.lnmperf 导入联调（解析器 FuelEngine Task 19 已有）、Navlog 时间/风数值（TAS 属燃油域）。**上游 bf PR 待办**：ParseRoute 支持 MORA 检查（消除手写航路 mora_checked=false 缺口，决策 26）。

## 10. SimBrief 差距（已覆盖 vs TODO）

已覆盖：燃油阶梯预设规则（5%/15min ↔ kEuOpsMax、45min ↔ kFaa121）、业载、高度 auto/manual + 规则层（半球/中国 RVSM）、备降过滤（距离/跑道）、多候选 k=5、.PLN 导出、OFP 区块、EOBT/ETD、Units、多 AIRAC、airframe type/variant、taxi 分离、Navlog 字段位。
TODO：ETOPS、Stepclimbs、Tankering、多备降、MEL/ATC/WXX 附加油、OFP Layout 版面、Runway Analysis（已砍）、avoid SIDs/STARs/FIRs（bf 部分支持）、NOTAM/METAR/TAF/航图/日出日落（Phase 11-12）、分享链接、FMS Downloader 写机模目录、真实航线导入（Phase 12 候选）、航司级字段两层 TODO（L1 VATSIM 必需 / L2 全量）。

## 11. 分阶段实施建议（Phase 9-12 更新）

**Phase 9（最小闭环）**：FlightPlan 域定稿（FlightSegment 值类型 + FromBf 转换 + 配载 + altitude_planner 规则层）；px_server JSON-RPC 框架 + plan.routes/generate/alternates + airframe 端点；bf::service 薄桥接（补 http_server 检出）；缓存架构；前端地图（leaflet/MapLibre）+ 候选列表 + 设置界面骨架；plan.export .PLN。验收：KLAX→KJFK 查询→生成→导出闭环，`px_tests` 全绿。
**Phase 10（燃油上屏）**：FuelEngine 设计迭代（双引擎/TODO 项）→ 燃油阶梯 JSON 填充 → 前端性能页。
**Phase 11（气象与安全）**：WeatherSource 批量采样落地（altitude auto 顺风最优、Navlog 数值）、地形剖面、日出日落、METAR 评估。
**Phase 12（生态）**：导出多格式抽象、99 点简化/拆分、SimBrief 导入、分享链接、NOTAM/航图、备降气象过滤、ETOPS、VATSIM 提交（TODO-L1 字段）。
