// SPDX-License-Identifier: MIT
// plan 流程 handler（决策 15：px 直调 NavDatabase 按 px 形状渲染；
// 决策 16：17 端点全集——plan 四 + airframe 四 + list_cycles +
// MakeBfHandlers 透传；find_routes 不暴露）。
#pragma once

#include <rapidjson/document.h>

#include <memory>
#include <mutex>
#include <string>
#include <unordered_map>

#include "px/module/navdata/airport_index.h"
#include "px/service/rpc_dispatch.h"

namespace bf {
class NavDatabase;
}

namespace px {

// plan handler 上下文（可空指针 = navdata 缺失——查询返回 -32000，
// 决策 47；服务照起，airframe 等无数据依赖端点可用）。
struct PlanContext {
  bf::NavDatabase* db = nullptr;
  const AirportIndex* airports = nullptr;  // alternates 数据源（决策 48）
  std::string data_dir;  // airframe 档案目录（airframes.json）
  uint32_t cycle = 0;    // 当前 AIRAC 周期（0 = 无，list_cycles）
  // airframe 文件写串行化（决策 44：handler 在线程池执行；MakePlanHandlers
  // 保证非空——shared_ptr 随 ctx 拷贝共享同一互斥；审查修复：原函数内
  // static 违反"禁 static 可变状态"）。
  std::shared_ptr<std::mutex> file_mutex;
};

// 组装全端点 handler 表：plan.routes/generate/alternates/export +
// airframe.list/get/upsert/delete + list_cycles + MakeBfHandlers
// （bf 8 个透传，find_routes 排除——决策 16）。
std::unordered_map<std::string, RpcHandler> MakePlanHandlers(PlanContext ctx);

}  // namespace px
