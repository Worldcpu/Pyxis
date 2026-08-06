// SPDX-License-Identifier: MIT
#pragma once

#include <string>

namespace px {

// 航段类型（flightplan 决策 2/2c：程序聚合段——SID/STAR/进近各一段，
// enroute 逐航路段、DCT 单独段、备降整体一段。无 taxi 段——滑行时间
// 是用户设置项，由燃油政策单独处理）。
enum class SegmentKind {
  kSid,        // 离场程序段（连续 SID legs 聚合为一段）
  kEnroute,    // 航路段（逐航路段；DCT 单独段）
  kStar,       // 进场程序段（连续 STAR legs 聚合为一段）
  kApproach,   // 进近段（DCT-to-IAF 等，无 STAR 时）
  kAlternate,  // 备降段（px 构造，不在 bf::Route 内）
};

// 航段：一等不可变值类型（决策 2b——独立于 bf 类型，经 FromBf() 一次性
// 转换产生；坐标与 top_fl 由后续切片补充）。
struct FlightSegment {
  SegmentKind kind = SegmentKind::kEnroute;
  std::string from_ident;
  std::string to_ident;
  std::string via;  // 航路名 / "DCT" / "SID" / "STAR"
  double distance_nm = 0.0;
};

}  // namespace px
