#pragma once

#include <string>

#include "px/core/graph_edge.h"

namespace px {

// 原始航点记录——解析器产出，GraphBuilder 消费。
struct RawWaypoint {
  std::string ident;
  std::string region;
  double latitude = 0.0;
  double longitude = 0.0;
};

// 原始航段记录。
struct RawSegment {
  std::string from_ident;
  std::string from_region;
  std::string to_ident;
  std::string to_region;
  std::string airway;                      // 航路名，空串 = DCT
  AirwayDirection direction = AirwayDirection::kBoth;
  AirwayLevel level = AirwayLevel::kLow;
  int base_fl = 0;                         // 底高（FL），0 = 不限
  int top_fl = 0;                          // 顶高（FL），0 = 不限
};

}  // namespace px
