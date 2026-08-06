// SPDX-License-Identifier: MIT
#pragma once

#include <rapidjson/document.h>

#include <functional>
#include <string>
#include <unordered_map>

#include "px/service/rpc_dispatch.h"

namespace bf {
class NavDatabase;  // 仅前向声明——头文件不暴露 bf 类型（分层纪律）
}  // namespace bf

namespace px {

// bf::service::HandlerResult(status, body) → px::RpcResult 薄转换
// （决策 15/18：status ≥ 400 为错误；错误文本提取自 bf JsonError 的
// "error" 字段，失败则回退 body 原样/兜底文本）。纯函数，transport 无关。
RpcResult FromBfHandlerResult(int status, const std::string& body);

// 透传 bf::service 全部 NamedHandler 为 px RpcHandler（find_routes 除外——
// 两步式 API 的 plan.routes 由 T5 直接调用引擎，不暴露为 RPC）。db 引用
// 在每次调用时透传给 bf handler。
std::unordered_map<std::string, RpcHandler> MakeBfHandlers(bf::NavDatabase& db);

}  // namespace px
