# ADR-0002：lnmperf 预置表公共领域认定

- 状态：已接受（2026-08-06，决策 4）
- 相关：[glossary.md](../glossary.md) 决策 4、[design.md](../design.md) §7

## Context

上一轮设计（sfp-swift-octopus）判断 .lnmperf 预置表"无明确许可（created by me or the community, provided as is）→ Task 21 内置表阻塞，等作者授权"。本轮用户明确表述"lnmperf 性能数据多，**公共领域数据**"。LNM 预置表内容为事实性性能参数（每阶段油耗率/速度/垂直速度数值），无创造性表达。

## Decision

**公共领域认定成立**：事实数据（数值参数）不受版权保护（Feist 标准）；lnmperf 预置表作为事实性性能参数属公共领域，**Task 21（内置表）解阻塞**。内置形态 = 转换后 JSON（tools/ 脚本批量转换，一次性构建步骤）+ **NOTICE 来源署名纪律保留**（来源 littlenavmap.org，随 data/fuel/lnm/ 附带）。用户导入路径永久保留（运行时 service 解析器实时转，互操作零风险）。

## Consequences

- 正向：默认引擎（标准/lnmperf）覆盖缺口解除——内置几百架预置表，无导入前置；NOTICE 保留来源透明度。
- 负向：若数据实际含作者创造性编排（非纯事实），认定有被挑战风险——NOTICE 署名 + 保留导入路径是缓冲。
- 权衡：内置分发 vs 仅导入——公共领域认定下内置价值（开箱即用）大于法律保守代价；若未来出现版权主张，可回退为仅导入（架构不破坏）。
