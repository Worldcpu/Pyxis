// SPDX-License-Identifier: MIT
#pragma once
// bf::Coordinate 扩展——自由函数补充 bf engine 未提供的坐标操作。

#include <cmath>
#include <numbers>

#include "core/domain/coordinate.h"  // bf::Coordinate

namespace px {

// 初始大圆方位角（前向），真北起算，角度制 [0, 360)。
//
// 前提：from 和 to 的 latitude/longitude 均为有限值（非 NaN/Inf）。
// 边界情况：
//   from == to  → 返回 0.0（同一点方位角数学上无定义，调用方自行处理）
//   from 在极点 → 数学上从极点出发方位角无定义，函数返回数值结果但不含语义
inline double BearingTo(const bf::Coordinate& from, const bf::Coordinate& to) {
  constexpr double kRadPerDeg = std::numbers::pi / 180.0;
  constexpr double kDegPerRad = 180.0 / std::numbers::pi;

  const double lat1 = from.latitude * kRadPerDeg;
  const double lat2 = to.latitude * kRadPerDeg;
  const double d_lon = (to.longitude - from.longitude) * kRadPerDeg;

  const double y = std::sin(d_lon) * std::cos(lat2);
  const double x = std::cos(lat1) * std::sin(lat2) -
                   std::sin(lat1) * std::cos(lat2) * std::cos(d_lon);
  double bearing = std::atan2(y, x) * kDegPerRad;
  if (bearing < 0.0) bearing += 360.0;
  return bearing;
}

}  // namespace px
