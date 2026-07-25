#pragma once

#include "px/core/constraint.h"
#include "px/core/route_query.h"

namespace px {

// 硬过滤：设置了巡航高度范围时，航段仅当该范围与 [base_fl, top_fl] 重叠
// 时才可用。0 表示该侧无限制：base_fl==0 为开放下限，top_fl==0 为开放上限。
class AltitudeBandConstraint : public Constraint {
 public:
  EdgeVerdict Evaluate(const EdgeContext& ctx,
                       const RouteQuery& query) const override {
    if (!query.altitude.has_value()) return EdgeVerdict::Allow();

    const GraphEdge& e = ctx.edge;
    const FlRange& r = *query.altitude;

    if (e.base_fl != 0 && r.max_fl < e.base_fl) return EdgeVerdict::Block();
    if (e.top_fl != 0 && r.min_fl > e.top_fl) return EdgeVerdict::Block();
    return EdgeVerdict::Allow();
  }
};

// 软罚分：引导搜索偏好指定航路层级（高/低），不禁止另一层级。
class LevelPreferenceConstraint : public Constraint {
 public:
  explicit LevelPreferenceConstraint(double penalty_fraction = 0.5)
      : penalty_fraction_(penalty_fraction) {}

  EdgeVerdict Evaluate(const EdgeContext& ctx,
                       const RouteQuery& query) const override {
    if (query.level == LevelPreference::kNone) return EdgeVerdict::Allow();

    // kBoth 层级航段在任意偏好下均不受罚。
    if (ctx.edge.level == AirwayLevel::kBoth) return EdgeVerdict::Allow();

    const bool wants_high = query.level == LevelPreference::kHigh;
    if ((ctx.edge.level == AirwayLevel::kHigh) == wants_high)
      return EdgeVerdict::Allow();

    return EdgeVerdict::Penalize(ctx.edge.distance_nm * penalty_fraction_);
  }

 private:
  double penalty_fraction_;
};

}  // namespace px
