# 燃油系统设计

> 状态：grill 收敛（2026-08-06，决策 7 项）。决策权威：[glossary.md](glossary.md)；基线 = [~/.claude/plans/sfp-swift-octopus.md](../../../.claude/plans/sfp-swift-octopus.md)（上一轮 27 决策）+ [flightplan 29 决策](../Flightplan/glossary.md)（兼容约束）。实施时经 writing-plans 展开 Task。燃油域接口位与 flightplan 的衔接见 [design.md §9](../Flightplan/design.md)。

## 1. 定位与范围

燃油系统 = 双引擎计算层：**标准引擎**（lnmperf 经验法则，常量流率模型，默认、覆盖广）+ **实验性引擎**（OpenAP 三阶段积分，PTF 表查表插值，15 架 + FCOM 转化 TODO）。统一 FuelResult 出口，SimBrief 式燃油阶梯政策分解，高空风修正，与 flightplan 模块契约兼容（FuelInput 消费 flightplan 域 FlightSegment）。

## 2. 双引擎架构

```text
                 ┌─ 标准引擎（默认，无徽章）─────────────────────┐
                 │  .lnmperf 常量模型（每阶段固定流率/速度/VS）   │
                 │  预置表（公共领域，内置）+ 用户导入（永久保留） │
                 └──────────────────────────────────────────────┘
FuelEngine 门面 ── ComputeFuel(perf_source 隐含引擎) ──┐
  统一 FuelResult（阶梯 + 时间明细 + 引擎标识）          └─ 实验性引擎（experimental 徽章）─────────┐
                                                                  OpenAP PTF 表（三阶段积分，查表插值）│
                                                                  15 架 + FCOM 转化（TODO，同 schema）│
                                                                  └────────────────────────────────────┘
```

**引擎由机型档案 perf_source 隐含确定**（决策 6）：kLnm/kCustom → 标准；kOpenAp/kFcom → 实验性。UI 双列呈现（经验列 / 实验性列），无显式引擎选择器。JSON 带内部 engine 标识 + experimental 字段位。

## 3. 高空风修正（决策 2 + 修订）

物理链：风 → GS → 段时间 = 距离/GS → 段油耗 = 流率 × 段时间（常量流率模型下风修正 = 地速修正）。**所有段油耗均随风**（时间 = 地面距离/GS，程序要飞完固定地面几何）。

- **GS ≈ TAS − head**（近似式，crosswind 忽略，<1% 误差；精确式 √(TAS²−cross²)−head 留可选档）
- **程序段端点平均风**（修订）：SID 段 GS 用 (机场地面风 + SID 出口点风)/2 的投影；STAR 段 = (STAR 入口点风 + 机场地面风)/2——端点平均天然含垂直风梯度；**端点风高度 = 程序代表高度**（SID 出口/STAR 入口采 ≈FL150 层风，非巡航 FL）；机场端采地面层风；无 SID/STAR 时端点降级为 enroute 首/尾点，一致
- **巡航段逐航路点分段风**：WeatherSource 批量采样（单请求多坐标，**采样集合 = 80 航路点 + 2 机场 + 2 程序端点 + 备降中点**（备降存在时）≤85 坐标 × 多层变量（surface/850hPa≈FL150/巡航层）——审议 A 定稿；坐标上限实测 <85 → 分批 ≤50 合并）；**Navlog 点风 = 计算用风**（SID/STAR 段内点填端点平均风、巡航点填逐点采样风——显示与计算同源，不另存一套）
- **trip 油联动 contingency**（5% of trip）；**备降段**（DCT）取中点风同修正；**final reserve** 时间基准不受风
- **GS 钳制下限 50kt + 警告**（head ≥ TAS 极端风，防除零/负时间；不中断）
- **无风降级**：GS = TAS；WeatherSource 失败降级链（在线 → 手动 → 无风）标注 wind_source

## 4. FCOM 转化（决策 3，架构定案实现 TODO）

