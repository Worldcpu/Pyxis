#pragma once

namespace px {

// WGS-84 坐标系下的地理坐标（十进制角度）。
//
// 北纬为正，东经为正。Coordinate 是不可变值类型；所有成员公开，
// 因为它仅存储两个数值，不维护额外不变量。
struct Coordinate {
  double latitude = 0.0;   // 纬度，度，[-90, 90]
  double longitude = 0.0;  // 经度，度，[-180, 180]

  // 到另一个坐标的大圆距离，单位为海里（NM），使用球面地球模型上的
  // haversine 公式计算。
  double DistanceTo(const Coordinate& other) const;

  // 从本坐标到另一个坐标的初始方位角（真北顺时针），度，[0, 360)。
  double BearingTo(const Coordinate& other) const;
};

}  // namespace px
