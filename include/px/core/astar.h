#pragma once

#include <functional>
#include <vector>

#include "px/core/constraint.h"
#include "px/core/nav_graph.h"
#include "px/core/route_query.h"

namespace px {

// A* 搜索结果。
struct ShortestPath {
  std::vector<int> vertices;  // 起点→终点的顶点序列；不可达时为空
  double distance_nm = 0.0;   // 地理距离（不含软罚分）
  double cost = 0.0;          // 有效代价 = 距离 + 软罚分
  bool found = false;
};

// 搜索选项：约束 + Yen 封禁函数（Phase 4）。
struct SearchOptions {
  std::vector<const Constraint*> constraints;
  const RouteQuery* request = nullptr;

  // 可选启发式重载：未设置时使用大圆距离；设置为零即退化为 Dijkstra。
  std::function<double(int vertex)> heuristic;

  std::function<bool(int)> node_blocked;
  std::function<bool(int from, int to)> edge_blocked;
};

// 单源单目标 A*，大圆距离可容许启发式。
ShortestPath FindShortestPath(const NavGraph& graph,
                              int start, int goal,
                              const SearchOptions& options);

// 无约束最短路。
ShortestPath FindShortestPath(const NavGraph& graph,
                              int start, int goal);

}  // namespace px
