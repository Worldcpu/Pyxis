# 飞行计划模块 UI 规格（SimBrief 式设置界面）

> 状态：2026-08-07 ui grill 收敛（决策 29 修订 + 决策 32-41，见 [glossary.md](glossary.md)）。设计系统来源：ui-ux-pro-max 技能（density 8/10，variance 5，motion 3）。技术栈：React 19 + TS + Vite + Tailwind + shadcn/ui + **Leaflet（react-leaflet）** + React Router + react-i18next。相关：[design.md](design.md)、[glossary.md](glossary.md)。

## 1. 设计系统

- **双主题**（决策 33，D5 修订）：亮/暗/跟随三态，默认跟随系统，localStorage 持久化；入口 = 壳层图标栏左下角。亮色基准：底 `#F8FAFC`、主色深海军蓝 `#1E3A5F`、次要 `#2563EB`、强调 `#059669`、状态色 绿/琥珀/红（燃油/警告/错误语义）、边框 `#E4E7EB`；暗色反转（深底 `#0B1220` 系 + 亮文字，**边框/分割线独立暗色值 `#1E293B` 系**——两主题独立验证，杜绝"亮色值套暗底"），数值区强对比色在暗色下反转为深底亮字。
- **组件体系**（决策 40）：shadcn/ui 基础层（Radix 无障碍底座：Collapsible/Switch/Dialog/Tooltip）+ 密度 token 覆盖——8/10 高密度（SimBrief 式紧凑），spacing/radius 全局 token 一次配置。
- **图标**（决策 40）：全部 lucide-react——功能栏/气象六宫格（Eye/Gauge/Wind/Thermometer/Droplet/Cloud）/警告（TriangleAlert）同源，`aria-hidden` 内建。
- **字体**：Fira Sans（正文）+ Fira Code（航路串/数字/表格——等宽天然对齐代码式文本）；数字 tabular-nums。
- **i18n**（决策 40）：react-i18next，文案全部 key 化管理（第一天就写 `t('...')`），默认中文，航空术语保留英文原样（Cost Index/Load Sheet/Prefile/RVSM/METAR/TAF）；首批单语言，英文语言包 TODO。
- 无障碍：对比度 ≥4.5:1、可见焦点、标签可见、150-300ms 悬停过渡、prefers-reduced-motion。
- **单位**：协议固定 SI 基准（kg/FL/NM/kt，决策 24）；所有数据数值后附单位符号（FL226、850NM、437kt），双单位显示（巡航高度 FLxxx / xxx m）；全局单位切换（前端 O(1) 换算）待设置模块（Phase 10）。

## 2. 应用壳与布局

**壳层**（决策 32）：最左侧垂直图标栏（模块切换，React Router 路由）。Phase 9 模块 = 飞行计划（激活）+ 航图/设置灰显占位（点击提示后续阶段）；**当前模块高亮指示（背景/指示条，G6）**；FIR/雷达是地图图层不是模块。图标栏左下角 = 主题三态切换按钮。

```text
┌──┬──────────────────────────────────────────────────┐
│功│ 飞行计划模块（三列）                                │
│能│ ┌──────────────┬──────────────────────────────┐   │
│图│ │ 主任务区      │ 地图（弹性）                  │   │
│标│ │ 三子阶段替换： │ 底图 + 候选航路叠加（k 条分色） │   │
│栏│ │ ① 表单        │ 点选候选 → 高亮              │   │
│  │ │ ② 候选列表    │ 航路点点击 → 信息卡           │   │
│  │ │ ③ Flight Task │ [图层面板]（SimBrief 式）     │   │
│  │ └──────────────┴──────────────────────────────┘   │
│🌙│                                                    │
└──┴──────────────────────────────────────────────────┘
```

**响应式**（决策 32）：Tauri minWidth/minHeight 900×600；CSS 断点（长边 ~1000px）以下降级为横菜单栏 + 任务区 + 交互区纵排，降级激活时顶部横幅提醒（主要为将来 Web 部署兜底）。

