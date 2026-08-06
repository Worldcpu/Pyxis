# 飞行计划模块领域词汇表

> 本文件是飞行计划模块（doc/Flightplan/）的领域词汇表，随 grill 会话实时维护。只收领域术语与既定决策，不收实现细节。相关设计输入：[bravofinder-capability-map.md](bravofinder-capability-map.md)、[competitor-analysis](../competitor-analysis/)。

## 术语

| 术语 | 英文 | 定义 |
|---|---|---|
| 飞行计划 | FlightPlan | 聚合根。一次航班的完整计划：航班元数据 + 航段序列 + 航路点序列视图 + 巡航高度 + 燃油结果 + 校验结果。不可变值类型。 |
| 航段 | FlightSegment | 一等的值类型，飞行计划的计算单元。带类型、两端点、via、距离。独立于 bf 类型，经 FromBf() 转换产生。 |
| 段类型 | SegmentKind | 航段枚举：kSid / kEnroute / kStar / kApproach / kAlternate。**无滑行段**——滑行时间是用户设置项，由燃油政策单独处理。 |
| 程序聚合段 | — | SID/STAR/进近各自 = 一个段（携带程序/transition 名，引用 bf::Procedure），不展开成内部 leg。 |
| 航路点序列视图 | waypoint sequence | 从 bf::Route.points 一次性转换的完整点序列（含程序内部点），用于 navlog 与地图呈现；与段序列互不推导，真源同为 bf::Route。 |
| 候选航路 | candidate route | bf 多路径（Yen k）输出的多条航路；候选只在航路层面不同，油量/高度在选定后计算。 |
| 燃油阶梯 | fuel ladder | taxi → trip → contingency → alternate → final reserve → additional → extra → block（SimBrief 式政策分解，FuelEngine 已设计）。 |
| 调度单 | OFP / dispatch | 中文调度单输出：基本信息 / 巡航高度 / 风源 / 航路 / 备降 / 配载 / 燃油阶梯 / 重量限制 / 检查结果。 |
| 检查结果 | check result | 重量与燃油限制校验结论（通过 / 超限警告 / 任务不可执行）。 |

## 已定决策

