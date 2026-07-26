#include <catch2/catch_test_macros.hpp>
#include <chrono>
#include <cstdlib>
#include <string>

#include "altitude_constraints.h"
#include "graph_builder.h"
#include "px/module/router/astar/astar.h"
#include "px/core/constraint.h"
#include "px/core/route_query.h"

namespace px {
namespace {

std::vector<RawWaypoint> ChainWpts() {
  return {{"A", "XX", 0.0, 0.0},
          {"B", "XX", 0.0, 1.0},
          {"C", "XX", 0.0, 2.0},
          {"D", "XX", 0.0, 3.0}};
}
std::vector<RawSegment> ChainSegs() {
  return {
      {"A", "XX", "B", "XX", "V1", AirwayDirection::kBoth, AirwayLevel::kHigh,
       100, 400},
      {"B", "XX", "C", "XX", "V1", AirwayDirection::kBoth, AirwayLevel::kHigh,
       100, 400},
      {"C", "XX", "D", "XX", "V1", AirwayDirection::kBoth, AirwayLevel::kHigh,
       100, 400},
  };
}

TEST_CASE("A* 线形链 A→D") {
  GraphBuilder b(ChainWpts(), ChainSegs());
  auto p = FindShortestPath(b.graph(), 0, 3);
  REQUIRE(p.found);
  REQUIRE(p.vertices.size() == 4);
  REQUIRE(p.vertices[0] == 0);
  REQUIRE(p.vertices[3] == 3);
  REQUIRE(p.cost == p.distance_nm);
}

TEST_CASE("A* 起点等于终点") {
  GraphBuilder b(ChainWpts(), ChainSegs());
  auto p = FindShortestPath(b.graph(), 0, 0);
  REQUIRE(p.found);
  REQUIRE(p.vertices.size() == 1);
}

TEST_CASE("A* 不可达") {
  std::vector<RawWaypoint> wpts = {{"A", "XX", 0.0, 0.0},
                                   {"B", "XX", 10.0, 0.0}};
  GraphBuilder b(wpts, {});
  auto p = FindShortestPath(b.graph(), 0, 1);
  REQUIRE(!p.found);
}

TEST_CASE("A* 高度约束阻断") {
  GraphBuilder b(ChainWpts(), ChainSegs());
  AltitudeBandConstraint alt;
  SearchOptions opts;
  opts.constraints.push_back(&alt);
  RouteQuery q;
  q.altitude = FlRange{10, 30};
  opts.request = &q;
  REQUIRE(!FindShortestPath(b.graph(), 0, 3, opts).found);
}

TEST_CASE("A* 高度约束放行") {
  GraphBuilder b(ChainWpts(), ChainSegs());
  AltitudeBandConstraint alt;
  SearchOptions opts;
  opts.constraints.push_back(&alt);
  RouteQuery q;
  q.altitude = FlRange{200, 300};
  opts.request = &q;
  REQUIRE(FindShortestPath(b.graph(), 0, 3, opts).found);
}

TEST_CASE("A* 层级偏好罚分") {
  GraphBuilder b(ChainWpts(), ChainSegs());
  LevelPreferenceConstraint lvl(1.0);
  SearchOptions opts;
  opts.constraints.push_back(&lvl);
  RouteQuery q;
  q.level = LevelPreference::kLow;
  opts.request = &q;
  auto p = FindShortestPath(b.graph(), 0, 3, opts);
  REQUIRE(p.found);
  REQUIRE(p.cost > p.distance_nm);
}

// ===================================================================
// 正确性：A* vs Dijkstra (h=0, 保证最优)
// ===================================================================

TEST_CASE("A* vs Dijkstra 代价一致") {
  // 6 顶点图含多条不同代价路径
  std::vector<RawWaypoint> wpts = {
      {"0", "XX", 0.0, 0.0}, {"1", "XX", 1.0, 0.0}, {"2", "XX", 2.0, 0.0},
      {"3", "XX", 0.0, 1.0}, {"4", "XX", 1.0, 1.0}, {"5", "XX", 2.0, 1.0},
  };
  std::vector<RawSegment> segs = {
      {"0", "XX", "1", "XX", "V1", AirwayDirection::kBoth, AirwayLevel::kHigh,
       0, 999},
      {"1", "XX", "2", "XX", "V1", AirwayDirection::kBoth, AirwayLevel::kHigh,
       0, 999},
      {"0", "XX", "3", "XX", "V1", AirwayDirection::kBoth, AirwayLevel::kHigh,
       0, 999},
      {"3", "XX", "4", "XX", "V1", AirwayDirection::kBoth, AirwayLevel::kHigh,
       0, 999},
      {"4", "XX", "5", "XX", "V1", AirwayDirection::kBoth, AirwayLevel::kHigh,
       0, 999},
      {"1", "XX", "4", "XX", "V1", AirwayDirection::kBoth, AirwayLevel::kHigh,
       0, 999},
      {"2", "XX", "5", "XX", "V1", AirwayDirection::kBoth, AirwayLevel::kHigh,
       0, 999},
      {"0", "XX", "4", "XX", "V1", AirwayDirection::kBoth, AirwayLevel::kHigh,
       0, 999},
  };
  GraphBuilder b(wpts, segs);
  const auto& g = b.graph();

  // Dijkstra: heuristic = 0（退化为无启发式，保证最优）
  SearchOptions d_opts;
  d_opts.heuristic = [&](int) { return 0.0; };

  // A*: 默认大圆距离启发式
  SearchOptions a_opts;

  // 对图中 15 对起终点逐一验证代价相同
  for (int s = 0; s < 6; ++s) {
    for (int t = 0; t < 6; ++t) {
      if (s == t) continue;
      auto dijk = FindShortestPath(g, s, t, d_opts);
      auto astr = FindShortestPath(g, s, t, a_opts);

      INFO("s=" << s << " t=" << t);
      REQUIRE(dijk.found == astr.found);
      if (dijk.found) {
        REQUIRE(dijk.cost == astr.cost);
      }
    }
  }
}

// ===================================================================
// Benchmark: A* 延迟 → 预测 Yen 开销（Phase 3.5）
// ===================================================================

// 生成网格状合成图（模拟真实航路拓扑）。
// V 顶点排列为 sqrt(V)×sqrt(V) 网格，每顶点连 4 邻居（双向）。
std::pair<std::vector<RawWaypoint>, std::vector<RawSegment>> MakeGridGraph(
    int side) {
  int V = side * side;
  std::vector<RawWaypoint> wpts;
  for (int r = 0; r < side; ++r)
    for (int c = 0; c < side; ++c)
      wpts.push_back(
          {"W" + std::to_string(r * side + c), "XX", r * 0.5, c * 0.5});

  std::vector<RawSegment> segs;
  for (int r = 0; r < side; ++r) {
    for (int c = 0; c < side; ++c) {
      int v = r * side + c;
      if (c + 1 < side)
        segs.push_back({"W" + std::to_string(v), "XX",
                        "W" + std::to_string(v + 1), "XX", "V1",
                        AirwayDirection::kBoth, AirwayLevel::kHigh, 0, 999});
      if (r + 1 < side)
        segs.push_back({"W" + std::to_string(v), "XX",
                        "W" + std::to_string(v + side), "XX", "V1",
                        AirwayDirection::kBoth, AirwayLevel::kHigh, 0, 999});
    }
  }
  return {wpts, segs};
}

// 运行 N 次 A*，返回平均耗时（ms）。
double BenchmarkAStar(const NavGraph& g, int start, int goal,
                      const SearchOptions& opts, int runs) {
  auto t0 = std::chrono::steady_clock::now();
  for (int i = 0; i < runs; ++i) FindShortestPath(g, start, goal, opts);
  auto t1 = std::chrono::steady_clock::now();
  return std::chrono::duration<double, std::milli>(t1 - t0).count() / runs;
}

// 第一轮：5K 顶点合成图
TEST_CASE("Benchmark: A* 5K 网格图") {
  auto [wpts, segs] = MakeGridGraph(71);  // 71×71 ≈ 5K 顶点，~10K 边
  GraphBuilder b(wpts, segs);
  const auto& g = b.graph();
  int V = g.VertexCount();

  // 对角最远端
  double avg = BenchmarkAStar(g, 0, V - 1, SearchOptions{}, 50);
  INFO("5K grid A* avg " << avg << " ms/call");

  // 预估: A*_单次 × 300
  double yen_est = avg * 300.0;
  INFO("Estimated Yen(k=5): " << yen_est
                              << " ms (threshold 50 ms → need Lawler)");
}

// 第二轮：270K 真实规模
TEST_CASE("Benchmark: A* 270K 图") {
  // 复用 Phase 2 真实规模图的生成逻辑
  constexpr int V = 270000;
  constexpr int chain_edges = V - 1;
  constexpr int cross_edges = 80000;

  std::vector<RawWaypoint> wpts;
  wpts.reserve(V);
  for (int i = 0; i < V; ++i)
    wpts.push_back(
        {"W" + std::to_string(i), "XX", (i % 180) - 90.0, (i / 180) - 180.0});

  std::vector<RawSegment> segs;
  segs.reserve(chain_edges + cross_edges);
  for (int i = 0; i < chain_edges; ++i)
    segs.push_back({"W" + std::to_string(i), "XX", "W" + std::to_string(i + 1),
                    "XX", "J1", AirwayDirection::kBoth, AirwayLevel::kHigh, 100,
                    400});

  std::srand(42);
  for (int k = 0; k < cross_edges; ++k) {
    int i = std::rand() % (V - 100);
    int j = i + 10 + std::rand() % 90;
    segs.push_back({"W" + std::to_string(i), "XX", "W" + std::to_string(j),
                    "XX", "V" + std::to_string(k % 500),
                    AirwayDirection::kForward, AirwayLevel::kHigh, 0, 999});
  }

  GraphBuilder b(wpts, segs);
  const auto& g = b.graph();

  // 从第一个顶点到最后有出边的顶点
  double avg = BenchmarkAStar(g, 0, V - 1, SearchOptions{}, 10);
  INFO("270K chain+cross A* avg " << avg << " ms/call");

  double yen_est = avg * 300.0;
  INFO("Estimated Yen(k=5): " << yen_est << " ms");
  INFO("Threshold: 50 ms => "
       << (yen_est < 50.0 ? "naive Yen OK" : "need Lawler"));
}

TEST_CASE("A* node_blocked 封禁起点") {
  GraphBuilder b(ChainWpts(), ChainSegs());
  SearchOptions opts;
  opts.node_blocked = [](int v) { return v == 0; };
  auto p = FindShortestPath(b.graph(), 0, 3, opts);
  REQUIRE(!p.found);
}

TEST_CASE("A* edge_blocked 封禁关键边迫使绕路") {
  GraphBuilder b(ChainWpts(), ChainSegs());
  SearchOptions opts;
  // 封禁 A→B，迫使 A* 无法前进
  opts.edge_blocked = [](int from, int to) { return from == 0 && to == 1; };
  auto p = FindShortestPath(b.graph(), 0, 3, opts);
  // 链形图中 A 只有一条出边 A→B，封禁后不可达
  REQUIRE(!p.found);
}

}  // namespace
}  // namespace px
