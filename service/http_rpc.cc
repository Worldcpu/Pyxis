// SPDX-License-Identifier: MIT
// HTTP 传输适配实现（决策 43：全 200 + error body；传输层错误 400/404）。
#include "px/service/http_rpc.h"

#include <rapidjson/stringbuffer.h>
#include <rapidjson/writer.h>

namespace px {

namespace {

// JSON-RPC 协议级错误码（与 rpc_dispatch.cc 一致；未在头文件暴露）。
constexpr int kParseError = -32700;      // 请求文本非法 JSON
constexpr int kInvalidRequest = -32600;  // 结构合法但非请求

// 协议级错误响应体（id null——传输层错误无请求 id 可回显）。
std::string ErrorBody(int code, const char* message) {
  rapidjson::StringBuffer buffer;
  rapidjson::Writer<rapidjson::StringBuffer> writer(buffer);
  writer.StartObject();
  writer.Key("jsonrpc");
  writer.String("2.0");
  writer.Key("id");
  writer.Null();
  writer.Key("error");
  writer.StartObject();
  writer.Key("code");
  writer.Int(code);
  writer.Key("message");
  writer.String(message);
  writer.EndObject();
  writer.EndObject();
  return buffer.GetString();
}

}  // namespace

HttpRpcResponse HandleHttpRpc(
    const std::string& method, const std::string& path, const std::string& body,
    const std::unordered_map<std::string, RpcHandler>& handlers) {
  // 传输层校验（决策 43：非 POST 400、非 /rpc 404）。
  if (method != "POST") {
    return {400, ErrorBody(kInvalidRequest, "expected POST")};
  }
  if (path != "/rpc") {
    return {404, ErrorBody(kInvalidRequest, "not found")};
  }
  if (body.empty()) {
    return {400, ErrorBody(kParseError, "empty body")};
  }
  const RpcResult result = DispatchRpc(body, handlers);
  // 协议级错误（解析/无效请求）→ HTTP 400；业务错误 → HTTP 200 + error
  // body（决策 43：错误语义单通道在 JSON-RPC error code）。
  if (!result.ok && (result.error_code == kParseError ||
                     result.error_code == kInvalidRequest)) {
    return {400, result.json};
  }
  return {200, result.json};
}

}  // namespace px