| # | 决策 |
|---|---|
| 1 | 设计交付物 = 全栈设计文档（领域层 + service JSON 形状 + WebSocket 协议 + SimBrief 式设置界面规格），实施按 Phase 9-12 分阶段，本次止步设计定稿。 |
| 2 | 领域模型 = **航段一等模型**（B）：FlightSegment 为真源，航路点序列为派生视图。 |
| 3 | FlightSegment 为**独立值类型**，不复用 bf::RouteLeg；转换在 FromBf() 层一次性完成。 |
| 4 | 段粒度 = **程序聚合段**（A）：SID/STAR/进近各 1 段，enroute 逐航路段，DCT 单独段，备降整体 1 段。 |
| 5 | **无 taxi 段**：滑行时间由用户设置，滑行油由燃油政策计算，不进航段序列。 |
| 6 | 计算流水线 = **两步式**：① plan.routes 生成候选航路列表（仅航路信息，快）→ 用户选定 → ② plan.generate 用完整设置（机型/油量政策/业载/风源/滑行时间）生成完整 FlightPlan。FlightPlan 保持单计划纯度。 |
| 7 | plan.routes 请求 = 起降场/跑道/SID/STAR/高度带（默认 FL250-410 保证 MORA）/高低空偏好/途经点/避让点/航路规则/k（默认 5）/random_seed。**seed 不进 UI**：不带 = 确定性 k 最短路；"换一批方案"按钮由 px 层管理 seed，响应回传，FlightPlan 元数据携带以便复现。 |
| 8 | 巡航 FL 在**第二步**确定：自动（默认）/ 手动（覆盖）。自动 = 候选层集（半球规则/中国 RVSM 按航向过滤）→ **升限硬过滤** → 有在线风温（WeatherSource）则选升限内**顺风时间最优**层（唯一目标，不做燃油联动），无则按航程经验降级。输出 (FL, 米制等价, 规则名) 三元组；手动输入只校验提示不拦截。 |
| 9 | plan.routes 候选条目 = index/route_string/距离（总分+分阶段）/sid/star/跑道/connection_kind/完整点序列/seed。**不带 segments**（generate 阶段才物化）。排序按引擎代价序。备降建议为独立端点 plan.alternates，**排序以距离为先**，不进 routes 响应。 |
| 10 | 航司级字段分层：**保留 Callsign**（调度单头部显示，可选）；其余砍。两层 TODO：**TODO-L1** = VATSIM/IVAO 提交必需（ICAO Equipment/Transponder/PBN/Item 18/Type of Flight/Pilot ID，Phase 12 一并补）；**TODO-L2** = 全量对齐（SELCAL/Mode-S/Fin/Dispatcher Remarks 等）。 |
| 11 | 滑行 = **taxi_out_min / taxi_in_min 分离**（默认 20/8，FuelEngine Task 7 FuelInput 契约修订点）：出港油进 block，进港油在到达油科目扣减。 |
| 12 | 备降：过滤参数先做 距离上限(默认 400NM) + 跑道下限(默认 7000ft) + 排除列表；气象过滤（云高/可见度）TODO(Phase 11 METAR)；自定义备降航路 TODO。**备降距离默认 DCT 大圆**（建议与生成阶段一致），备降条目预留 `route` 字段（默认 "DCT"）保证未来兼容。 |
| 13 | 业载双入口单向：pax_count + cargo_kg → 配载（单位旅客/行李重量字段进机型档案）→ payload/ZFW；`zfw_kg` 直接输入则跳过配载。反推（ZFW→商载 等）TODO。FuelEngine 契约不动（仍收显式质量链）。 |
| 14 | plan.generate 响应 = meta/altitude 三元组+依据/航路（route_string+segments+点序列）/备降/配载/燃油阶梯/重量+限制/检查结果。**JSON 唯一真源**，调度单文本/打印由前端渲染（OFP Layout 版面切换因此放前端）。 |
| 15 | 服务层分工 = **薄桥接**：raw 查询（waypoints/airports/procedures/airways/holds 等）透传 bf::service 9 个 handler（JSON 形状 bf 定，api_keys 单源）；plan 流程（routes/generate/alternates）px 直调 NavDatabase 按 px 形状渲染。RouteJsonModule 退役，JsonModule 注册表保留给 px 域模块（flightplan/fuel）。构建需补 libs/http_server 检出 + 两个 add_subdirectory。 |
| 16 | WebSocket 协议 = **JSON-RPC 2.0 风格**（method/params/id，错误码/通知语义）。消息集：px 专属 plan.routes / plan.generate / plan.alternates；透传 lookup_waypoints / lookup_airports / lookup_procedures / lookup_procedure_legs / lookup_airways / lookup_navaid_detail / lookup_holds / parse_route / list_cycles；**find_routes 不暴露**（与 plan.routes 双入口漂移）。 |
| 17 | 导出 = **后端导出**（.PLN 是行业格式转换非 UI 渲染）：plan.export 端点，输入 = 前端回传 FlightPlan JSON + 格式参数（msfs2024/fsx/…），后端校验 → 查机场详情 → 生成 XML 返回；前端只 blob 下载 / Tauri 写盘。Phase 9 仅 .PLN；多格式抽象（LNM RouteExportFormat 式）、99 点简化、拆分导出 Phase 12 TODO。 |
| 18 | 错误模型 = JSON-RPC error code 分区（-32000 内部 / 422 无解 / 404 查无 / 400 参数错），复用 px::ErrorCode + FromBfErrorCode()，不新建枚举。generate 秒级任务**不做进度通知**。多 AIRAC：list_cycles 透传 + 请求可带 cycle，默认最新。 |
| 19 | 缓存架构 = **先服务后建缓存**：有 .bfdb 且 provenance（cycle/loader/data_dir）匹配 → OpenCached（毫秒）；缺失/失配 → Open(源数据)（秒级立即服务）+ 后台 WriteUnified 落盘，实例不热替换（收益下次启动）。**缓存目录用户可配**（设置界面），默认平台数据目录（Tauri appDataDir/bfdb，Windows %APPDATA% / macOS Application Support / Linux XDG）；路径持久化在 appdata 设置文件；px_server 收 --cache-dir / --data-dir / --navdata-dir 独立参数；测试以运行时传参指定临时目录。 |
| 20 | 在线适配 = WeatherSource（Open-Meteo 批量采样，单请求多坐标，按航路点 ≤80 点等比例抽样）；**失败降级链不报错**：在线风温 → 手动风 → 无风，响应 `wind_source` 标注实际来源，altitude auto 同步降级规则层；ETD 决定采样时刻，缺省当前 UTC。 |
| 21 | airframe 档案（flightplan 域字段）：type/variant/engine_category/weights（DOW/MZFW/MTOW/MLW/service_ceiling）/payload（unit_pax_kg/unit_bag_kg）。存储 data_dir/airframes.json + airframe.list/get/upsert/delete 四端点。**燃油域字段（三 profile/fuel_factor/双引擎契约修订/.lnmperf 导入联调）全部 TODO，留待燃油引擎专门设计**（双引擎方向已定：OpenAP 实验性 + lnmperf 经验法则）；experimental 标志字段位已预留在响应 meta。**修订（2026-08-06 fuel 会话）**：airframe 档案补 **`perf_source` 字段**（kLnm/kOpenAp/kCustom/kFcom）——引擎隐含模型的跨模块修订点，见 [fuel glossary 决策 6/8](../fuel/glossary.md)。 |
| 22 | Navlog 逐点字段位预留：generate 响应点序列带 cum_nm/wind/gs/ete/utc（数值计算 TODO——TAS 属燃油域，待燃油引擎设计后填充；字段位先立住保 JSON 形状稳定）。真实航线导入（参考实现 CSV 航线库）不进本次范围（Phase 12 候选）。 |
| 23 | **generate 航路输入统一 route_string**（迭代修订）：放弃候选 index（无状态重算存在竞态：参数手改/seed 变化/周期切换后 index 漂移）；前端把选中候选的 route_string 传回，手写航路同路径合一；bf::ParseRoute 校验。 |
| 24 | **协议固定 SI 公制基准**（迭代修订）：kg/FL/NM/kt；units 移出所有请求（与 FuelEngine 决策 6 一致）；前端渲染时按全局设置换算（O(1) 纯数学）。 |
| 25 | **巡航层与高度带构造性一致**（迭代修订）：手动 cruise_fl → routes 高度带联动锁 [FL,FL] 单点带（bf 原生语义 min==max，候选与 MORA 按该层搜索，对齐 SimBrief Estimated Altitude）；auto → 候选层集 ∩ 高度带内选层。候选与最终巡航层永远一致，无需运行时校验。**单点带短路（2026-08-06 修订）**：CandidateLevels 收到 min_fl == max_fl 时跳过规则层合法性（该 FL 不在 ICAO 半球层位/中国 FLAS 表内也返回单层——决策 8 手动只校验提示不拦截，超限警告走 PlanChecks）；超升限同样不拦截；米制等价为中国表内层命中表值、否则 FL×30.48 近似；kAuto 时手动锁优先于规则解析。 |
| 26 | **mora_checked 标记**（迭代修订）：generate 响应 meta 标注地形安全是否覆盖——候选路径 true（搜索已带 MORA）；手写航路 false，前端提示。上游 bf PR（ParseRoute 支持 MORA 检查）待办。 |
| 27 | **altitude_rule 三态**（迭代修订）：auto（默认，按起降场区域推断：中国十 FIR 双向 → 中国米制档，否则 ICAO 半球档）/ icao / china，手动可覆盖。 |
| 28 | **airframe.upsert 校验**（迭代修订）：物理不等式链 DOW ≤ MZFW ≤ MTOW、MLW ≤ MTOW、单位旅客/行李重量 > 0、service_ceiling > 0；非法返回 400 + 字段错误。 |
| 29 | **UI 规格**（ui-ux-pro-max 设计系统，见 ui-spec.md）：浅色为主 + 暗色例外；Fira Sans + Fira Code（等宽数据）；密度 8/10；单页三区（设置面板/地图/结果区）；**Leaflet**（轻量化 ~42KB，500 点量级够用；MapLibre 否决）；**图层系统 LayerManager 从 Phase 9 按叠加架构设计**（底图/候选航路/标注/备降 + 后续气象/空域/在线网络层即插即用，显隐持久化）；表单 SimBrief 式分区 + 内联校验 + 渐进披露。 |
| 30 | **互锁修订（2026-08-06 fuel 会话）**：① **FlightSegment 字段扩展**（决策 2 修订）：补 from/to 坐标（航迹角可导）+ 程序段 top_fl（SID 顶高，取值规则见 [fuel glossary 决策 13](../fuel/glossary.md)）② **generate 请求不暴露 fuel_factor**（归 airframe 档案）③ **点序列补 segment_index**（决策 22 修订：起飞机场点 = 爬升段 0，落地机场点 = 下降段，程序内点均有归属）④ 采样点 = 航路点+机场+程序端点 × 多层变量（决策 20 修订，API 分批防护）。 |
| 31 | **flightplan 测试策略六面**（迭代审议 C，入实施计划）：① FromBf 转换（legs→segments kind 映射/点序列完整性/top_fl 提取含 transition 键/segment_index 赋值）② 配载（pax/cargo→ZFW、zfw 直接输入跳过）③ altitude 规则层（半球层集/中国 RVSM/航向过滤/升限过滤/顺风最优/无风降级）④ **JSON golden**（plan.routes/generate/alternates 响应快照，与 fuel P2 golden 呼应）⑤ 协议集成（JSON-RPC 分派→错误码 400/404/422/-32000 映射）⑥ seed 确定性（同请求同 seed 同结果）。测试纪律：运行时传参临时目录，不暴露本地环境。 |
