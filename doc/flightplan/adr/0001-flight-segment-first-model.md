# ADR-0001：航段一等领域模型

- 状态：已接受（2026-08-06，grill 决策 2/2b/2c）
- 相关：[glossary.md](../glossary.md) 决策 2-5、[design.md](../design.md) §2

## Context

飞行计划的领域模型有三个候选：LNM 式航路点序列模型（`QList<FlightplanEntry>`，每点挂 airway/过程名字符串，leg 由邻点推导）、航段一等模型（FlightSegment 为真源）、双视图。同时 FuelEngine 契约已引用 `const FlightSegment* alternate`（备降段），即燃油引擎以"段"为输入。设计宪法要求领域类型为不可变值类型，且计划编辑发生在请求层（重新生成）而非原地修改。

## Decision

采用**航段一等模型**：`FlightSegment` 为独立不可变值类型（不复用 bf::RouteLeg，经 FromBf() 一次性转换），`FlightPlan` 聚合根持有段序列（真源）+ 完整点序列视图（派生，含程序内点，供 navlog/地图）。段粒度 = 程序聚合段（SID/STAR/进近各 1 段，引用程序名；enroute 逐航路段；DCT 单独段；备降整体 1 段）。**无 taxi 段**（滑行时间用户设置，燃油政策处理）。SegmentKind = kSid/kEnroute/kStar/kApproach/kAlternate。

## Consequences

- 正向：FuelEngine 直接消费段；OFP/Navlog 逐段输出自然；bf::Route 的 legs 1:1 映射零损耗；px API 面不暴露 bf 类型。
- 负向：点序列需从 bf::Route.points 独立转换（一个函数的事）；与 LNM 序列模型的"编辑直观"无缘——但不可变模型下编辑本就不发生。
- 权衡：若未来需要原地编辑航路（地图拖拽改点），需在请求层实现"改点 → 重搜索"路径，而非给领域类型加可变性。
