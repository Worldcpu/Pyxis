#include "px/module/router/yen.h"

#include <algorithm>
#include <cstdint>
#include <set>
#include <tl/expected.hpp>
#include <unordered_set>

#include "px/core/result.h"

namespace px {

namespace {

// 有向边打包为 64 位键（banned_edges 用）。对照 bravofinder yen_kshortest.cc:18。
inline int64_t EdgeKey(int from, int to) {
  return (static_cast<int64_t>(from) << 32) | static_cast<uint32_t>(to);
}

// B 集候选路径。存 edges + cumulative_cost——候选构造 O(1) 前缀代价相加。
struct Candidate {
  std::vector<int> vertices;
  std::vector<int> edges;
  std::vector<double> cumulative_cost;
  double cost = 0.0;
  double distance = 0.0;
  int deviation = 0;

  bool operator<(const Candidate& o) const {
    if (cost != o.cost) return cost < o.cost;
    return vertices < o.vertices;
  }
};

constexpr int kMaxRoutes = 8;

double EdgeDistance(const NavGraph& graph, const std::vector<int>& edges) {
  double d = 0.0;
  for (int ei : edges) d += graph.EdgeAt(ei).distance_nm;
  return d;
}

// 从边索引序列累积地理距离，构建 cumul[0]=0, cumul[i+1]=cumul[i]+dist[i]。
std::vector<double> BuildCumulDistance(const NavGraph& graph,
                                       const std::vector<int>& edges) {
  std::vector<double> c(edges.size() + 1, 0.0);
  for (size_t i = 0; i < edges.size(); ++i)
    c[i + 1] = c[i] + graph.EdgeAt(edges[i]).distance_nm;
  return c;
}

}  // namespace

Result<std::vector<ShortestPath>> FindKShortestPaths(
    const NavGraph& graph, int start, int goal, int k,
    const YenOptions& options) {
  using PathVec = std::vector<ShortestPath>;

  if (k < 1 || k > kMaxRoutes)
    return tl::make_unexpected(
        Error{ErrorCode::kInvalidInput, "k must be in [1, 8]"});

  SearchWorkspace ws;

  ShortestPath first =
      FindShortestPath(graph, start, goal, options.search, &ws);
  if (!first.found) return PathVec{};

  PathVec result;
  result.push_back(std::move(first));
  if (k == 1) return std::move(result);

  std::set<Candidate> candidates;
  int last_deviation = 0;

  for (int kth = 1; kth < k; ++kth) {
    if (options.cancel && options.cancel->IsCancelled())
      return std::move(result);

    const auto& prev_path = result.back();
    const auto& prev_verts = prev_path.vertices;

    for (size_t i = static_cast<size_t>(last_deviation);
         i + 1 < prev_verts.size(); ++i) {
      const int spur_node = prev_verts[i];

      // root 前缀（顶点序列 + 代价）
      std::vector<int> root_verts(prev_verts.begin(),
                                  prev_verts.begin() + i + 1);
      double root_cost = prev_path.PrefixCost(i + 1);

      // 封禁边：所有已接受路径中与 root 共享前缀的 (i→i+1) 边
      std::unordered_set<int64_t> banned_edges;
      for (const auto& p : result) {
        if (p.vertices.size() > i + 1 &&
            std::equal(root_verts.begin(), root_verts.end(),
                       p.vertices.begin())) {
          banned_edges.insert(
              EdgeKey(p.vertices[i], p.vertices[i + 1]));
        }
      }

      // 封禁节点：root 中除 spur_node 外的节点（防环）
      const std::unordered_set<int> banned_nodes(
          root_verts.begin(), root_verts.end() - 1);

      // 组合 ban（按值捕获——自包含闭包，对照 bravofinder yen_kshortest.cc:150-164）
      SearchOptions spur_opts = options.search;
      auto base_nb = options.search.node_blocked;
      auto base_eb = options.search.edge_blocked;
      spur_opts.node_blocked = [banned_nodes, base_nb](int v) {
        return banned_nodes.count(v) != 0 ||
               (base_nb && base_nb(v));
      };
      spur_opts.edge_blocked = [banned_edges, base_eb](int from, int to) {
        return banned_edges.count(EdgeKey(from, to)) != 0 ||
               (base_eb && base_eb(from, to));
      };

      const ShortestPath spur =
          FindShortestPath(graph, spur_node, goal, spur_opts, &ws);
      if (!spur.found) continue;

      // 拼接候选路径——O(1) 代价
      Candidate cand;
      // 顶点：root 去掉重复 spur_node + spur
      cand.vertices.assign(root_verts.begin(), root_verts.end() - 1);
      cand.vertices.insert(cand.vertices.end(), spur.vertices.begin(),
                           spur.vertices.end());
      // 边：root_edges + spur_edges
      cand.edges.assign(prev_path.edges.begin(),
                        prev_path.edges.begin() + i);
      cand.edges.insert(cand.edges.end(), spur.edges.begin(), spur.edges.end());
      // 代价
      cand.cost = root_cost + spur.cost;
      cand.distance = EdgeDistance(graph, cand.edges);
      cand.cumulative_cost = BuildCumulDistance(graph, cand.edges);
      cand.deviation = static_cast<int>(i);

      candidates.insert(std::move(cand));
    }

    if (candidates.empty()) break;

    auto best = candidates.begin();
    ShortestPath next;
    next.vertices = best->vertices;
    next.edges = best->edges;
    next.cumulative_cost = best->cumulative_cost;
    next.cost = best->cost;
    next.distance_nm = best->distance;
    next.found = true;
    last_deviation = best->deviation;
    candidates.erase(best);
    result.push_back(std::move(next));
  }

  return std::move(result);
}

}  // namespace px
