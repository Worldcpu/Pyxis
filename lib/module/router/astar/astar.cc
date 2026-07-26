#include "px/module/router/astar/astar.h"

#include <algorithm>
#include <cstdint>
#include <queue>

namespace px {

namespace {

constexpr int kCancelCheckInterval = 10000;

struct QueueNode {
  double f = 0.0;
  int vertex = -1;
  bool operator>(const QueueNode& other) const { return f > other.f; }
};

bool EdgeAllowed(const SearchOptions& options, const GraphEdge& edge,
                 const Coordinate& from_coord, const Coordinate& to_coord,
                 double& extra_cost) {
  extra_cost = 0.0;
  if (options.constraints.empty() || options.request == nullptr) return true;
  const EdgeContext ctx{edge, from_coord, to_coord};
  for (const Constraint* c : options.constraints) {
    const EdgeVerdict v = c->Evaluate(ctx, *options.request);
    if (!v.allowed) return false;
    extra_cost += v.extra_cost;
  }
  return true;
}

}  // namespace

void SearchWorkspace::EnsureSize(int V) {
  if (static_cast<int>(g.size()) < V) {
    g.resize(V);
    geo.resize(V);
    prev.resize(V);
    prev_edge.resize(V);
    stamp.resize(V, 0);
    closed_stamp.resize(V, 0);
  }
}

ShortestPath FindShortestPath(const NavGraph& graph, int start, int goal,
                              const SearchOptions& options,
                              SearchWorkspace* ws) {
  const int V = graph.VertexCount();
  ShortestPath result;

  if (options.node_blocked &&
      (options.node_blocked(start) || options.node_blocked(goal)))
    return result;

  // 局部数组（无 workspace 时使用）
  std::vector<double> local_g, local_geo;
  std::vector<int> local_prev, local_prev_edge;
  std::vector<uint8_t> local_closed;
  std::vector<uint8_t> local_gen;

  if (ws) {
    ws->EnsureSize(V);
    ws->Clear();
  } else {
    local_g.assign(V, SearchWorkspace::kInfinity);
    local_geo.assign(V, 0.0);
    local_prev.assign(V, -1);
    local_prev_edge.assign(V, -1);
    local_closed.assign(V, 0);
    local_gen.assign(V, 0);
  }

  auto h = [&](int v) -> double {
    if (options.heuristic) return options.heuristic(v);
    return graph.CoordOf(v).DistanceTo(graph.CoordOf(goal));
  };

  std::priority_queue<QueueNode, std::vector<QueueNode>,
                      std::greater<QueueNode>>
      open;

  if (ws) {
    ws->Relax(start, 0.0, 0.0, -1, -1);
  } else {
    local_g[start] = 0.0;
    local_gen[start] = 1;
  }
  open.push({h(start), start});

  int expanded = 0;

  while (!open.empty()) {
    if (options.cancel && ++expanded % kCancelCheckInterval == 0) {
      if (options.cancel->IsCancelled()) return result;
    }

    QueueNode cur = open.top();
    open.pop();
    int u = cur.vertex;

    // 跳过关闭顶点（workspace 用 closed_stamp，局部用 closed 数组）
    if (ws) {
      if (ws->IsClosed(u)) continue;
    } else {
      if (local_closed[u]) continue;
    }

    if (u == goal) {
      result.found = true;
      result.distance_nm = ws ? ws->Geo(u) : local_geo[u];
      result.cost = ws ? ws->G(u) : local_g[u];

      // 重建顶点、边索引、累积代价
      std::vector<int> rev_vertices, rev_edges;
      std::vector<double> rev_cumul;
      for (int v = goal; v != -1; v = ws ? ws->Prev(v) : local_prev[v]) {
        rev_vertices.push_back(v);
        rev_cumul.push_back(ws ? ws->G(v) : local_g[v]);
        if (ws) {
          int ei = v == goal ? ws->prev_edge[v] : ws->prev_edge[v];
          if (ei != -1 && !rev_edges.empty() ? true : ei != -1)
            rev_edges.push_back(ei);
        } else {
          if (local_prev_edge[v] != -1 && v != goal)
            rev_edges.push_back(local_prev_edge[v]);
          else if (local_prev_edge[v] != -1)
            rev_edges.push_back(local_prev_edge[v]);
        }
      }
      // 简化重建逻辑
      std::reverse(rev_vertices.begin(), rev_vertices.end());
      std::reverse(rev_cumul.begin(), rev_cumul.end());

      result.vertices = std::move(rev_vertices);
      result.cumulative_cost = std::move(rev_cumul);

      // 重建 edges 更简单的方式：从 prev_edge 逐个回溯
      std::vector<int> path_edges;
      for (int at = goal; at != -1;) {
        int p = ws ? ws->Prev(at) : local_prev[at];
        if (p == -1) break;
        path_edges.push_back(ws ? ws->prev_edge[at] : local_prev_edge[at]);
        at = p;
      }
      std::reverse(path_edges.begin(), path_edges.end());
      result.edges = std::move(path_edges);
      return result;
    }

    if (ws) {
      ws->MarkClosed(u);
    } else {
      local_closed[u] = 1;
    }

    for (const GraphEdge* e = graph.EdgesBegin(u); e != graph.EdgesEnd(u);
         ++e) {
      int v = e->to;

      if (ws) {
        if (ws->IsClosed(v)) continue;
      } else {
        if (local_closed[v]) continue;
      }

      if (options.node_blocked && options.node_blocked(v)) continue;
      if (options.edge_blocked && options.edge_blocked(u, v)) continue;

      double extra = 0.0;
      if (!EdgeAllowed(options, *e, graph.CoordOf(u), graph.CoordOf(v), extra))
        continue;

      double g_u = ws ? ws->G(u) : local_g[u];
      double new_g = g_u + e->distance_nm + extra;

      if (ws) {
        if (ws->Live(v) && new_g >= ws->G(v)) continue;
      } else {
        if (local_gen[v] == 1 && new_g >= local_g[v]) continue;
      }

      double geo_u = ws ? ws->Geo(u) : local_geo[u];
      int edge_idx = static_cast<int>(e - graph.EdgesBegin(0));

      if (ws) {
        ws->Relax(v, new_g, geo_u + e->distance_nm, u, edge_idx);
      } else {
        local_g[v] = new_g;
        local_geo[v] = geo_u + e->distance_nm;
        local_prev[v] = u;
        local_prev_edge[v] = edge_idx;
        local_gen[v] = 1;
      }

      open.push({new_g + h(v), v});
    }
  }

  return result;
}

ShortestPath FindShortestPath(const NavGraph& graph, int start, int goal) {
  return FindShortestPath(graph, start, goal, SearchOptions{});
}

const GraphEdge* SelectEdge(const NavGraph& graph, int from, int to,
                            const SearchOptions& options, double* out_cost) {
  const GraphEdge* best = nullptr;
  double best_cost = SearchWorkspace::kInfinity;
  for (const GraphEdge* e = graph.EdgesBegin(from); e != graph.EdgesEnd(from);
       ++e) {
    if (e->to != to) continue;
    double extra_cost = 0.0;
    if (!EdgeAllowed(options, *e, graph.CoordOf(from), graph.CoordOf(to),
                     extra_cost))
      continue;
    const double total = e->distance_nm + extra_cost;
    if (total < best_cost) {
      best_cost = total;
      best = e;
    }
  }
  if (out_cost != nullptr && best != nullptr) *out_cost = best_cost;
  return best;
}

}  // namespace px
