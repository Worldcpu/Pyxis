#include "graph_builder.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <limits>
#include <unordered_map>

namespace px {

namespace {

// DCT 连接：每个机场最多连接到 N 个最近航路点。
constexpr int kMaxDctPerAirport = 5;

}  // namespace

GraphBuilder::GraphBuilder(const std::vector<RawWaypoint>& waypoints,
                           const std::vector<RawSegment>& segments,
                           const std::vector<RawAirport>& airports) {
  const int w_count = static_cast<int>(waypoints.size());
  const int a_count = static_cast<int>(airports.size());
  const int total = w_count + a_count;

  // --- 顶点：坐标 + ident + kind ---
  graph_.coords_.reserve(total);
  idents_.reserve(total);
  kinds_.reserve(total);
  for (int i = 0; i < w_count; ++i) {
    const auto& w = waypoints[i];
    graph_.coords_.push_back(Coordinate{w.latitude, w.longitude});
    Ident key{w.ident, w.region};
    idents_.push_back(key);
    kinds_.push_back(w.kind);
    ident_index_[key] = i;
  }
  for (int i = 0; i < a_count; ++i) {
    const auto& ap = airports[i];
    const int v = w_count + i;
    graph_.coords_.push_back(Coordinate{ap.latitude, ap.longitude});
    idents_.push_back(Ident{ap.icao, ""});
    kinds_.push_back(WaypointKind::kFix);
    airport_index_[ap.icao] = v;
  }

  // --- 航路名表："DCT" 保留在索引 0 ---
  airway_names_.push_back("DCT");
  std::unordered_map<std::string, int> name_to_id;
  name_to_id["DCT"] = 0;

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
    adj[from].push_back(GraphEdge{to, static_cast<float>(dist),
                                  static_cast<uint16_t>(airway_id),
                                  static_cast<int16_t>(s.base_fl),
                                  static_cast<int16_t>(s.top_fl), s.level});
  };

  // 航路边（仅涉及航路点顶点，0 到 w_count-1）。
  for (const auto& seg : segments) {
    int from_v = VertexByIdent(Ident{seg.from_ident, seg.from_region});
    int to_v = VertexByIdent(Ident{seg.to_ident, seg.to_region});
    if (from_v < 0 || to_v < 0) continue;

    int id = airway_id_for(seg.airway);

    if (seg.direction != AirwayDirection::kBackward) {
      add_edge(from_v, to_v, id, seg);
    }
    if (seg.direction != AirwayDirection::kForward) {
      add_edge(to_v, from_v, id, seg);
    }
  }

  // --- 方向性标志（仅基于航路边，不包含后续 DCT 边） ---
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

  // --- DCT 边：机场 → 最近 N 个航路点，双向 ---
  // 使用 1°×1° 空间网格。参考 bravofinder GraphBuilder DegreeGrid 设计。
  if (a_count > 0 && w_count > 0) {
    static constexpr int kLatBins = 180;
    static constexpr int kLonBins = 360;
    static constexpr int kMaxDctPerAirport = 2;

    std::vector<std::vector<int>> grid(kLatBins * kLonBins);
    for (int wi = 0; wi < w_count; ++wi) {
      int r = static_cast<int>(std::floor(graph_.coords_[wi].latitude + 90.0));
      int c = static_cast<int>(std::floor(graph_.coords_[wi].longitude + 180.0));
      if (r < 0) r = 0; else if (r >= kLatBins) r = kLatBins - 1;
      if (c < 0) c = 0; else if (c >= kLonBins) c = kLonBins - 1;
      grid[r * kLonBins + c].push_back(wi);
    }

    struct DistPair { int vertex; double dist_nm; };

    for (int ai = 0; ai < a_count; ++ai) {
      const int airport_v = w_count + ai;
      const Coordinate& ap = graph_.coords_[airport_v];
      const int cr = static_cast<int>(std::floor(ap.latitude + 90.0));
      const int cc = static_cast<int>(std::floor(ap.longitude + 180.0));

      std::vector<DistPair> candidates;
      // 搜索 3×3 → 5×5 邻域，直到找到足够候选
      for (int radius = 1; radius <= 2; ++radius) {
        candidates.clear();
        for (int dr = -radius; dr <= radius; ++dr) {
          for (int dc = -radius; dc <= radius; ++dc) {
            int r = cr + dr, c = cc + dc;
            if (r < 0 || r >= kLatBins || c < 0 || c >= kLonBins) continue;
            for (int wi : grid[r * kLonBins + c]) {
              candidates.push_back({wi, ap.DistanceTo(graph_.coords_[wi])});
            }
          }
        }
        if (static_cast<int>(candidates.size()) >= kMaxDctPerAirport) break;
      }

      const int n = std::min(kMaxDctPerAirport,
                             static_cast<int>(candidates.size()));
      if (n == 0) continue;
      std::partial_sort(candidates.begin(), candidates.begin() + n,
                        candidates.end(),
                        [](const DistPair& a, const DistPair& b) {
                          return a.dist_nm < b.dist_nm;
                        });

      for (int j = 0; j < n; ++j) {
        const int wp_v = candidates[j].vertex;
        const float d = static_cast<float>(candidates[j].dist_nm);
        adj[airport_v].push_back(GraphEdge{wp_v, d, 0, 0, 0, AirwayLevel::kBoth});
        adj[wp_v].push_back(GraphEdge{airport_v, d, 0, 0, 0, AirwayLevel::kBoth});
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

WaypointKind GraphBuilder::KindOf(int vertex) const { return kinds_[vertex]; }

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
GraphBuilder::GraphBuilder(std::vector<Coordinate> coords,
                           std::vector<int> offsets,
                           std::vector<GraphEdge> edges,
                           std::vector<Ident> idents,
                           std::vector<std::string> airway_names,
                           std::vector<WaypointKind> kinds,
                           std::vector<bool> has_outbound,
                           std::vector<bool> has_inbound)
    : idents_(std::move(idents)),
      kinds_(std::move(kinds)),
      has_outbound_(std::move(has_outbound)),
      has_inbound_(std::move(has_inbound)),
      airway_names_(std::move(airway_names)) {
  const int total = static_cast<int>(coords.size());
  graph_.coords_ = std::move(coords);
  graph_.offsets_ = std::move(offsets);
  graph_.edges_ = std::move(edges);

  // 重建 lookup 索引
  for (int i = 0; i < total; ++i) {
    ident_index_[idents_[i]] = i;
  }
}

}  // namespace px