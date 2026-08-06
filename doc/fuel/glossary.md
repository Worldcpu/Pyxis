# 燃油系统领域词汇表

> 本文件是燃油系统（doc/fuel/）的领域词汇表，随 grill 会话实时维护。只收领域术语与既定决策。基线：~/.claude/plans/sfp-swift-octopus.md（上一轮 27 决策）+ [flightplan 29 决策](../flightplan/glossary.md)（兼容约束）。相关设计输入：[design.md](../flightplan/design.md) §9。

## 术语

| 术语 | 英文 | 定义 |
|---|---|---|
| 燃油阶梯 | fuel ladder | taxi → trip → contingency → alternate → final reserve → extra → block（SimBrief 式政策分解） |
| 政策预设 | FuelPolicyRules | kIcao5Pct / kEuOpsMax（max(5%,5min)）/ kFaa121（45min 巡航）/ kFromProfile（.lnmperf 兼容）/ kCustom |
| 三阶段积分 | three-phase integration | OpenAP 引擎：climb/cruise/descent 逐段积分（查表插值） |
| 常量模型 | constant profile | lnmperf 引擎：每阶段固定流率/速度/垂直速度，纯乘法 |
| 高空风修正 | wind correction | 风 → GS → 段时间 → 段油耗；常量流率模型下风修正 = 地速修正 |
| 备降段 | alternate segment | flightplan 域 FlightSegment（kAlternate），备降距离默认 DCT |
| FCOM 转化 | FCOM conversion | 航司运营手册性能数据 → OpenAP PTF 表同 schema，复用 OpenAP 引擎 |

## 已定决策

