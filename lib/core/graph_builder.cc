#include "graph_builder.h"

#include <algorithm>
#include <cmath>
#include <unordered_map>

namespace px {

GraphBuilder::GraphBuilder(const std::vector<RawWaypoint>& waypoints,
                           const std::vector<RawSegment>& segments) {
  const int total = static_cast<int>(waypoints.size());

  // --- 顶点：坐标 + ident ---
  graph_.coords_.reserve(total);
  idents_.reserve(total);
  for (int i = 0; i < total; ++i) {
    const auto& w = waypoints[i];
    graph_.coords_.push_back(Coordinate{w.latitude, w.longitude});
    Ident key{w.ident, w.region};
    idents_.push_back(key);
    ident_index_[key] = i;
  }

  // --- 航路名表："DCT" 保留在索引 0 —— 即使空输入也初始化 ---
  airway_names_.push_back("DCT");
  std::unordered_map<std::string, int> name_to_id;
  name_to_id["DCT"] = 0;

  // 空输入: 无需处理航段
  if (segments.empty()) {
    graph_.offsets_.resize(total + 1, 0);
    has_outbound_.assign(total, false);
    has_inbound_.assign(total, false);
    return;
  }

  auto airway_id_for = [&](const std::string& name) -> int {
    if (name.empty()) return 0;  // DCT
    auto it = name_to_id.find(name);
    if (it != name_to_id.end()) return it->second;
    int id = static_cast<int>(airway_names_.size());
    airway_names_.push_back(name);
    name_to_id[name] = id;
    return id;
  };

  // --- 邻接表 ---
  std::vector<std::vector<GraphEdge>> adj(total);

  auto add_edge = [&](int from, int to, int airway_id, const RawSegment& s) {
    double dist = graph_.coords_[from].DistanceTo(graph_.coords_[to]);
    adj[from].push_back(
        GraphEdge{to, static_cast<float>(dist), static_cast<uint16_t>(airway_id),
                  static_cast<int16_t>(s.base_fl), static_cast<int16_t>(s.top_fl),
                  s.level});
  };

  for (const auto& seg : segments) {
    int from_v = VertexByIdent(Ident{seg.from_ident, seg.from_region});
    int to_v = VertexByIdent(Ident{seg.to_ident, seg.to_region});
    if (from_v < 0 || to_v < 0) continue;

    int id = airway_id_for(seg.airway);

    // 方向编码
    if (seg.direction != AirwayDirection::kBackward) {
      add_edge(from_v, to_v, id, seg);
    }
    if (seg.direction != AirwayDirection::kForward) {
      add_edge(to_v, from_v, id, seg);
    }
  }

  // --- 方向性标志（基于航路边，不含后续可能加的 DCT 边）---
  has_outbound_.assign(total, false);
  has_inbound_.assign(total, false);
  for (int v = 0; v < total; ++v) {
    if (!adj[v].empty()) {
      has_outbound_[v] = true;
      for (const auto& e : adj[v]) {
        has_inbound_[e.to] = true;
      }
    }
  }

  // --- 展平邻接表为 CSR ---
  graph_.offsets_.resize(total + 1, 0);
  for (int v = 0; v < total; ++v) {
    graph_.offsets_[v + 1] =
        graph_.offsets_[v] + static_cast<int>(adj[v].size());
  }
  graph_.edges_.reserve(graph_.offsets_[total]);
  for (int v = 0; v < total; ++v) {
    graph_.edges_.insert(graph_.edges_.end(), adj[v].begin(), adj[v].end());
  }
}

const NavGraph& GraphBuilder::graph() const { return graph_; }

const Ident& GraphBuilder::IdentOf(int vertex) const { return idents_[vertex]; }

const std::string& GraphBuilder::AirwayName(int airway_id) const {
  return airway_names_[airway_id];
}

int GraphBuilder::AirwayCount() const {
  return static_cast<int>(airway_names_.size());
}

bool GraphBuilder::HasOutbound(int vertex) const {
  return vertex >= 0 && vertex < static_cast<int>(has_outbound_.size()) &&
         has_outbound_[vertex];
}

bool GraphBuilder::HasInbound(int vertex) const {
  return vertex >= 0 && vertex < static_cast<int>(has_inbound_.size()) &&
         has_inbound_[vertex];
}

int GraphBuilder::VertexByIdent(const Ident& key) const {
  auto it = ident_index_.find(key);
  return (it != ident_index_.end()) ? it->second : -1;
}

int GraphBuilder::VertexByAirport(const std::string& icao) const {
  auto it = airport_index_.find(icao);
  return (it != airport_index_.end()) ? it->second : -1;
}

}  // namespace px
