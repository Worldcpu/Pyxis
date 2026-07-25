#pragma once

#include "px/core/coordinate.h"
#include "px/core/graph_edge.h"
#include "px/core/route_query.h"

namespace px {

// 约束对一条边的评估结果。
struct EdgeVerdict {
  bool allowed = true;         // false = 硬约束阻断，此边不可通行
  double extra_cost = 0.0;     // 软约束罚分（海里等值）

  static EdgeVerdict Allow() { return {true, 0.0}; }
  static EdgeVerdict Block() { return {false, 0.0}; }
  static EdgeVerdict Penalize(double cost) { return {true, cost}; }
};

// 单条有向边的评估上下文：边本身及两端点坐标。
// 位置相关约束（如 MORA）可据此采样沿途地形单元。
struct EdgeContext {
  const GraphEdge& edge;
  Coordinate from_coord;
  Coordinate to_coord;
};

// 可插拔航路约束。每条边在活跃查询下被评估，
// 返回是否可用（硬过滤）及/或额外代价（软罚分）。
// 约束是无状态的，由搜索组合：任一约束阻断则边被丢弃，罚分求和。
class Constraint {
 public:
  virtual ~Constraint() = default;
  virtual EdgeVerdict Evaluate(const EdgeContext& ctx,
                               const RouteQuery& query) const = 0;
};

}  // namespace px