| # | 决策 |
|---|---|
| 1 | 设计形态：输出全新文档体系到 doc/fuel/（design/glossary/ADR），基线 = sfp-swift-octopus 27 决策 + flightplan 29 决策，本次迭代 FCOM 转化 / 高空风修正 / 兼容修订，标注迭代记录。旧 fuel-engine.md 已丢失不复活。 |
| 2 | 高空风修正 = **GS ≈ TAS − head（近似式，crosswind 忽略，<1% 误差）** + **逐航路点分段风**（WeatherSource 批量 ≤80 点，与 Navlog 字段位对齐）+ **全阶段修正**（climb/descent 段内代表点）。trip 油联动 contingency；备降段（DCT）取中点风；final reserve 时间基准不受风。 |
| 3 | FCOM 转化架构已确认（**实现留 TODO**）：FCOM 数据 → 转化器（字段表驱动校验 + 单位统一 SI）→ **OpenAP PTF 表同 schema**（复用三阶段积分器，不建第三引擎）→ airframe 档案指向（provenance 标 source: fcom + 机型 + 数据日期）。录入方式：表单先行，表格粘贴导入 TODO。 |
| 4 | **lnmperf 许可 = 公共领域认定（b）**：数据是事实性性能参数（速度/油耗数值，无创造性表达），公共领域成立；**Task 21（内置表）解阻塞**；仍保留 NOTICE 来源署名纪律（来源 littlenavmap.org，内置"转换后 JSON + NOTICE"）。 |
| 5 | **flightplan 兼容契约（6 项全确认）**：① taxi_out_min/taxi_in_min（默认 20/8，出港进 block、进港到到达油科目）② alternate = flightplan 域 FlightSegment（DCT 距离 + 中点风）③ WeatherSource 批量采样 ≤80 点 + 降级链标注 ④ **Navlog 数值产出**（FuelResult 带逐段时间明细：TAS/GS/ETE/逐点风 → 填充 flightplan 决策 22 字段位）⑤ 协议 SI 公制基准（kg/FL/NM/kt），单位换算前端职责 ⑥ 引擎选择 + experimental 标志字段位。 |
| 6 | **引擎选择模型重构**：引擎由机型档案 perf_source 隐含确定（kLnm/kCustom → 标准；kOpenAp/kFcom → 实验性），**UI 无显式引擎选择器**。呈现 = SimBrief 式：左选机型（type），右侧 airframe variant **双列竖滑列表**——第一列"经验"（lnmperf 预置/导入，默认多），第二列"实验性"（OpenAP 表，experimental 徽章）；选定后调度单 meta 显示"性能模型：标准/实验性"（知情不繁复）。协议 JSON 仍带内部 engine 标识 + experimental 字段位（决策 21 不变）。 |
| 7 | **Navlog 数值产出**（SimBrief 思路）：主航路**逐点**产出 cum_nm/wind/gs/ete/utc——ETE = 距离/GS（GS = 阶段常量 TAS − head）；**UTC 链含 taxi_out 偏移**（起飞点 = EOBT + taxi_out，此后逐点累加）；**备降段单值到达时刻**（DCT 单段无点序列，航司惯例不展开备降 Navlog；供 Phase 11 备降场日出日落检查与 ETA 显示；不参与 final reserve 计算）。TAS：每阶段常量（lnmperf 语义），OpenAP 引擎可填真实演化值（字段位不设限）。 |
| 8 | **迭代修订**：① airframe 档案补 **`perf_source` 字段**（kLnm/kOpenAp/kCustom/kFcom）——flightplan 决策 21 的跨模块修订点，引擎隐含模型的数据基础；② ui-spec §4 机型选择器改**双列竖滑列表**（经验/实验性列），替代原"两级下拉"；③ 积分器**单遍**（每段用段首质量查表，不做质量-风迭代收敛；迭代仅成本指数场景有意义，TODO）。 |
| 9 | **切分与风修正修订（L1/L2 定稿）**：① 三阶段切分 = **程序衔接点**——爬升段 [起飞, SID 出口 fix]、巡航段 [SID 出口, STAR 入口]（enroute 全部）、下降段 [STAR 入口, 落地]（STAR+approach）；无 SID/STAR → enroute 首/尾点（DCT 连接段/IAF）；FlightSegment kind 即边界；**纯程序切分**（过渡爬升段用 Cruise 流率，~200kg 低估接受）② **VS 闲置**（不参与积分）③ **全段随风**（L1 推翻：时间 = 地面距离/GS，程序固定几何）④ **程序段端点平均风**：SID = (机场地面风 + SID 出口巡航层风)/2，STAR 对称；巡航段逐航路点分段风不变 ⑤ 边界：巡航层<场高报错 400；巡航层<SID 顶高接受；GS 钳制下限 50kt + 警告（防除零/负时间）。 |
| 10 | **OpenAP 轮定稿（O0-O2，SimBrief 对齐）**：① 切分继承程序衔接点；段内单值查表（爬升/下降 = 段首重量×段代表高度 ≈FL100），巡航段逐航路点分段查表（风点即查表点）② **双线性插值**（重量×高度）；**表外报错**（400，指明维度 + 低巡航层提示切标准引擎，不钳制）；VS 保留（剖面图用）③ 质量演化链单遍链式无环：TOW→−taxi→−爬升→−巡航逐点→−下降→−备降→−reserve→LW ④ **政策流率两引擎口径分离**：OpenAP = **备降场上空 1500ft AGL 体系**（SimBrief 对齐，替换旧决策 (arr_fl+15) FL）；lnmperf = 档案 Cruise 常量流率/直接值（kFromProfile）⑤ **fuel_factor = block 乘性修正**（默认 1.00）⑥ PTF 表 kg/h（SI 基准）⑦ **备降段查表高度（hold_fl 默认 FL150）≠ reserve 等待高度（1500ft AGL）**——两高度分离。 |
| 11 | **审议轮修订（R1-R3）**：① lnmperf 等待流率 = 档案 Cruise 常量/直接值；备降段查表高度（hold_fl 默认 FL150）与 reserve 等待高度（1500ft AGL）分离写清 ② 程序段端点风高度 = 程序代表高度（SID 出口/STAR 入口 ≈FL150 层）；**Navlog 点风 = 计算用风**（SID/STAR 内点 = 端点平均、巡航点 = 采样风，显示与计算同源）③ 表外报错指明维度 + 低巡航层提示切标准引擎。 |
| 12 | **P1 终版（数据验证定稿）**：① **过渡段修正确认**——SID 顶高（ProcedureLeg.alt1_ft，kAtOrAbove/kBetween 上界，需打通 CifpData→Procedure→legs 查取路径）→ 巡航层：lnmperf = VS 常量（Climb 2280/Descent 1436 fpm 实测可用）时间锁定不随风；OpenAP = 爬升表积分（ROC 查表）；kNone 约束回退（过渡高度 10000ft 或跳过）；备降段纯常量（Alternate 无 VS）② **实测低估修正**：Climb = 2.26× Cruise（lnmperf 实测 ~450kg/次）、OpenAP 巡航近似 vs 爬升积分 38%（358kg）——"~200kg 接受"表述作废 ③ **OpenAP 爬升/下降段细分积分**（高度步进 ~1000ft/FL；单值查表 ±10-15% 超单阶段 <10% 容限；必须用 climb 表带 γ，平飞流率低估 30%+ 禁止）④ 备降/等待 1500ft AGL < FL30 表外钳制最低行 ⑤ 生成器网格：FL0/30 起步、≤2000ft、**CAS→Mach 过渡高度强制格点**（VS 不连续 -37%）、climb 三质量流率扩展 ⑥ **C1：政策字段原样直读 + >10% 异常值 UI 警告**（Fenix 档案 Contingency 30% 实测）⑦ **C2：P2 验证入 Task 8**（LNM 对拍 <5% / fuel.py 对拍阶段 <10% 全 <3% / golden ±3% / 过渡修正专项回归）。 |
| 13 | **互锁缺口定稿（flightplan × fuel）**：① **FlightSegment 补坐标 + 高度**：from/to 坐标（航迹角可导，风投影用）+ 程序段 top_fl（SID 顶高；取值规则：kBetween/kAt/kAtOrBelow → alt1 上界；**kAtOrAbove/kNone → 顶高未知 → 回退过渡高度 10000ft 或跳过过渡修正**）② **fuel_factor 归 airframe 档案**（机型属性非航班属性，generate 不暴露）③ **TOW-ZFW-block 两遍封顶**：lnmperf 无循环（常量流率不依赖 TOW，TOW 仅校验用）；OpenAP = TOW_start=ZFW×1.15 → 积分 → block₁ → |TOW₁−TOW_start|/TOW_start>3% 则用 TOW₁ 重积分（封顶 2 遍）→ TOW₂ 校验，误差 <0.1% ④ **点序列补 segment_index**（段归属：起飞机场点 = 爬升段 0 且 cum=0/ete=0/utc=EOBT+taxi_out；落地机场点 = 下降段；程序内点均有归属）⑤ **采样多层**：≤85 坐标（80 航路点+2 机场+2 程序端点+**备降中点**（备降存在时，haversine 大圆中点，高度层 = hold_fl 默认 FL150））× 多层变量（surface+850hPa≈FL150+巡航层）单请求（Open-Meteo 等压面一次可带多层）；坐标上限实测 <85 → 分批（每批 ≤50）合并；频率超限走降级链。 |
| 14 | **迭代审议定稿（A/B/C）**：**A 备降中点采样**（决策 13⑤ 修订，85 坐标，修复备降 GS 无风降级缺口）**B transition 顶高查取路径**：route.sid（含 transition 全名，如 GURUN-9D.TOWIN）→ LookupProcedureLegs → 末段定点 leg alt1_ft；STAR 对称（入口 IF leg alt 约束）；route.sid 空 → 跳过过渡修正（默认 10000ft）**C flightplan 测试策略六面**（见 [flightplan glossary 决策 31](../Flightplan/glossary.md)：FromBf 转换/配载/altitude 规则层/JSON golden/协议集成/seed 确定性）。 |
