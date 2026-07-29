// SPDX-License-Identifier: MIT
#pragma once
// bf::Coordinate 扩展——自由函数补充 bf engine 未提供的坐标操作。

#include <cmath>

#include "core/domain/coordinate.h"  // bf::Coordinate

namespace px {

// 初始大圆方位角（前向），真北起算，角度制 [0, 360)。
inline double BearingTo(const bf::Coordinate& from, const bf::Coordinate& to) {
  constexpr double kPi = 3.14159265358979323846;
  auto to_rad = [](double deg) { return deg * kPi / 180.0; };
  auto to_deg = [](double rad) { return rad * 180.0 / kPi; };

  const double lat1 = to_rad(from.latitude);
  const double lat2 = to_rad(to.latitude);
  const double d_lon = to_rad(to.longitude - from.longitude);

  const double y = std::sin(d_lon) * std::cos(lat2);
  const double x = std::cos(lat1) * std::sin(lat2) -
                   std::sin(lat1) * std::cos(lat2) * std::cos(d_lon);
  double bearing = to_deg(std::atan2(y, x));
  if (bearing < 0.0) bearing += 360.0;
  return bearing;
}

}  // namespace px
