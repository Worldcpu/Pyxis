#pragma once
// bravofinder yen_kshortest.cc 的直接移植——CostOfPath + SelectEdge 方案。
// 用作 Pyxis 边序列存储方案的独立基准测试件。
// 对照 bravofinder lib/core/graph/yen_kshortest.cc。

#include <algorithm>
#include <cstdint>
#include <set>
#include <unordered_set>
#include <vector>

#include "px/core/astar.h"
#include "px/core/nav_graph.h"

namespace bravo {

namespace {

inline int64_t EdgeKey(int from, int to) {
  return (static_cast<int64_t>(from) << 32) | static_cast<uint32_t>(to);
}

bool CostOfPath(const px::NavGraph& graph, const std::vector<int>& path,
                const px::SearchOptions& options, double& cost,
                double& distance) {
  cost = 0.0;
  distance = 0.0;
  for (size_t i = 0; i + 1 < path.size(); ++i) {
    const int u = path[i];
    const int v = path[i + 1];
    double edge_cost = 0.0;
    const px::GraphEdge* e = px::SelectEdge(graph, u, v, options, &edge_cost);
    if (e == nullptr) return false;
    cost += edge_cost;
    distance += e->distance_nm;
  }
  return true;
}

struct Candidate {
  double cost;
  double distance;
  std::vector<int> vertices;
  int deviation = 0;
  bool operator<(const Candidate& other) const {
    if (cost != other.cost) return cost < other.cost;
    return vertices < other.vertices;
  }
};

}  // namespace

// bravofinder 原版 Yen（含 Lawler + CostOfPath/SelectEdge）——直接对照。
inline std::vector<px::ShortestPath> FindKShortestPaths(
    const px::NavGraph& graph, int start, int goal, int k,
    const px::SearchOptions& base_options) {
  std::vector<px::ShortestPath> result;
  if (k <= 0) return result;

  px::SearchWorkspace ws;
  px::ShortestPath first =
      px::FindShortestPath(graph, start, goal, base_options, &ws);
  if (!first.found) return result;
  result.push_back(std::move(first));

  std::set<Candidate> candidates;
  int last_deviation = 0;

  for (int kth = 1; kth < k; ++kth) {
    const std::vector<int>& prev_path = result.back().vertices;

    for (size_t i = static_cast<size_t>(last_deviation);
         i + 1 < prev_path.size(); ++i) {
      const int spur_node = prev_path[i];
      const std::vector<int> root(prev_path.begin(), prev_path.begin() + i + 1);

      std::unordered_set<int64_t> banned_edges;
      for (const px::ShortestPath& p : result) {
        if (p.vertices.size() > i + 1 &&
            std::equal(root.begin(), root.end(), p.vertices.begin())) {
          banned_edges.insert(EdgeKey(p.vertices[i], p.vertices[i + 1]));
        }
      }
      const std::unordered_set<int> banned_nodes(root.begin(), root.end() - 1);

      px::SearchOptions spur_opts = base_options;
      auto base_nb = base_options.node_blocked;
      auto base_eb = base_options.edge_blocked;
      spur_opts.node_blocked = [banned_nodes, base_nb](int v) {
        return banned_nodes.count(v) != 0 || (base_nb && base_nb(v));
      };
      spur_opts.edge_blocked = [banned_edges, base_eb](int from, int to) {
        return banned_edges.count(EdgeKey(from, to)) != 0 ||
               (base_eb && base_eb(from, to));
      };

      const px::ShortestPath spur =
          px::FindShortestPath(graph, spur_node, goal, spur_opts, &ws);
      if (!spur.found) continue;

      std::vector<int> total(root.begin(), root.end() - 1);
      total.insert(total.end(), spur.vertices.begin(), spur.vertices.end());

      double cost = 0.0;
      double distance = 0.0;
      if (CostOfPath(graph, total, base_options, cost, distance)) {
        candidates.insert(
            Candidate{cost, distance, std::move(total), static_cast<int>(i)});
      }
    }

    if (candidates.empty()) break;

    auto best = candidates.begin();
    px::ShortestPath next;
    next.vertices = best->vertices;
    next.cost = best->cost;
    next.distance_nm = best->distance;
    next.found = true;
    last_deviation = best->deviation;
    candidates.erase(best);
    result.push_back(std::move(next));
  }

  return result;
}

}  // namespace bravo
