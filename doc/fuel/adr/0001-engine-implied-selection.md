# ADR-0001：引擎隐含选择模型（airframe 驱动）

- 状态：已接受（2026-08-06，决策 6）
- 相关：[glossary.md](../glossary.md) 决策 6、[design.md](../design.md) §2

## Context

双引擎（标准 lnmperf / 实验性 OpenAP）下，初版设计是"全局引擎选择器 + 每次生成可覆盖"——用户要先理解两种引擎区别再选。但机型档案本身携带性能来源（perf_source），且两引擎的机型档案集不同（lnmperf 预置几百架 vs OpenAP 15 架）——显式引擎选择制造"选机型还要选引擎"的重复认知负担。

## Decision

**引擎由机型档案 perf_source 隐含确定**：kLnm/kCustom → 标准引擎；kOpenAp/kFcom → 实验性引擎。UI 呈现 = SimBrief 式：左选机型（type），右侧 airframe variant **双列竖滑列表**（"经验"列 / "实验性"列，experimental 徽章）——用户看到的始终是"选机型"，引擎概念不暴露。协议 JSON 仍带内部 engine 标识 + experimental 字段位。EngineKind 从"用户选择参数"改为"perf_source 派生值"。

## Consequences

- 正向：用户零引擎认知负担；档案集差异自然呈现（每列数量即覆盖面）；协议层字段位不变（flightplan 决策 21 兼容）。
- 负向：同机型双档案时 variant 列表出现近似重复项（A320 标准 / A320 实验性）——用户需理解列语义；无"全局默认引擎"概念（每档案自带归属）。
- 权衡：显式选择器被否决——其"高级用户精确控制"价值低于"默认用户不被迷惑"；未来若需要强制引擎（如验证场景），可在请求参数加 engine 覆盖位（协议兼容扩展）。