**主任务区三子阶段**（决策 34）：① 生成前表单 → ② 候选列表 → ③ 生成后 Flight Task 面板；左上角返回按钮回退上一阶段。候选列表不再有地图下方结果区（决策 29 旧布局废弃）。

## 3. 生成前表单（六分区，SimBrief 式折叠组）

| 分区 | 内容 |
|---|---|
| Flight Info | 呼号（callsign，前端自持显示）、起降场、跑道、ETD（决策 10 修订） |
| Aircraft | type → variant 两级选择器（"经验"列 kLnm/kCustom，"实验性"列 kOpenAp/kFcom + 徽章）+ airframe 管理入口（Dialog） |
| Selections | 业载（pax/cargo 或 zfw 任其一，决策 13）、备降选择（plan.alternates 过滤列表，选中后 generate 请求带 `alternate` 参数，决策 14 修订）、Cost Index（默认 0，决策 5） |
| Optional Entries | 途经点/避让点/航路规则等（决策 7） |
| Fuel Planning | 燃油政策预设 + 额外油 + 滑行时间（决策 11/21；数值 Phase 10 填充，表单先立住） |
| Route | 手动航路输入（route_string）或 **SimBrief 导入**（决策 39：DOMParser 最小导入——起降场/航路串/机型；按钮常驻） |

表单模式：内联校验（airframe 校验错误显示在字段旁，**onBlur 触发 + `role="alert"`/`aria-live` 播报，G7/G8**）、渐进披露（高级设置折叠）、AUTO 联动项。

## 4. 地图与图层系统（SimBrief 式叠加）

**Leaflet**（轻量化，~42KB）：500 点量级 Canvas 渲染足够；底图随主题换源（决策 33：Carto Voyager ↔ Carto Dark）。

**图层系统（LayerManager）**——从第一天按叠加架构设计（对齐 SimBrief 地图叠加）：

| 层 | Phase | 数据 |
|---|---|---|
| 底图瓦片 | 9 | 可换源（随主题） |
| 候选航路层 | 9 | plan.routes points，全部候选同色细线 + 选中高亮（SimBrief 式列表主导，无多色区分，色盲安全） |
| 航路点/机场标注 | 9 | lookup 透传；**Phase 9 全部同色**（点类型着色跳过，决策 41） |
| 备降机场标记 | 9 | plan.alternates |
| 气象（METAR/降水雷达） | 11 | WeatherSource 扩展 |
| 地形剖面 | 11 | MORA 网格渲染（图表区） |
| 空域多边形 | 12 | OpenAIP 评估 |
| 在线网络（VATSIM/IVAO） | 12 | — |
| ETOPS 圆 | 12 | — |

契约：每层 = 独立 React 组件 + 显式开关（图层面板，SVG 图标）+ 显隐状态持久化（localStorage）；层间不互知，仅通过 LayerManager 注册。

**航路点交互**（决策 41）：**点击**（非 hover）航路点弹出信息卡——ident / 所属航路（via）/ 坐标 + 到达时间/航路点风字段位（`--` 占位，Phase 10 填充）。

## 5. 生成后视图（Flight Task 面板，决策 35，整栏纵向滚动）

自上而下（SimBrief OFP 式流式滚动，不做分区锁定）：

