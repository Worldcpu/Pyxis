#pragma once

#include <string>

namespace px {

// 航路查询参数——纯数据 struct。
// Phase 2 定义，供 Phase 3 约束框架和 Phase 5 NavDatabase 共用。
struct RouteQuery {
  std::string departure_icao;
  std::string arrival_icao;
  int cruise_fl = 370;
  int max_routes = 5;
};

}  // namespace px
