#include "px/core/coordinate.h"

#include <algorithm>
#include <cmath>

namespace px {

namespace {

// 地球平均半径（海里）。6371.0088 km 为 IUGG 平均半径；
// 一海里精确等于 1.852 km。
constexpr double kEarthRadiusNm = 6371.0088 / 1.852;

constexpr double kPi = 3.14159265358979323846;

double ToRadians(double degrees) { return degrees * kPi / 180.0; }

double ToDegrees(double radians) { return radians * 180.0 / kPi; }

}  // namespace

double Coordinate::DistanceTo(const Coordinate& other) const {
  const double lat1 = ToRadians(latitude);
  const double lat2 = ToRadians(other.latitude);
  const double d_lat = lat2 - lat1;
  const double d_lon = ToRadians(other.longitude - longitude);

  const double sin_lat = std::sin(d_lat / 2.0);
  const double sin_lon = std::sin(d_lon / 2.0);
  const double a =
      sin_lat * sin_lat + std::cos(lat1) * std::cos(lat2) * sin_lon * sin_lon;
  // atan2 形式：对趾点/近对趾点处数值稳定——浮点误差可能
  // 导致 a 略大于 1.0（此时 sqrt(a) > 1 会使 asin 产生 NaN）。
  const double c =
      2.0 * std::atan2(std::sqrt(a), std::sqrt(std::max(0.0, 1.0 - a)));
  return kEarthRadiusNm * c;
}

double Coordinate::BearingTo(const Coordinate& other) const {
  const double lat1 = ToRadians(latitude);
  const double lat2 = ToRadians(other.latitude);
  const double d_lon = ToRadians(other.longitude - longitude);

  const double y = std::sin(d_lon) * std::cos(lat2);
  const double x = std::cos(lat1) * std::sin(lat2) -
                   std::sin(lat1) * std::cos(lat2) * std::cos(d_lon);
  double bearing = ToDegrees(std::atan2(y, x));
  // 归一化到 [0, 360)
  if (bearing < 0.0) bearing += 360.0;
  return bearing;
}

}  // namespace px