FCOM 数据 → 转化器（字段表驱动校验 + 单位统一 SI）→ **OpenAP PTF 表同 schema**（复用三阶段积分器，不建第三引擎）→ airframe 档案指向（provenance: source=fcom + 机型 + 数据日期）。录入：表单先行，表格粘贴导入 TODO。**实现留 TODO**（架构决策已定，产出物 = 生成器 + 录入 UI）。

## 5. 与 flightplan 兼容契约（决策 5，6 项）

| # | 契约 |
|---|---|
| 1 | FuelInput：`taxi_out_min` + `taxi_in_min`（默认 20/8；出港进 block，进港到到达油科目） |
| 2 | alternate = flightplan 域 `FlightSegment*`（kAlternate，DCT 距离 + 中点风） |
| 3 | WeatherSource 批量采样（≤80 点）+ 降级链标注 wind_source |
| 4 | FuelResult 带逐段时间明细（Navlog 数值，决策 7） |
| 5 | 协议 SI 公制基准（kg/FL/NM/kt）；单位换算前端职责 |
| 6 | engine 标识 + experimental 字段位（响应 meta） |

## 6. Navlog 数值产出（决策 7，SimBrief 思路）

主航路**逐点**：cum_nm/wind/gs/ete/utc——ETE = 距离/GS；UTC 链含 taxi_out 偏移（起飞点 = EOBT + taxi_out，此后累加）。**备降段单值到达时刻**（DCT 单段无点序列；供 Phase 11 日出日落与 ETA 显示；不参与 final reserve）。TAS 每阶段常量（lnmperf 语义），OpenAP 可填真实演化值。

## 7. 许可（决策 4）

lnmperf 预置表 = **公共领域认定**（事实性性能参数，无创造性表达）；Task 21 内置表解阻塞，产物 = 转换后 JSON（tools/ 脚本）+ **NOTICE 来源署名**（littlenavmap.org）。导入路径永久保留（运行时 service 解析器实时转）。

## 8. 计算方式（公式级）

> 本节为计算规格；标注【待确认】的公式项由 grill 定稿后移除标注。

### C1. 风分量与 GS
- 相对角：θ = 最小夹角(|风向 − 段航迹角|, 360° − |风向 − 段航迹角|)，θ ∈ [0°, 180°]；段航迹角 = 段大圆初始方位角
- 逆风分量：head = W × cos θ（cos 自然带符号：θ > 90° 为顺风，head < 0）
- GS = TAS − head（决策 2 近似式，crosswind 忽略；精确档 √(TAS²−cross²)−head 留 TODO）
- 无风降级：head = 0 → GS = TAS

### C2. lnmperf 常量模型积分（程序衔接点切分 + 过渡段修正）
- 每阶段 i：time_i = dist_i / GS_i（GS 含该段风修正）；fuel_i = flow_i × time_i
- **切分（L2 定稿）**：爬升段 = [起飞, SID 出口 fix]（SID 段）；巡航段 = [SID 出口, STAR 入口]（enroute 段全部）；下降段 = [STAR 入口, 落地]（STAR + approach）；流率 = Climb/Cruise/Descent/Alternate。**无 SID/STAR**：爬升段 = [起飞, enroute 起始点]（DCT 连接段）、下降段 = [enroute 终点(=IAF), 落地]（含进近体）。FlightSegment kind 即切分边界（转换层按 kind 分组）
- **过渡段修正（P1 定稿，数据验证）**：SID 出口（顶高）→ 巡航层的继续爬升 = 过渡段，流率 = Climb：t_transition = (巡航FL − SID顶高)/VS_climb（VS 常量重新启用）；fuel = Climb流率 × t（时间锁定不随风，同 L1 原逻辑）；下降侧对称（巡航层 → STAR 入口高度，VS_descent）。**SID 顶高查取路径（审议 B 定稿）**：route.sid（**含 transition 全名**，如 GURUN-9D.TOWIN——bf 按 name+transition 双键区分记录）→ LookupProcedureLegs → 末段定点 leg（BuildDeparture 出口 leg）→ alt1_ft（kBetween/kAt/kAtOrBelow 上界）；**STAR 对称**（入口 IF leg 的 alt 约束）；**route.sid 空（无 SID）→ 跳过过渡修正**（enroute 起点高度未知 → 默认过渡高度 10000ft）；kNone 约束回退同 10000ft。修正消除实测 358-450kg/次低估（Climb = 2.26× Cruise）
- **备降段纯常量**：Alternate 无 VS → 不做过渡修正（DCT 短段，误差可接受）
- **边界**：巡航层 < 场高 → 报错 400；巡航层 < SID 出口高度 → 过渡段为负 → 钳 0（无过渡，全程序内爬升）；短航线巡航段近零 → 接受；GS 钳制下限 50kt + 警告
- taxi：TaxiFuelLbsGal 直接值（不乘时间）；政策字段**原样直读 + 异常值警告**（Contingency >10% 时 UI 警告"应急油比例异常高"，C1 定稿——Fenix 档案实测 30%）
- alternate：Alternate 阶段 flow_alt × (备降 DCT 距离 / GS_alt)（GS_alt 用中点风）

