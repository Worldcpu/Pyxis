// SPDX-License-Identifier: MIT
#pragma once

#include <optional>
#include <string>
#include <vector>

#include "px/module/flightplan/altitude_planner.h"
#include "px/module/flightplan/flight_segment.h"

namespace px {

// 航路点序列视图元素（决策 22/30③：点序列为派生视图——含程序内点；
// segment_index 为段归属，起飞机场点 = 段 0，落地机场点 = 末段）。
// Navlog 数值字段位（cum_nm/wind/gs/ete/utc）由燃油引擎（Phase 10）填充。
struct FlightPoint {
  std::string ident;
  std::string via;  // 所在段的 via（Navlog 显示用）
  double latitude = 0.0;
  double longitude = 0.0;
  // 段归属下标；segments 为空（bf 单顶点路径边界）时为 -1 哨兵——
  // 消费端须先查 segments 空再按下标取段。
  int segment_index = 0;
};

// 飞行计划聚合根（决策 2：航段一等模型——segments 为真源；点序列为派生
// 视图）。不可变值类型。
// 巡航高度三元组（决策 8：(FL, 米制等价, 规则名) + 推荐依据）。
struct CruiseAltitude {
  int fl = 0;
  int meters = 0;
  AltitudeRule rule = AltitudeRule::kAuto;
  std::string rationale;  // 推荐依据（auto 时）
  bool manual = false;  // 手动输入（决策 8：只校验提示不拦截）
};

// 燃油阶梯字段位（决策 21/14：数值由 Phase 10 FuelEngine 填充；
// engine/experimental 标识在 meta）。std::nullopt = 未计算。
struct FuelLadder {
  std::optional<double> taxi_kg;
  std::optional<double> trip_kg;
  std::optional<double> contingency_kg;
  std::optional<double> alternate_kg;
  std::optional<double> final_reserve_kg;
  std::optional<double> extra_kg;
  std::optional<double> block_kg;
};

// 配载与重量（决策 13/5：配载在模块内算；重量校验结果）。
struct PlanWeights {
  double dow_kg = 0.0;
  double zfw_kg = 0.0;
  double tow_kg = 0.0;
  double lw_kg = 0.0;
  double mzfw_kg = 0.0;
  double mtow_kg = 0.0;
  double mlw_kg = 0.0;
};

// 检查结果（决策 8：通过 / 超限警告 / 任务不可执行）。
enum class CheckStatus { kOk, kWarning, kUnflyable };

struct PlanChecks {
  CheckStatus status = CheckStatus::kOk;
  std::vector<std::string> warnings;  // 逐条警告
};

// 飞行计划聚合根（决策 2：航段一等模型——segments 为真源；点序列为派生
// 视图）。不可变值类型。
struct FlightPlan {
  std::string route_string;  // ICAO filed-plan 串（决策 7：候选条目核心字段）
  // 程序与连接（决策 9 候选契约；FromBf 自 bf::Route 搬运，无则空串）。
  std::string sid;             // 离场程序名
  std::string star;            // 进场程序名
  std::string dep_runway;      // 离场跑道
  std::string arr_runway;      // 进场跑道
  std::string dep_connection;  // bf 稳定 token：procedure/direct/radar_vectors/
  std::string arr_connection;  // terminal_transition（bf::ToString 形式）
  std::vector<FlightSegment> segments;
  std::vector<FlightPoint> points;

  // generate 阶段字段（决策 8/14/21/26）。
  CruiseAltitude altitude;
  FuelLadder fuel;
  PlanWeights weights;
  PlanChecks checks;
  bool mora_checked = true;  // 决策 26：手写航路 false（前端提示）
  bool experimental = false;  // 决策 21：实验性引擎标志
};

}  // namespace px
