#include "px/core/astar.h"

#include <algorithm>
#include <cstdint>
#include <limits>
#include <queue>

namespace px {

namespace {

constexpr double kInfinity = std::numeric_limits<double>::infinity();

// 优先队列节点，按 f = g + h 升序排列。
struct QueueNode {
  double f = 0.0;
  int vertex = -1;
  bool operator>(const QueueNode& other) const { return f > other.f; }
};

// 对一条有向边评估全部活跃约束。
// 任一硬约束阻断则返回 false；否则将所有软罚分累加到 extra_cost。
bool EdgeAllowed(const SearchOptions& options, const GraphEdge& edge,
                 const Coordinate& from_coord, const Coordinate& to_coord,
                 double& extra_cost) {
  extra_cost = 0.0;
  if (options.constraints.empty() || options.request == nullptr)
    return true;

  const EdgeContext ctx{edge, from_coord, to_coord};
  for (const Constraint* c : options.constraints) {
    const EdgeVerdict v = c->Evaluate(ctx, *options.request);
    if (!v.allowed) return false;
    extra_cost += v.extra_cost;
  }
  return true;
}

}  // namespace

// 单源单目标 A* 搜索。
// 以大圆距离为启发式——在球面地球上任意真实路径都不短于大圆，
// 因此启发式可容许，保证首个弹出的目标路径为最优解。
//
// node_blocked / edge_blocked 是 Yen 算法用来排除特定顶点/边的钩子；
// 普通调用留空即可。
ShortestPath FindShortestPath(const NavGraph& graph,
                              int start, int goal,
                              const SearchOptions& options) {
  const int V = graph.VertexCount();
  ShortestPath result;

  if (options.node_blocked &&
      (options.node_blocked(start) || options.node_blocked(goal)))
    return result;

  std::vector<double> g(V, kInfinity);
  std::vector<double> geo(V, 0.0);
  std::vector<int> prev(V, -1);
  std::vector<uint8_t> closed(V, 0);

  std::priority_queue<QueueNode, std::vector<QueueNode>,
                      std::greater<QueueNode>>
      open;

  // 启发式函数：优先使用重载，否则大圆距离。
  auto h = [&](int v) -> double {
    if (options.heuristic) return options.heuristic(v);
    return graph.CoordOf(v).DistanceTo(graph.CoordOf(goal));
  };

  g[start] = 0.0;
  open.push({h(start), start});

  while (!open.empty()) {
    QueueNode cur = open.top();
    open.pop();
    int u = cur.vertex;
    if (closed[u]) continue;
    closed[u] = 1;

    if (u == goal) {
      result.found = true;
      result.distance_nm = geo[u];
      result.cost = g[u];
      for (int v = goal; v != -1; v = prev[v])
        result.vertices.push_back(v);
      std::reverse(result.vertices.begin(), result.vertices.end());
      return result;
    }

    for (const GraphEdge* e = graph.EdgesBegin(u);
         e != graph.EdgesEnd(u); ++e) {
      int v = e->to;
      if (closed[v]) continue;
      if (options.node_blocked && options.node_blocked(v)) continue;
      if (options.edge_blocked && options.edge_blocked(u, v)) continue;

      double extra = 0.0;
      if (!EdgeAllowed(options, *e, graph.CoordOf(u), graph.CoordOf(v), extra))
        continue;

      double new_g = g[u] + e->distance_nm + extra;
      if (new_g >= g[v]) continue;

      g[v] = new_g;
      geo[v] = geo[u] + e->distance_nm;
      prev[v] = u;
      open.push({new_g + h(v), v});
    }
  }

  return result;
}

// 无约束最短路便利重载。
ShortestPath FindShortestPath(const NavGraph& graph, int start, int goal) {
  return FindShortestPath(graph, start, goal, SearchOptions{});
}

}  // namespace px