### C3. OpenAP 三阶段积分（动态查表 + 细分）
- **查表**：PTF 表 = FL × 三质量级（BADA 13 列语义，openap_gen 扩展重量维），**双线性插值** → flow(t, m, h)、TAS(t, m, h)、ROC；**表外报错**（400，指明维度 + 低巡航层提示切标准引擎；不钳制）；**备降/等待 1500ft AGL 查表 < FL30（表最低层）→ 钳制到最低行**
- **段划分同 C2**（程序衔接点切分 + **过渡段修正**：SID 顶高 → 巡航层用爬升表积分，dt = dh/ROC(查表)，质量演化）
- **爬升/下降段细分积分**（P1 定稿——数据推翻"段内单值"）：爬升流率随高度降 25%（FL150 4393→FL350 3305 kg/h），单值查表误差 ±10-15% 超"单阶段 <10%"容限——**爬升/下降段按高度步进（~1000ft/FL）细分**，每步查表（climb 表带 γ 项，平飞流率低估 30%+ 禁止）；**巡航段逐航路点分段查表**（每点对 = 当前质量 × 巡航FL，风点即查表点）
- **质量演化链（单遍、链式无环）**：TOW → −taxi → −爬升细分 → −巡航逐点 → −下降细分 → −备降 → −reserve → LW
- **生成器网格规格**（openap_gen）：FL0/30 起步、≤2000ft 步长、**CAS→Mach 过渡高度强制格点**（VS 不连续 -37%，跨格插值误差不可接受）、climb 表三质量流率扩展（low/nom/high）
- taxi：0.3 × 巡航流量(TOW × 巡航FL) × (taxi_out + taxi_in)

### C4. 燃油阶梯（FuelPolicy，SimBrief 对齐）
- taxi_kg：按 C2/C3 引擎 taxi 值
- trip_kg = 爬升 + 巡航 + 下降积分和（风修正后）
- contingency_kg：kIcao5Pct = 0.05×trip；kEuOpsMax = max(0.05×trip, flow_wait×5min)；kFaa121 = flow_cruise×45min；kFromProfile = profile 的 ContingencyFuelPercent；kCustom = 用户百分比
- alternate_kg = 备降段积分（DCT 距离 + 中点风）
- final_reserve_kg = 30min × flow(**备降场标高+1500ft AGL** 巡航流率, 备降终点质量)（SimBrief 语义；lnmperf：ReserveFuel 直接值）
- **政策流率两引擎口径分离**：OpenAP 引擎 = **1500ft AGL 体系**（flow_wait/contingency-5min/reserve 查表高度 = 备降场上空 1500ft AGL 巡航流率，SimBrief 对齐，替换旧决策 (arr_fl+15) FL）；**lnmperf 引擎** = 档案 Cruise 常量流率（或 .lnmperf 的 ContingencyFuelPercent/ReserveFuel 直接值，kFromProfile 预设）
- **备降段查表高度 ≠ reserve 等待高度**：备降段飞**备降巡航高度**（hold_fl 参数，默认 FL150）查表积分；reserve 在**1500ft AGL** 等待查表——两个高度分离
- extra_kg = 用户输入
- **block_kg = (taxi+trip+contingency+alternate+reserve+extra) × fuel_factor**（fuel_factor = block 乘性修正，默认 1.00，SimBrief Fuel Factor）
- 质量演化链（单遍）：TOW → −taxi → −trip → −contingency → −alternate → −reserve → LW 校验

