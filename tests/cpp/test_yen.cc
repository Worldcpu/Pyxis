#include <catch2/catch_test_macros.hpp>
#include <chrono>
#include <cstdlib>
#include <random>
#include <set>
#include <string>

#include "altitude_constraints.h"
#include "bravo_yen.h"
#include "graph_builder.h"
#include "px/core/astar.h"
#include "px/module/router/yen.h"

namespace px {
namespace {

// ── 测试图 ──

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

std::vector<RawWaypoint> DiamondWpts() {
  return {{"A", "XX", 0.0, 2.0},
          {"B", "XX", 1.0, 1.0},
          {"C", "XX", -1.0, 1.0},
          {"D", "XX", 0.0, 0.0}};
}
std::vector<RawSegment> DiamondSegs() {
  return {
      {"A", "XX", "B", "XX", "V1", AirwayDirection::kBoth, AirwayLevel::kHigh,
       0, 999},
      {"A", "XX", "C", "XX", "V2", AirwayDirection::kBoth, AirwayLevel::kHigh,
       0, 999},
      {"B", "XX", "D", "XX", "V1", AirwayDirection::kBoth, AirwayLevel::kHigh,
       0, 999},
      {"C", "XX", "D", "XX", "V2", AirwayDirection::kBoth, AirwayLevel::kHigh,
       0, 999},
  };
}

// ── k=1 vs A* ──

TEST_CASE("Yen: k=1 与 A* 一致") {
  GraphBuilder b(ChainWpts(), ChainSegs());
  auto yen = FindKShortestPaths(b.graph(), 0, 3, 1, YenOptions{});
  REQUIRE(yen.has_value());
  REQUIRE(yen->size() == 1);

  auto a = FindShortestPath(b.graph(), 0, 3);
  REQUIRE((*yen)[0].cost == a.cost);
  REQUIRE((*yen)[0].vertices == a.vertices);
}

// ── 备选路径数 ──

TEST_CASE("Yen: 菱形 k=4 仅 2 条") {
  GraphBuilder b(DiamondWpts(), DiamondSegs());
  auto yen = FindKShortestPaths(b.graph(), 0, 3, 4, YenOptions{});
  REQUIRE(yen.has_value());
  REQUIRE(yen->size() == 2);
}

TEST_CASE("Yen: 菱形路径按代价升序") {
  GraphBuilder b(DiamondWpts(), DiamondSegs());
  auto yen = FindKShortestPaths(b.graph(), 0, 3, 2, YenOptions{});
  REQUIRE(yen.has_value());
  REQUIRE(yen->size() == 2);
  REQUIRE((*yen)[0].cost <= (*yen)[1].cost);
}

TEST_CASE("Yen: 不可达返回空") {
  std::vector<RawWaypoint> w = {{"A", "XX", 0.0, 0.0}, {"B", "XX", 10.0, 0.0}};
  GraphBuilder b(w, {});
  auto yen = FindKShortestPaths(b.graph(), 0, 1, 3, YenOptions{});
  REQUIRE(yen.has_value());
  REQUIRE(yen->empty());
}

TEST_CASE("Yen: 起点即终点") {
  GraphBuilder b(ChainWpts(), ChainSegs());
  auto yen = FindKShortestPaths(b.graph(), 0, 0, 3, YenOptions{});
  REQUIRE(yen.has_value());
  REQUIRE(yen->size() == 1);
  REQUIRE((*yen)[0].vertices.size() == 1);
}

// ── k 校验 ──

TEST_CASE("Yen: k=0 错误") {
  GraphBuilder b(ChainWpts(), ChainSegs());
  auto yen = FindKShortestPaths(b.graph(), 0, 3, 0, YenOptions{});
  REQUIRE_FALSE(yen.has_value());
  REQUIRE(yen.error().code == ErrorCode::kInvalidInput);
}

TEST_CASE("Yen: k=9 错误") {
  GraphBuilder b(ChainWpts(), ChainSegs());
  auto yen = FindKShortestPaths(b.graph(), 0, 3, 9, YenOptions{});
  REQUIRE_FALSE(yen.has_value());
  REQUIRE(yen.error().code == ErrorCode::kInvalidInput);
}

// ── 约束 ──

TEST_CASE("Yen: 高度约束阻断全部") {
  std::vector<RawWaypoint> w = {{"A", "XX", 0.0, 0.0},
                                {"B", "XX", 0.0, 1.0},
                                {"C", "XX", 0.0, 2.0},
                                {"D", "XX", 0.0, 3.0}};
  std::vector<RawSegment> s = {
      {"A", "XX", "B", "XX", "V1", AirwayDirection::kBoth, AirwayLevel::kHigh,
       100, 200},
      {"B", "XX", "C", "XX", "V2", AirwayDirection::kBoth, AirwayLevel::kHigh,
       300, 400},
      {"C", "XX", "D", "XX", "V3", AirwayDirection::kBoth, AirwayLevel::kHigh,
       100, 200},
  };
  GraphBuilder b(w, s);

  AltitudeBandConstraint alt;
  YenOptions opts;
  opts.search.constraints.push_back(&alt);
  RouteQuery q;
  q.altitude = FlRange{350, 370};
  opts.search.request = &q;

  auto yen = FindKShortestPaths(b.graph(), 0, 3, 3, opts);
  REQUIRE(yen.has_value());
  REQUIRE(yen->empty());
}

// ── edges/cumulative_cost ──

TEST_CASE("Yen: 边和累积代价正确") {
  GraphBuilder b(ChainWpts(), ChainSegs());
  auto yen = FindKShortestPaths(b.graph(), 0, 3, 1, YenOptions{});
  REQUIRE(yen.has_value());
  const auto& p = (*yen)[0];

  REQUIRE(p.edges.size() == 3);
  REQUIRE(p.cumulative_cost.size() == p.vertices.size());
  REQUIRE(p.cumulative_cost[0] == 0.0);
  for (size_t i = 1; i < p.cumulative_cost.size(); ++i)
    REQUIRE(p.cumulative_cost[i] >= p.cumulative_cost[i - 1]);
}

// ── CancelToken ──

TEST_CASE("Yen: 取消令牌") {
  GraphBuilder b(ChainWpts(), ChainSegs());
  CancelToken ct;
  ct.Cancel();

  YenOptions opts;
  opts.cancel = &ct;
  auto yen = FindKShortestPaths(b.graph(), 0, 3, 3, opts);
  REQUIRE(yen.has_value());
  REQUIRE(yen->size() <= 1);
}

// ── 无环 + 有序 ──

TEST_CASE("Yen: 6 顶点图无环且有序") {
  std::vector<RawWaypoint> w = {
      {"0", "XX", 0.0, 0.0}, {"1", "XX", 1.0, 0.0}, {"2", "XX", 2.0, 0.0},
      {"3", "XX", 0.0, 1.0}, {"4", "XX", 1.0, 1.0}, {"5", "XX", 2.0, 1.0},
  };
  std::vector<RawSegment> s = {
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
  GraphBuilder b(w, s);

  auto yen = FindKShortestPaths(b.graph(), 0, 5, 3, YenOptions{});
  REQUIRE(yen.has_value());
  REQUIRE(yen->size() >= 1);

  for (const auto& p : *yen) {
    std::set<int> seen(p.vertices.begin(), p.vertices.end());
    REQUIRE(seen.size() == p.vertices.size());
  }
  for (size_t i = 1; i < yen->size(); ++i)
    REQUIRE((*yen)[i - 1].cost <= (*yen)[i].cost);
}

// ── 10K Benchmark ──

TEST_CASE("Benchmark: Yen 10K k=3") {
  constexpr int V = 10000;
  constexpr int chain_edges = V - 1;
  constexpr int cross_edges = 5000;

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

  auto t0 = std::chrono::steady_clock::now();
  auto yen = FindKShortestPaths(g, 0, V - 1, 3, YenOptions{});
  auto t1 = std::chrono::steady_clock::now();
  auto ms =
      std::chrono::duration_cast<std::chrono::milliseconds>(t1 - t0).count();

  REQUIRE(yen.has_value());
  REQUIRE_FALSE(yen->empty());

  INFO("10K Yen(k=3): " << ms << " ms, paths=" << yen->size());
  CHECK(ms < 30000);
}

// ===================================================================
// bravofinder 测试套件移植
// ===================================================================

// ── 辅助函数：构建 r 行 c 列的网格图 ──
// 对照 bravofinder tests/unit/yen_test.cc MakeLatticeData
std::pair<std::vector<RawWaypoint>, std::vector<RawSegment>> MakeLattice(
    int cols, int rows) {
  std::vector<RawWaypoint> wpts;
  std::vector<RawSegment> segs;
  auto name = [](int c, int r) {
    return "L" + std::to_string(c) + "r" + std::to_string(r);
  };
  for (int c = 0; c < cols; ++c) {
    for (int r = 0; r < rows; ++r) {
      double lat = (r - (rows - 1) / 2.0) * 0.2;
      wpts.push_back({name(c, r), "ZZ", lat, static_cast<double>(c)});
    }
  }
  for (int c = 0; c + 1 < cols; ++c) {
    for (int r = 0; r < rows; ++r) {
      for (int r2 = 0; r2 < rows; ++r2) {
        segs.push_back({name(c, r), "ZZ", name(c + 1, r2), "ZZ",
                        "A" + std::to_string(c), AirwayDirection::kBoth,
                        AirwayLevel::kHigh, 0, 999});
      }
    }
  }
  return {wpts, segs};
}

// ── Lattice golden signature ──
// 对照 bravofinder "Yen golden candidate sequence on a lattice"
TEST_CASE("Yen: 网格图 golden signature (Lawler 回归)") {
  auto [w, s] = MakeLattice(4, 3);
  GraphBuilder b(w, s);
  // L0r1 → L3r1，顶点索引: 行优先 (c=0,r=0)=0, (0,1)=1, (0,2)=2, (1,0)=3...
  // L0r1 = 1, L3r1 = 10
  const int start = 1;
  const int goal = 10;
  REQUIRE(b.IdentOf(start).ident == "L0r1");
  REQUIRE(b.IdentOf(goal).ident == "L3r1");

  auto yen = FindKShortestPaths(b.graph(), start, goal, 8, YenOptions{});
  REQUIRE(yen.has_value());
  REQUIRE(yen->size() >= 5);

  std::set<std::vector<int>> seen;
  for (size_t i = 0; i < yen->size(); ++i) {
    CHECK((*yen)[i].found);
    if (i > 0) CHECK((*yen)[i - 1].cost <= (*yen)[i].cost);
    CHECK(seen.insert((*yen)[i].vertices).second);
  }

  // Golden signature: cost (×1e6 取整) + 顶点序列
  // 对照 bravofinder 的 kGolden
  std::string sig;
  for (const auto& p : *yen) {
    sig +=
        "cost=" + std::to_string(static_cast<long long>(p.cost * 1e6)) + " [";
    for (int v : p.vertices) sig += std::to_string(v) + ",";
    sig += "]\n";
  }
  static const std::string kGolden =
      "cost=180121616 [1,4,7,10,]\n"
      "cost=182499088 [1,3,6,10,]\n"
      "cost=182499088 [1,5,8,10,]\n"
      "cost=182499454 [1,3,7,10,]\n"
      "cost=182499454 [1,4,6,10,]\n"
      "cost=182499454 [1,4,8,10,]\n"
      "cost=182499454 [1,5,7,10,]\n"
      "cost=187124443 [1,3,8,10,]\n";
  CHECK(sig == kGolden);
}

// ── 朴素 Yen（无 Lawler，基准实现）──
// 对照 bravofinder NaiveKShortest

struct RefCandidate {
  double cost;
  double distance;
  std::vector<int> vertices;
  bool operator<(const RefCandidate& o) const {
    if (cost != o.cost) return cost < o.cost;
    return vertices < o.vertices;
  }
};

bool RefCostOfPath(const NavGraph& g, const std::vector<int>& path,
                   double& cost, double& dist) {
  cost = 0.0;
  dist = 0.0;
  for (size_t i = 0; i + 1 < path.size(); ++i) {
    const GraphEdge* found = nullptr;
    for (auto* e = g.EdgesBegin(path[i]); e != g.EdgesEnd(path[i]); ++e) {
      if (e->to == path[i + 1]) {
        found = e;
        break;
      }
    }
    if (!found) return false;
    cost += found->distance_nm;
    dist += found->distance_nm;
  }
  return true;
}

std::vector<ShortestPath> NaiveKShortest(const NavGraph& graph, int start,
                                         int goal, int k) {
  std::vector<ShortestPath> result;
  if (k <= 0) return result;
  ShortestPath first = FindShortestPath(graph, start, goal);
  if (!first.found) return result;
  result.push_back(std::move(first));
  std::set<RefCandidate> candidates;
  for (int kth = 1; kth < k; ++kth) {
    const auto prev = result.back().vertices;
    for (size_t i = 0; i + 1 < prev.size(); ++i) {
      const std::vector<int> root(prev.begin(), prev.begin() + i + 1);
      std::set<std::pair<int, int>> banned_edges;
      for (const auto& p : result) {
        if (p.vertices.size() > i + 1 &&
            std::equal(root.begin(), root.end(), p.vertices.begin())) {
          banned_edges.emplace(p.vertices[i], p.vertices[i + 1]);
        }
      }
      const std::set<int> banned_nodes(root.begin(), root.end() - 1);
      SearchOptions opts;
      opts.node_blocked = [&banned_nodes](int v) {
        return banned_nodes.count(v) != 0;
      };
      opts.edge_blocked = [&banned_edges](int f, int t) {
        return banned_edges.count({f, t}) != 0;
      };
      const ShortestPath spur = FindShortestPath(graph, prev[i], goal, opts);
      if (!spur.found) continue;
      std::vector<int> total(root.begin(), root.end() - 1);
      total.insert(total.end(), spur.vertices.begin(), spur.vertices.end());
      double cost = 0.0, dist = 0.0;
      if (RefCostOfPath(graph, total, cost, dist))
        candidates.insert({cost, dist, std::move(total)});
    }
    if (candidates.empty()) break;
    auto best = candidates.begin();
    ShortestPath next;
    next.vertices = best->vertices;
    next.cost = best->cost;
    next.distance_nm = best->distance;
    next.found = true;
    candidates.erase(best);
    result.push_back(std::move(next));
  }
  return result;
}

// ── 随机图生成 ──
// 对照 bravofinder MakeRandomData
std::pair<std::vector<RawWaypoint>, std::vector<RawSegment>> MakeRandom(
    std::mt19937& rng, int n, int extra_edges) {
  std::vector<RawWaypoint> w;
  std::uniform_real_distribution<double> lat(-2.0, 2.0);
  std::uniform_real_distribution<double> lon(0.0, 6.0);
  for (int i = 0; i < n; ++i)
    w.push_back({"W" + std::to_string(i), "ZZ", lat(rng), lon(rng)});
  std::vector<RawSegment> s;
  for (int i = 0; i + 1 < n; ++i)
    s.push_back({"W" + std::to_string(i), "ZZ", "W" + std::to_string(i + 1),
                 "ZZ", "R" + std::to_string(i), AirwayDirection::kBoth,
                 AirwayLevel::kHigh, 0, 999});
  std::uniform_int_distribution<int> pick(0, n - 1);
  for (int e = 0; e < extra_edges; ++e) {
    int a = pick(rng), b = pick(rng);
    if (a != b)
      s.push_back({"W" + std::to_string(a), "ZZ", "W" + std::to_string(b), "ZZ",
                   "R" + std::to_string(n + e), AirwayDirection::kBoth,
                   AirwayLevel::kHigh, 0, 999});
  }
  return {w, s};
}

// ── Lawler vs 朴素 Yen（200 个随机图）──
// 对照 bravofinder "Lawler matches naive Yen on hundreds of random graphs"
TEST_CASE("Yen: Lawler vs 朴素 Yen (200 随机图)") {
  std::mt19937 rng(0xB4A0);
  std::uniform_int_distribution<int> n_dist(3, 15);
  int checked = 0;
  for (int t = 0; t < 200; ++t) {
    int n = n_dist(rng);
    auto [w, s] = MakeRandom(rng, n, n);
    GraphBuilder b(w, s);
    std::uniform_int_distribution<int> vpick(0, n - 1);
    int start = vpick(rng), goal = vpick(rng);
    if (start == goal) continue;
    int k = 1 + (t % 8);  // k ∈ [1, 8]

    auto opt = FindKShortestPaths(b.graph(), start, goal, k, YenOptions{});
    auto ref = NaiveKShortest(b.graph(), start, goal, k);
    REQUIRE(opt.has_value());
    REQUIRE(opt->size() == ref.size());
    for (size_t i = 0; i < opt->size(); ++i) {
      INFO("trial=" << t << " i=" << i);
      CHECK((*opt)[i].vertices == ref[i].vertices);
      CHECK((*opt)[i].cost == ref[i].cost);
    }
    ++checked;
  }
  CHECK(checked > 150);
  INFO("checked " << checked << " random graphs");
}

// ── Pyxis vs bravo (CostOfPath+SelectEdge) 对比 ──
// 验证边序列存储方案与 bravofinder 原版输出完全一致
TEST_CASE("Yen: Pyxis vs bravo Yen (200 随机图)") {
  std::mt19937 rng(0xC0DE);
  std::uniform_int_distribution<int> n_dist(3, 15);
  int checked = 0;
  for (int t = 0; t < 200; ++t) {
    int n = n_dist(rng);
    auto [w, s] = MakeRandom(rng, n, n);
    GraphBuilder b(w, s);
    std::uniform_int_distribution<int> vpick(0, n - 1);
    int start = vpick(rng), goal = vpick(rng);
    if (start == goal) continue;
    int k = 1 + (t % 8);

    // Pyxis: 边序列 + O(1) cumulative_cost
    auto pyxis = FindKShortestPaths(b.graph(), start, goal, k, YenOptions{});
    // bravo: CostOfPath + SelectEdge（bravofinder 原版逻辑）
    auto bravo =
        bravo::FindKShortestPaths(b.graph(), start, goal, k, SearchOptions{});

    REQUIRE(pyxis.has_value());
    REQUIRE(pyxis->size() == bravo.size());
    for (size_t i = 0; i < pyxis->size(); ++i) {
      INFO("trial=" << t << " i=" << i);
      CHECK((*pyxis)[i].vertices == bravo[i].vertices);
      CHECK((*pyxis)[i].cost == bravo[i].cost);
    }
    ++checked;
  }
  CHECK(checked > 150);
  INFO("Pyxis vs bravo: " << checked << " random graphs");
}

// ── Pyxis vs bravo 性能对比 ──
TEST_CASE("Benchmark: Pyxis vs bravo 2K k=5") {
  constexpr int V = 2000;
  constexpr int extra = 1000;
  // 确定性随机图（固定种子）
  std::mt19937 rng(42);
  std::uniform_real_distribution<double> lat(-2.0, 2.0);
  std::uniform_real_distribution<double> lon(0.0, 6.0);
  std::vector<RawWaypoint> w;
  for (int i = 0; i < V; ++i)
    w.push_back({"W" + std::to_string(i), "ZZ", lat(rng), lon(rng)});
  std::vector<RawSegment> s;
  for (int i = 0; i + 1 < V; ++i)
    s.push_back({"W" + std::to_string(i), "ZZ", "W" + std::to_string(i + 1),
                 "ZZ", "R0", AirwayDirection::kBoth, AirwayLevel::kHigh, 0,
                 999});
  std::uniform_int_distribution<int> pick(0, V - 1);
  for (int e = 0; e < extra; ++e) {
    int a = pick(rng), b = pick(rng);
    if (a != b)
      s.push_back({"W" + std::to_string(a), "ZZ", "W" + std::to_string(b), "ZZ",
                   "X" + std::to_string(e), AirwayDirection::kBoth,
                   AirwayLevel::kHigh, 0, 999});
  }
  GraphBuilder b(w, s);
  const auto& g = b.graph();
  INFO("V=" << g.VertexCount() << " E=" << g.EdgeCount());

  const int start = 0, goal = V - 1, k = 5;

  // Pyxis（边序列 + O(1) cumulative_cost）
  auto t0 = std::chrono::steady_clock::now();
  auto p = FindKShortestPaths(g, start, goal, k, YenOptions{});
  auto t1 = std::chrono::steady_clock::now();
  auto pyxis_ms =
      std::chrono::duration_cast<std::chrono::milliseconds>(t1 - t0).count();

  // bravo（CostOfPath + SelectEdge）
  t0 = std::chrono::steady_clock::now();
  auto bv = bravo::FindKShortestPaths(g, start, goal, k, SearchOptions{});
  t1 = std::chrono::steady_clock::now();
  auto bravo_ms =
      std::chrono::duration_cast<std::chrono::milliseconds>(t1 - t0).count();

  REQUIRE(p.has_value());
  REQUIRE(p->size() == bv.size());

  INFO("Pyxis: " << pyxis_ms << " ms | bravo: " << bravo_ms
                 << " ms | paths=" << p->size());
  // 性能不应劣于 bravo 原版
  CHECK(pyxis_ms <= bravo_ms + 500);
}

}  // namespace
}  // namespace px
