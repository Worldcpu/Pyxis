// SPDX-License-Identifier: MIT
#pragma once

#include <rapidjson/document.h>

#include <functional>
#include <string>
#include <unordered_map>

namespace px {

// JSON-RPC 2.0 分派结果（决策 18：错误码分区——-32700 解析错 / -32600
// 无效请求 / -32601 未知方法 / -32000 内部；业务错误透传 bf 风格码
// 400/404/422）。
struct RpcResult {
  bool ok = false;  // true = result 响应；false = error 响应
  std::string json;  // 完整 JSON-RPC 响应（含 id 回显）；通知时为空串
  int error_code = 0;  // ok=false 时有效
  std::string error_message;  // 业务错误的人类可读文本（决策 18：码+消息）
};

// 单方法 handler：收 params（rapidjson::Value），返回 RpcResult（ok=true
// 的 json 为 result 内容，ok=false 的 error_code 透传为 JSON-RPC error、
// error_message 进入 error.message）。
using RpcHandler = std::function<RpcResult(const rapidjson::Value& params)>;

// 解析 JSON-RPC 请求并分派（transport 无关——px_server WS 层直接调用）。
// 支持单请求与 batch 数组（含通知——无 id 请求不产生响应，json 为空串）；
// 结构合法但非请求（字符串/数字/空 batch）→ -32600，仅非法 JSON → -32700。
RpcResult DispatchRpc(
    const std::string& request_json,
    const std::unordered_map<std::string, RpcHandler>& handlers);

}  // namespace px