1. **警告条**（决策 36）：三类全收——checks.status=kUnflyable（红）/ mora_checked=false 手写航路（橙黄）/ 候选过期（橙黄）；TriangleAlert 感叹号 + 右侧 x 关闭（本次会话消失，重新生成重现）。
2. **航班任务（Flight Task）**：标题"您的航班任务"（左）+ 航班号（右靠地图侧）。动线图：起降场 ICAO 两端 + 起降时刻（**wheels 口径**：离地/触地时刻，非 block，示例 04/10Z、05/20Z）+ 灰折线 + 中部小字空中时间/距离（0840/850NM；Phase 9 `--` 占位）。性能参数网格（上标题+下数值三列居中）：备降机场 / Cost Index / 零油重 / block time / 巡航高度（FLxxx / xxx m）/ **AIRAC 周期版本**（决策 6，list_cycles 取）。航路串模块：ICAO 标准格式（ZYTL/28 CHI19D... TOS72A ZYTX/24），Fira Code 等宽 + 点间呼吸留白。
3. **舱单四栏**（OFP 子模块，决策 20）：EnrouteBurn/BlockBurn、Passengers/Baggage、Payload/ZFW、TOW/LW。数据源：Passengers/Baggage 前端自持、Payload/ZFW 现有（weights）、Burn/TOW/LW Phase 10 占位 `--`。完整 OFP 视图（调度单文本/打印）独立栏目 TODO。
4. **航务气象区**（决策 37，纯空态接口位）：起降场两块。每块：ICAO 代码 + Weather Category（VFR/MVFR/IFR 颜色标识）+ 原始数据滑块（Switch，激活显示原始 METAR/TAF）。六宫格：视距（RVR/m）、气压（QNH/Alt）、风向速（254/11kt 或 254/11MPS）、气温、露点、对流层顶高度（tropopause，Phase 11 源确认）；每格上侧简笔图标。着陆场附 TAF 原始数据（解析 TODO）。**数据全部 Phase 11 占位**。
5. **Prefile on a Network**（决策 38，默认折叠）：三行 = Network / Website / Prefile 按钮。三网络全做——VATSIM（`flightplan?raw=(FPL-...)&fuel_time=`）、IVAO（`flightPlan=` + base64 JSON）、PilotEdge（`flightplan[type]=...` 平铺 query）；Prefile = 生成预填 URL（巡航速度取自 airframe 档案，未录则提示）→ Tauri opener 跳浏览器。
6. **Flightplan Download**（默认折叠）：Format（Phase 9 仅 .PLN，多格式 TODO）+ Download 按钮（plan.export → Tauri 写盘，决策 17）。

## 6. 交互流（对齐两步式 + 三子阶段）

设置表单 → [生成候选]（plan.routes）→ 子阶段②候选列表 + 地图画线 → 点选（route_string 入状态）→ 补全性能设置 → [生成计划]（plan.generate，请求带 alternate/CI）→ 子阶段③Flight Task 面板 + 警告条 → [Download .PLN] / [Prefile] / [导出调度单 TODO]。返回按钮回退子阶段。候选过期提示（mora_checked/altitude 一致性由决策 25/26 保证；参数改后候选过期 → 警告条橙黄）。

**加载与错误反馈（review 修订 G1/G3）**：所有 plan.* 请求（routes/generate/alternates/export）期间触发按钮转 loading + 禁用防双击（不做百分比进度条，决策 18 语义保持）；请求失败 → 面板顶部错误条（红，与警告条同层不同色），RPC 错误码映射 i18n 中文消息（400 参数错 / 404 查无 / 422 无解 / -32000 内部 / -32601 方法不存在）；WS 断线 → 顶部横幅 + 自动重连提示。

## 7. 决策记录

- D5（修订）：双主题必须，三态切换 + 跟随默认（决策 33）。
- D6：Leaflet + react-leaflet（轻量化；MapLibre 200KB+ 否决）。
- D7：图层系统从 Phase 9 起按叠加架构设计（LayerManager + 开关面板），气象/空域等后续层即插即用。
- D8：壳层架构——图标栏 + React Router 模块路由，航图/设置占位（决策 32）。
- D9：主任务区三子阶段替换，无下方结果区（决策 34）。
- D10：shadcn/ui + 密度 token；lucide 图标；react-i18next（决策 40）。
- D11：生成后视图规格——wheels 口径动线图、AIRAC 版本显示、舱单 OFP 子模块、气象空态、Prefile 三网络、警告条三类（决策 35-38）。
- D12：SimBrief 最小导入（前端 DOMParser，决策 39）。
- D13：响应式——Tauri min 900×600 + CSS 断点 + 横幅提醒（决策 32）。
- D14：航路点点击信息卡 + Phase 9 同色（决策 41）。
- D15（review 修订，决策 42）：加载反馈（按钮 loading 防双击）+ 错误条/WS 断线横幅（G1/G3）；候选视图 SimBrief 式——列表主导含 seed、地图同色叠加选中高亮（G2）；暗色边框/分割线独立 token（G5）；图标栏活动模块高亮（G6）。
