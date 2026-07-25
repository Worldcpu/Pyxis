#pragma once

#include <optional>
#include <string>

namespace px {

// 航路高度偏好。
enum class LevelPreference {
  kNone,  // 无偏好（默认）
  kLow,   // 偏好低空（Victor）航路
  kHigh   // 偏好高空（Jet）航路
};

// 巡航高度层范围（FL，百英尺），两端均包含。
struct FlRange {
  int min_fl = 0;
  int max_fl = 0;
};

// 航路查询参数——纯数据 struct。
// Phase 2 定义，供 Phase 3 约束框架和 Phase 5 NavDatabase 共用。
struct RouteQuery {
  std::string departure_icao;
  std::string arrival_icao;

  // 巡航高度范围。未设置时不应用高度相关约束（等价于无约束最短路）。
  std::optional<FlRange> altitude;

  LevelPreference level = LevelPreference::kNone;

  // 候选航路数（Yen K-最短路径），默认为 1。
  int max_routes = 1;
};

}  // namespace px
