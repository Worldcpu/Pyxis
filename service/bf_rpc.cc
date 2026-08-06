// SPDX-License-Identifier: MIT
// bf::service 薄桥接（决策 15：bf handler 原样透传，px 只做状态/错误翻译）。
#include "px/service/bf_rpc.h"

#include <rapidjson/document.h>

#include <string>

#include "handlers.h"
#include "io/nav_database.h"

namespace px {

RpcResult FromBfHandlerResult(int status, const std::string& body) {
  if (status < bf::service::kErrorStatusThreshold) {
    return {true, body, 0, ""};
  }
  // 错误文本：bf handler 的错误 body 是 JsonError（{"error":"..."}，
  // RapidJSON Writer 自动转义）——提取 message；解析失败回退原样。
  std::string message = "handler error";
  if (!body.empty()) {
    rapidjson::Document doc;
    if (!doc.Parse(body.c_str()).HasParseError() && doc.IsObject() &&
        doc.HasMember("error") && doc["error"].IsString()) {
      message = doc["error"].GetString();
    } else {
      message = body;
    }
  }
  return {false, "", status, message};
}

std::unordered_map<std::string, RpcHandler> MakeBfHandlers(
    bf::NavDatabase& db) {
  std::unordered_map<std::string, RpcHandler> handlers;
  for (const auto& named : bf::service::MakeHandlers()) {
    // find_routes 不暴露为 RPC：两步式 API（决策 6）的 plan.routes 由
    // px 层直接调用引擎（T5 接线），避免双路径契约漂移。
    if (named.name == "find_routes") continue;
    handlers.emplace(named.name, [&db, named](const rapidjson::Value& params) {
      const auto result = named.handler(params, db);
      return FromBfHandlerResult(result.status, result.body);
    });
  }
  return handlers;
}

}  // namespace px