### C5. 重量（flightplan 域衔接）
- ZFW = DOW + payload（配载，决策 13）
- TOW = ZFW + block_kg
- LW = TOW − taxi_out − trip − contingency（落地时已耗；alternate+reserve+extra 仍在机上）
- 到达油【待确认】= block − taxi_out − trip − contingency（= alternate + reserve + extra + taxi_in；taxi_in 在进港滑行中扣减——科目语义待 grill）

### C6. Navlog / 时间链（决策 7）
- ETE_i = dist_i / GS_i；UTC_0 = EOBT + taxi_out；UTC_i = UTC_{i−1} + ETE_i
- 备降到达时刻 = 落地 UTC + 备降 ETE（单值）
- cum_nm_i = Σ dist[0..i]

### C7. 密度高度（继承 sfp 决策）
- ISA_T = 15 − 1.98 × PA/1000 (°C)；DA = PA + 120 × (OAT − ISA_T)（ft）

## 9. TODO 池

起降滑跑距离（OpenAP WRAP 地面段表）/ 成本指数（PerformanceTable schema 升级）/ W&B·CG 包线（前端）/ **FCOM 转化实现**（决策 3 架构已定）/ **表格粘贴导入** / 多备降 / MEL·ATC·WXX 附加油细分 / 精确 GS 公式档（√(TAS²−cross²)−head）。

## 9. 接口修订汇总（继承 + 本次）

继承 sfp-swift-octopus：FuelPolicyRules 四预设 + kCustom、DensityAltitudeFt、FuelDensityLbPerGal（前端换算）、ConstantPerformanceProfile（.lnmperf 解析产物）、EngineKind。
本次修订：taxi 分离（契约 1）、FlightSegment* alternate（契约 2）、Navlog 时间明细（契约 4）、引擎选择模型（决策 6，EngineKind 从"用户选择参数"改为"perf_source 派生"）。

## 10. 实施建议（Task 骨架）

1. FuelInput/FuelResult 修订落地（契约 1-6）+ 测试
2. 常量积分器 + 风修正集成（决策 2，逐点 GS/ETE）+ **过渡段修正（C2：SID 顶高查取 + VS）** + 测试
3. .lnmperf 解析器 + 导入路径（继承 Task 18/19；政策字段原样直读 + >10% 异常警告）
4. tools/ 批量转换脚本 + 内置表 + NOTICE（决策 4）
5. OpenAP 引擎衔接（继承 Task 4-13；**爬升/下降细分积分 + 过渡段积分 + 生成器网格规格**）
6. Navlog 数值产出（决策 7）→ flightplan 字段位填充联调
7. 引擎选择模型落地（决策 6：airframe.perf_source → 引擎派生；UI 双列）
8. **精度验证（P2 定稿）**：lnmperf 与 LNM 对拍（同档案同航线 block fuel <5%）；OpenAP 与 fuel.py 复现对拍（单阶段 <10% / 全阶段 <3%）+ golden 基线（固定输入→固定输出 ±3%）；**过渡段修正专项回归**（巡航近似 vs 爬升积分 38% 低估消除）
9. FCOM 转化（TODO 池，架构定案）
