// SPDX-License-Identifier: MIT
// HTTP 传输适配实现（决策 43：全 200 + error body；传输层错误 400/404）。
#include "px/service/http_rpc.h"

namespace px {

HttpRpcResponse HandleHttpRpc(
    const std::string& method, const std::string& path, const std::string& body,
    const std::unordered_map<std::string, RpcHandler>& handlers) {
  // 传输层校验（决策 43：非 POST 400、非 /rpc 404）。协议错误体共用
  // rpc_dispatch 的 BuildError（单源——审查修复）。
  if (method != "POST") {
    return {400, BuildError(kRpcInvalidRequest, "expected POST", nullptr)};
  }
  if (path != "/rpc") {
    return {404, BuildError(kRpcInvalidRequest, "not found", nullptr)};
  }
  if (body.empty()) {
    return {400, BuildError(kRpcParseError, "empty body", nullptr)};
  }
  const RpcResult result = DispatchRpc(body, handlers);
  // 协议级错误（解析/无效请求）→ HTTP 400；业务错误 → HTTP 200 + error
  // body（决策 43：错误语义单通道在 JSON-RPC error code）。
  if (!result.ok && (result.error_code == kRpcParseError ||
                     result.error_code == kRpcInvalidRequest)) {
    return {400, result.json};
  }
  return {200, result.json};
}

}  // namespace px
