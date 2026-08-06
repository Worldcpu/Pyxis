# 飞行计划模块 UI 规格（SimBrief 式设置界面）

> 状态：grill 收敛（2026-08-06，决策 29）。设计系统来源：ui-ux-pro-max 技能（density 8/10，variance 5，motion 3）。技术栈：React 19 + TS + Vite + Tailwind + **Leaflet（react-leaflet）**。相关：[design.md](design.md)、[glossary.md](glossary.md)。

## 1. 设计系统

- **模式：浅色为主，暗色为例外**（Tailwind dark 变量，暗色后续可选）。浅色基准：底 `#F8FAFC`、主色深海军蓝 `#1E3A5F`、次要 `#2563EB`、强调 `#059669`、状态色 绿/琥珀/红（燃油/警告/错误语义）、边框 `#E4E7EB`。
- **字体**：Fira Sans（正文）+ Fira Code（航路串/数字/表格——等宽天然对齐代码式文本）；数字 tabular-nums。
- **密度**：8/10 高密度（SimBrief 式紧凑表单）；表单分区折叠 + 渐进披露（高级设置折叠）。
- 无障碍：对比度 ≥4.5:1、可见焦点、标签可见、150-300ms 悬停过渡、prefers-reduced-motion。

## 2. 布局（单页三区）

```text
┌──────────────┬────────────────────────────────────┐
│ 设置面板（左 360px 可折叠） │       地图（中，弹性）           │
│  └ 航班：呼号/起降/跑道/ETD │  底图 + 候选航路叠加（k 条分色）  │
│  └ 机型：type→variant 两级  │  点选候选 → 高亮                │
│  └ 业载：旅客/货物/ZFW      │  备降机场标记                    │
│  └ 燃油政策：预设下拉+额外油 │  [图层开关面板]（SimBrief 式）    │
│  └ 巡航：auto/手动 FL+规则  │  ┌───────────────────┐          │
│  └ 备降：建议/手动+过滤参数  │  │ 结果区（下，可折叠）│          │
│  └ 全局：airframe 管理/设置  │  │ 候选列表→调度单    │          │
└──────────────┴────────────────────────────────────┘
   [生成候选] → 选候选 → [生成飞行计划] → 调度单渲染 → [导出 .PLN]
```

## 3. 地图与图层系统（SimBrief 式叠加）

**Leaflet（轻量化，~42KB）**：500 点量级 Canvas 渲染足够；浅色底图（Carto Voyager/OSM），暗色需求将来换瓦片源即可。

**图层系统（LayerManager）**——从第一天按叠加架构设计（对齐 SimBrief 地图叠加）：

| 层 | Phase | 数据 |
|---|---|---|
| 底图瓦片 | 9 | 可换源（浅色默认） |
| 候选航路层 | 9 | plan.routes points，k 条分色，选中高亮 |
| 航路点/机场标注 | 9 | lookup 透传 |
| 备降机场标记 | 9 | plan.alternates |
| 气象（METAR/降水雷达） | 11 | WeatherSource 扩展 |
| 地形剖面 | 11 | MORA 网格渲染（图表区） |
| 空域多边形 | 12 | OpenAIP 评估 |
| 在线网络（VATSIM/IVAO） | 12 | — |
| ETOPS 圆 | 12 | — |

契约：每层 = 独立 React 组件 + 显式开关（图层面板，SVG 图标）+ 显隐状态持久化（localStorage）；层间不互知，仅通过 LayerManager 注册。

## 4. 表单模式

- SimBrief 式分区折叠组：可见标签、**内联校验**（airframe 校验错误显示在字段旁）、渐进披露、AUTO 联动项（业载四联动简化版：给 pax/cargo 或 zfw 任其一）、单位切换（全局，前端 O(1) 换算）。
- **机型选择器 = 左机型（type）→ 右侧 airframe variant 双列竖滑列表**（"经验"列 = kLnm/kCustom 档案，"实验性"列 = kOpenAp/kFcom 档案 + experimental 徽章）——修订于 2026-08-06 fuel 会话（原"两级下拉"废弃，见 [fuel glossary 决策 6/8](../fuel/glossary.md)）；选定后调度单 meta 显示"性能模型：标准/实验性"。
- 字段 → 请求映射：以 glossary 决策 7/23/24/25/27 为准；UI 字段 = 请求字段超集。

## 5. OFP 渲染

区块卡片流（基本信息→高度三元组→航路→备降→配载→燃油阶梯→重量→检查结果），等宽数字列，状态色徽章；调度单文本/打印（window.print）由前端从 JSON 渲染（决策 14）。

## 6. 交互流（对齐两步式）

设置 → [生成候选]（plan.routes）→ 地图画线 + 候选列表 → 点选（route_string 入剪贴板/状态）→ 补全性能设置 → [生成计划]（plan.generate）→ 调度单 + [导出 .PLN]（plan.export → blob/写盘）。候选过期提示（mora_checked/altitude 一致性由决策 25/26 保证）。

## 7. 决策记录

- D5：浅色为主 + 暗色例外。
- D6：Leaflet + react-leaflet（轻量化；MapLibre 200KB+ 否决）。
- D7：图层系统从 Phase 9 起按叠加架构设计（LayerManager + 开关面板），气象/空域等后续层即插即用。
