# ADR-0002：raw 查询薄桥接 bf::service

- 状态：已接受（2026-08-06，grill 决策 15）
- 相关：[glossary.md](../glossary.md) 决策 15、[design.md](../design.md) §3/§8

## Context

bf::service（MIT）提供 9 个传输无关 JSON-args handler（`HandlerResult{body, status}`），JSON 键名由 api_keys.h 单源化、测试完备、与 bf 自己的 HTTP/MCP 前端同源。Pyxis 的 px_service 已有 JsonModule 框架 + RouteJsonModule（Phase 7 实现）。查询分两类：raw 查询（机场/航路点/程序/航路浏览器——无 px 特有语义）与 plan 流程（px 专属组合流程）。bf::service 的类型化入口返回渲染串而非结构化对象，plan 流程拿结构化数据只能调 engine 层 NavDatabase。

## Decision

**薄桥接**：raw 查询（lookup_waypoints/airports/procedures/procedure_legs/airways/navaid_detail/holds/parse_route/list_cycles）由 px_server 透传 bf::service handler（JSON 形状 bf 定）；plan 流程（routes/generate/alternates/export）px 直调 NavDatabase 按 px 形状渲染。**RouteJsonModule 退役**（其单条 Route 投影形状无消费者），JsonModule 注册表保留给 px 域模块（flightplan/fuel）。构建补 libs/http_server 检出（bf::http_status 头）+ 两个 add_subdirectory；bf_service_lib 只被 px_service 链接。

## Consequences

- 正向：raw 查询零渲染代码、api_keys 单源防漂移、错误语义（400/404/422）与 bf 一致；px 形状控制权保留在 plan 流程。
- 负向：raw 查询形状失去 px 控制（改字段需动 bf 侧）；bf_service_lib 成为构建依赖（MIT 无义务）。
- 权衡：若未来某 raw 查询需要 px 定制形状，可单点降级为 px 渲染（混合路径），不影响整体。
