#pragma once

#include <algorithm>
#include <cmath>

#include "px/core/constraint.h"
#include "px/core/mora_grid.h"

namespace px {

// 完整圆周度数，用于将经度差规整为较短弧（绕 antimeridian 时的短路径）。
inline constexpr double kDegreesFullCircle = 360.0;

// 硬过滤：设置了巡航高度范围时，仅当范围中存在某个高度层在整个航段上
// 不低于网格最低偏航安全高度（MORA）时，该边才可用——即范围的顶端处处
// 高于安全下界。这使航路能在地形/障碍物之上保持安全高度。
// 无 MORA 数据的单元格（值 0）不施加任何下界。
//
// 采样策略：MORA 是 1° 网格，长航段可能穿越多个单元格，包括两端点均
// 未落入的高 MORA 单元格。约束沿大圆轨迹采样（在 1° 网格分辨率下，
// 经纬度线性插值足够准确），取所有采样点及两端点中的最大 MORA 值。
// 短航段退化为仅检查两端点。
//
// 注意：MORA 是 MSL 高度而飞行高度层是气压高度；直接比较两者是目前
// 合理的安全下界近似。
//
// 对照 bravofinder lib/core/constraints/mora_constraint.h。
class MoraConstraint : public Constraint {
 public:
  explicit MoraConstraint(const MoraGrid& grid) : grid_(grid) {}

  EdgeVerdict Evaluate(const EdgeContext& ctx,
                       const RouteQuery& query) const override {
    if (!query.altitude.has_value()) return EdgeVerdict::Allow();

    const int16_t mora = MaxMoraAlongLeg(ctx.from_coord, ctx.to_coord);
    // 沿途所有已知单元格均无下界
    if (mora == 0) return EdgeVerdict::Allow();

    // 即便是巡航范围顶端也低于 MORA——此边不可通行
    if (query.altitude->max_fl < mora) return EdgeVerdict::Block();

    return EdgeVerdict::Allow();
  }

 private:
  // 航段沿途已知的最高 MORA 值，采样间距约 0.5°（细于 1° 网格，
  // 确保不漏过任何单元格），同时检查两端点。全部采样单元格未知时返回 0。
  int16_t MaxMoraAlongLeg(const Coordinate& from, const Coordinate& to) const {
    int16_t best = 0;
    best = std::max(best, grid_.MoraAt(from));
    best = std::max(best, grid_.MoraAt(to));

    const double dlat = to.latitude - from.latitude;
    // 规整经度差为较短弧：例如 +179° → -179° 差 -2° 而非 +358°。
    // 不如此处理则采样点会绕远路读取完全错误的 MORA 格子。
    const double dlon =
        std::remainder(to.longitude - from.longitude, kDegreesFullCircle);
    const double span = std::max(std::abs(dlat), std::abs(dlon));
    const int steps = std::max(1, static_cast<int>(std::ceil(span / 0.5)));

    for (int i = 1; i < steps; ++i) {
      const double t = static_cast<double>(i) / steps;
      // 将插值经度回绕至 [-180, 180]，使穿越 antimeridian 的采样点
      // 仍能映射到正确的网格单元格。
      const double lon =
          std::remainder(from.longitude + dlon * t, kDegreesFullCircle);
      best = std::max(best,
                      grid_.MoraAt(Coordinate{from.latitude + dlat * t, lon}));
    }
    return best;
  }

  const MoraGrid& grid_;
};

}  // namespace px
