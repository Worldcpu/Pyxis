# Domain Docs

工程技能在探索代码库时如何消费本仓库的领域文档。

## 探索前先读这些

- **`doc/<module>/design.md`** — 模块设计文档（如 `doc/flightplan/design.md`）
- **`doc/<module>/glossary.md`** — 模块术语表（如 `doc/flightplan/glossary.md`）
- **`doc/<module>/adr/`** — 模块级架构决策记录（如 `doc/flightplan/adr/`）

按任务涉及的模块读取对应文件夹。仓库根目录没有 `CONTEXT.md` —— `doc/` 下的模块文件夹就是领域文档。

如果这些文件不存在，**静默继续**。不要标注缺失，也不要主动建议提前创建。`/domain-modeling` 技能（通过 `/grill-with-docs` 和 `/improve-codebase-architecture` 触达）会在术语或决策真正落地时惰性创建它们。

## 文件结构

```
doc/
├── flightplan/          ← 飞行计划模块
│   ├── design.md        ← 模块设计
│   ├── glossary.md      ← 术语表
│   ├── ui-spec.md
│   └── adr/             ← 模块级 ADR
└── fuel/
    ├── design.md
    ├── glossary.md
    └── adr/
```

## 使用术语表的词汇

当你的产出命名一个领域概念时（issue 标题、重构提案、假设、测试名），使用对应模块 `glossary.md` 中定义的术语。不要漂移到术语表明确回避的同义词。

如果需要的概念不在术语表中，这是一个信号 —— 要么你在发明项目不使用的语言（重新考虑），要么存在真实缺口（记下来交给 `/domain-modeling`）。

## 标注 ADR 冲突

如果你的产出与现有 ADR 矛盾，显式指出而不是静默覆盖：

> _与 ADR-0007（event-sourced orders）矛盾 —— 但值得重新讨论，因为……_
