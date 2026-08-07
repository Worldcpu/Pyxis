// SPDX-License-Identifier: MIT
// HTTP 传输适配纯函数（决策 43/ADR-0004：单路径 POST /rpc；
// 全部 RPC 响应（含错误）HTTP 200，错误语义在 body JSON-RPC error 对象；
// 传输层错误——非 POST / 非 /rpc / body 非 JSON——用 HTTP 状态码）。
#pragma once

#include <string>
#include <unordered_map>

#include "px/service/rpc_dispatch.h"

namespace px {

// HTTP 层响应（bf::http_server 适配在 px_server 内完成；本函数
// transport 无关，便于单测）。
struct HttpRpcResponse {
  int status = 200;
  std::string body;  // JSON-RPC 响应文本；通知（无 id）时为空串
};

// HTTP 请求 → HTTP 响应。业务 handler 在调用线程执行——重 handler
// （plan.*/airframe 写）由调用方 QueueWork offload（决策 44）。
HttpRpcResponse HandleHttpRpc(
    const std::string& method, const std::string& path, const std::string& body,
    const std::unordered_map<std::string, RpcHandler>& handlers);

}  // namespace px
