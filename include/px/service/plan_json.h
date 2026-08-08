// SPDX-License-Identifier: MIT
#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "px/module/flightplan/flight_plan.h"

namespace px {

// 候选数组 JSON 渲染（决策 9：条目 = index/route_string/距离（总分+分阶段）/
// sid/star/跑道/connection_kind/seed/完整点序列；**不带 segments**——generate
// 阶段才物化）。seed 由 px 层管理（决策 7：响应回传，FlightPlan 元数据携带）。
// 纯函数——输入 FlightPlan 域对象，不依赖 NavDatabase。
std::string RenderPlanCandidatesJson(const std::vector<FlightPlan>& candidates,
                                     uint32_t seed);

// 完整计划 JSON 渲染（决策 14：route（route_string+segments+点序列）/
// altitude 三元组/fuel 字段位/weights/checks/mora_checked/experimental）。
std::string RenderPlanJson(const FlightPlan& plan);

// 航路分析结果（决策 54：plan.analyze）。
struct AnalyzeResult {
  bool valid = false;
  int cycle = 0;
  double distance_nm = 0.0;         // valid=true 时有效
  std::vector<std::string> errors;  // valid=false 时逐条（bf ParseRoute 消息）
  // valid=true 时完整点序列（S9.1 修订：前端规划航路图层需要几何——
  // 父 epic 用户故事 23 要求分析后地图高亮规划航路，T1 契约补丁）。
  std::vector<FlightPoint> points;
};

// 航路分析 JSON 渲染（决策 54）：{valid, cycle, distance_nm?, errors[]}。
std::string RenderAnalyzeJson(const AnalyzeResult& result);

}  // namespace px
