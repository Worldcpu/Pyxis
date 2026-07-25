#pragma once

#include <cstdint>
#include <functional>
#include <limits>
#include <vector>

#include "px/core/cancel_token.h"
#include "px/core/constraint.h"
#include "px/core/nav_graph.h"
#include "px/core/route_query.h"

namespace px {

// A* 搜索结果。
struct ShortestPath {
  std::vector<int> vertices;           // 起点→终点的顶点序列；不可达时为空
  std::vector<int> edges;              // 边索引序列（NavGraph::edges_ 中的偏移）
  std::vector<double> cumulative_cost; // cumulative_cost[i] = 从起点到 vertices[i] 的有效代价
  double distance_nm = 0.0;            // 地理距离（不含软罚分）
  double cost = 0.0;                   // 有效代价 = 距离 + 软罚分
  bool found = false;

  // O(1) 获取前缀代价（Yen 中取 root path 的代价）。
  double PrefixCost(size_t prefix_vertex_count) const {
    return prefix_vertex_count > 0 ? cumulative_cost[prefix_vertex_count - 1] : 0.0;
  }
};

// 搜索选项：约束 + Yen 封禁函数（Phase 4）。
struct SearchOptions {
  std::vector<const Constraint*> constraints;
  const RouteQuery* request = nullptr;

  // 可选启发式重载：未设置时使用大圆距离；设置为零即退化为 Dijkstra。
  std::function<double(int vertex)> heuristic;

  std::function<bool(int)> node_blocked;
  std::function<bool(int from, int to)> edge_blocked;

  // 协作式取消令牌。每展开 10000 个节点检查一次；nullptr = 不可取消。
  const CancelToken* cancel = nullptr;
};

// 可复用的 A* 搜索工作区。per-vertex 数组分配一次，用代数标记在 O(1) 内
// "清空"：Touch() 在首次访问时惰性重置 g[v] 为 kInfinity，替代每次搜索的
// O(V) 数组重初始化。对照 bravofinder bench/variants/workspace 实现。
struct SearchWorkspace {
  static constexpr double kInfinity = std::numeric_limits<double>::infinity();

  std::vector<double> g;
  std::vector<double> geo;
  std::vector<int> prev;
  std::vector<int> prev_edge;          // Pyxis 扩展：到达 v 的边索引
  std::vector<uint32_t> stamp;         // 值槽位代数标记
  std::vector<uint32_t> closed_stamp;  // 关闭标记代数
  uint32_t generation = 0;             // 每搜索递增；0 = 尚未运行

  void Clear() { ++generation; }
  void EnsureSize(int V);

  bool Live(int v) const { return stamp[v] == generation; }
  bool IsClosed(int v) const { return closed_stamp[v] == generation; }
  void MarkClosed(int v) { closed_stamp[v] = generation; }

  double G(int v) const { return Live(v) ? g[v] : kInfinity; }
  double Geo(int v) const { return geo[v]; }
  int Prev(int v) const { return Live(v) ? prev[v] : -1; }

  // 惰性重置：首次访问时将 g 置为 kInfinity，prev/prev_edge 置为 -1。
  void Touch(int v) {
    if (stamp[v] != generation) {
      stamp[v] = generation;
      g[v] = kInfinity;
      geo[v] = 0.0;
      prev[v] = -1;
      prev_edge[v] = -1;
    }
  }

  // 松弛顶点 v：记录代价、地理距离、前驱顶点和边索引，并标记为活跃代数。
  void Relax(int v, double new_g, double new_geo, int u, int edge_idx) {
    Touch(v);
    g[v] = new_g;
    geo[v] = new_geo;
    prev[v] = u;
    prev_edge[v] = edge_idx;
  }
};

// 单源单目标 A*，大圆距离可容许启发式。
// 传入 workspace 时复用其数组（O(1) 清零）；传 nullptr 时内部分配。
ShortestPath FindShortestPath(const NavGraph& graph,
                              int start, int goal,
                              const SearchOptions& options,
                              SearchWorkspace* ws = nullptr);

// 无约束最短路。
ShortestPath FindShortestPath(const NavGraph& graph,
                              int start, int goal);

}  // namespace px
