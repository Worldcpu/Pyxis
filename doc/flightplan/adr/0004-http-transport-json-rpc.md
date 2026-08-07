# ADR-0004：传输层 = HTTP/1.1 + JSON-RPC over POST

- 状态：已接受（2026-08-07，T1/T5 grill 决策 43）
- 相关：[glossary.md](../glossary.md) 决策 16/43、[design.md](../design.md) §3/§5/§8

## Context

设计初期定 px_server 传输层为 **WebSocket** + JSON-RPC 2.0（决策 16，对齐 VS Code LSP / MCP 行业惯例，Tauri CSP 也已放行 `ws://127.0.0.1:*`）。T1 实施摸底时发现事实约束：`bf_http_server`（计划复用的 libuv+llhttp 服务器）是**纯 HTTP/1.1**——无 upgrade/101 处理，唯一流式机制是 SSE（`BeginStream`/`WriteEvent`）。要忠实 WS 决策需自建 RFC6455 握手 + 帧编解码（~400 行），且无法复用 bf 的 Server/Connection/QueueWork（线程池 offload）全栈；或改 bf 子模块加 upgrade 支持（违背"fork 可干净更新"约束）。

替代方案对比：① 自建 WS（决策原样）——语义双工，成本 ~400 行 + 线程模型自研 + 测试面扩大；② HTTP/1.1 + POST（bf 全栈复用）——请求-响应语义足够（generate 秒级同步等待，决策 18 无进度通知），未来推送可轮询或 SSE；③ HTTP + SSE 混合——当前无推送需求，预建成本无收益。

## Decision

**HTTP/1.1 + JSON-RPC 2.0 over POST**（glossary 决策 43 修订决策 16）：

- 单路径 `/rpc`，Content-Type `application/json`；非 POST / body 非 JSON → HTTP 400。
- **全部 RPC 响应（含错误）HTTP 200**——错误语义全在 body 的 JSON-RPC `error` 对象（error code 分区不变：-32000/422/404/400/-32600/-32601/-32700）；前端 fetch 只查 body，与 ui-spec 决策 42 的"RPC 错误码 → i18n 中文消息"映射单通道契合。
- 响应带 `Access-Control-Allow-Origin`（WebView 跨源 fetch）；Tauri CSP `connect-src` 补 `http://127.0.0.1:*`。
- 重 handler 经 bf `QueueWork` offload libuv 线程池（决策 44），loop 线程只做传输与分派。

## Consequences

- 正向：复用 bf_http_server 验证过的 Server/Connection/Limits（1MiB/30s 超时）+ QueueWork 线程池；前端 `fetch` 原生支持，无需 WS 客户端库；JSON-RPC 错误码单通道，无 HTTP status/body 双码混乱。
- 负向：无服务端主动推送——Phase 11 气象更新需前端轮询或 SSE（`BeginStream` 现成，代价可控）；决策 16 "WS" 字样已修订。
- 权衡：当前产品形态（桌面单用户、同步请求-响应）无双工需求；未来若需推送优先补 SSE，仍不够再加 WS（届时 px 传输层独立于 bf，可自由演进）。
