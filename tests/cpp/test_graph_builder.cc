#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <chrono>
#include <cstdlib>
#include <string>
#include <vector>

#include "px/core/graph_edge.h"
#include "px/core/ident.h"
#include "px/module/navdata/nav_data_ir.h"
#include "px/core/nav_graph.h"

#include "graph_builder.h"

namespace px {
namespace {

// ===================================================================
// 测试图数据
// ===================================================================

// ① 三角形: A—B—C—A，全双向，航路 "V1"
std::vector<RawWaypoint> TriangleWpts() {
  return {
      {"A", "XX", 0.0, 0.0},
      {"B", "XX", 0.0, 1.0},
      {"C", "XX", 1.0, 1.0},
  };
}
std::vector<RawSegment> TriangleSegs() {
  return {
      {"A", "XX", "B", "XX", "V1", AirwayDirection::kBoth, AirwayLevel::kHigh, 100, 400},
      {"B", "XX", "C", "XX", "V1", AirwayDirection::kBoth, AirwayLevel::kHigh, 100, 400},
      {"C", "XX", "A", "XX", "V1", AirwayDirection::kBoth, AirwayLevel::kHigh, 100, 400},
  };
}

// ② 线形链: A—B—C—D，全双向，航路 "J10"（低空）
std::vector<RawWaypoint> ChainWpts() {
  return {
      {"A", "XX", 0.0, 0.0},
      {"B", "XX", 0.0, 2.0},
      {"C", "XX", 0.0, 4.0},
      {"D", "XX", 0.0, 6.0},
  };
}
std::vector<RawSegment> ChainSegs() {
  return {
      {"A", "XX", "B", "XX", "J10", AirwayDirection::kBoth, AirwayLevel::kLow, 50, 200},
      {"B", "XX", "C", "XX", "J10", AirwayDirection::kBoth, AirwayLevel::kLow, 50, 200},
      {"C", "XX", "D", "XX", "J10", AirwayDirection::kBoth, AirwayLevel::kLow, 50, 200},
  };
}

// ③ Y 分叉: A→B, A→C（central→two branches）
std::vector<RawWaypoint> YWpts() {
  return {
      {"A", "XX", 0.0, 0.0},   // 中心
      {"B", "XX", 1.0, 0.0},   // 右支
      {"C", "XX", -1.0, 0.0},  // 左支
  };
}
std::vector<RawSegment> YSegs() {
  return {
      {"A", "XX", "B", "XX", "Y1", AirwayDirection::kBoth, AirwayLevel::kHigh, 0, 999},
      {"A", "XX", "C", "XX", "Y2", AirwayDirection::kBoth, AirwayLevel::kHigh, 0, 999},
  };
}

// ④ 死胡同: A—B→C，C 仅由 DCT 边连接（无航路边），无出边
std::vector<RawWaypoint> DeadEndWpts() {
  return {
      {"A", "XX", 0.0, 0.0},
      {"B", "XX", 1.0, 0.0},
      {"C", "XX", 2.0, 0.0},  // 死胡同终点
  };
}
std::vector<RawSegment> DeadEndSegs() {
  return {
      {"A", "XX", "B", "XX", "V1", AirwayDirection::kBoth, AirwayLevel::kHigh, 0, 999},
      // B→C 是 DCT（无航路名），单向 forward — C 只有入边
      {"B", "XX", "C", "XX", "", AirwayDirection::kForward, AirwayLevel::kBoth, 0, 0},
  };
}

// ⑤ 菱形: A 分叉到 B/C，汇聚到 D — 两条不同路径
std::vector<RawWaypoint> DiamondWpts() {
  return {
      {"A", "XX", 0.0, 2.0},   // 顶
      {"B", "XX", 1.0, 1.0},   // 右上
      {"C", "XX", -1.0, 1.0},  // 左上
      {"D", "XX", 0.0, 0.0},   // 底
  };
}
std::vector<RawSegment> DiamondSegs() {
  return {
      {"A", "XX", "B", "XX", "V1", AirwayDirection::kBoth, AirwayLevel::kHigh, 0, 999},
      {"A", "XX", "C", "XX", "V2", AirwayDirection::kBoth, AirwayLevel::kHigh, 0, 999},
      {"B", "XX", "D", "XX", "V1", AirwayDirection::kBoth, AirwayLevel::kHigh, 0, 999},
      {"C", "XX", "D", "XX", "V2", AirwayDirection::kBoth, AirwayLevel::kHigh, 0, 999},
  };
}

// ===================================================================
// 基本图构建
// ===================================================================

TEST_CASE("三角形") {
  GraphBuilder b(TriangleWpts(), TriangleSegs());
  const auto& g = b.graph();
  REQUIRE(g.VertexCount() == 3);
  // A 有 2 条出边：→B 和 ←C
  int n = 0;
  for (auto* e = g.EdgesBegin(0); e != g.EdgesEnd(0); ++e) n++;
  REQUIRE(n == 2);
}

TEST_CASE("线形链") {
  GraphBuilder b(ChainWpts(), ChainSegs());
  const auto& g = b.graph();
  REQUIRE(g.VertexCount() == 4);
  // 端点 A 和 D 各 1 条出边，中间 B、C 各 2 条出边（双向）
  REQUIRE(g.EdgesEnd(0) - g.EdgesBegin(0) == 1);  // A→B
  REQUIRE(g.EdgesEnd(1) - g.EdgesBegin(1) == 2);  // B→A, B→C
  REQUIRE(g.EdgesEnd(2) - g.EdgesBegin(2) == 2);  // C→B, C→D
  REQUIRE(g.EdgesEnd(3) - g.EdgesBegin(3) == 1);  // D→C
}

TEST_CASE("Y 分叉") {
  GraphBuilder b(YWpts(), YSegs());
  const auto& g = b.graph();
  REQUIRE(g.VertexCount() == 3);
  // 中心 A 有 2 条出边（双向），支端点各 1 条
  REQUIRE(g.EdgesEnd(0) - g.EdgesBegin(0) == 2);
  REQUIRE(g.EdgesEnd(1) - g.EdgesBegin(1) == 1);
  REQUIRE(g.EdgesEnd(2) - g.EdgesBegin(2) == 1);
}

TEST_CASE("死胡同") {
  GraphBuilder b(DeadEndWpts(), DeadEndSegs());
  REQUIRE(b.HasOutbound(0));   // A 有出边
  REQUIRE(b.HasOutbound(1));   // B 有出边（→C 的 DCT）
  REQUIRE(!b.HasOutbound(2));  // C 无出边——死胡同
  REQUIRE(b.HasInbound(2));    // C 有入边（B→C）
}

TEST_CASE("菱形网络") {
  GraphBuilder b(DiamondWpts(), DiamondSegs());
  const auto& g = b.graph();
  REQUIRE(g.VertexCount() == 4);
  // A 有 2 出边（→B, →C），D 有 2 入边
  REQUIRE(g.EdgesEnd(0) - g.EdgesBegin(0) == 2);
  // B 出边：←A 和 →D（双向）= 2
  REQUIRE(g.EdgesEnd(1) - g.EdgesBegin(1) == 2);
  // D 出边：←B 和 ←C（双向）= 2
  REQUIRE(g.EdgesEnd(3) - g.EdgesBegin(3) == 2);
}

// ===================================================================
// 顶点查找
// ===================================================================

TEST_CASE("VertexByIdent 找到和找不到") {
  GraphBuilder b(TriangleWpts(), TriangleSegs());
  REQUIRE(b.VertexByIdent(Ident{"A", "XX"}) == 0);
  REQUIRE(b.VertexByIdent(Ident{"B", "XX"}) == 1);
  REQUIRE(b.VertexByIdent(Ident{"X", "XX"}) == -1);
}

TEST_CASE("IdentOf 返回正确标识符") {
  GraphBuilder b(ChainWpts(), ChainSegs());
  REQUIRE(b.IdentOf(0).ident == "A");
  REQUIRE(b.IdentOf(3).ident == "D");
}

// ===================================================================
// HasOutbound / HasInbound
// ===================================================================

TEST_CASE("HasOutbound / HasInbound 正确标记") {
  GraphBuilder b(TriangleWpts(), TriangleSegs());
  // 全双向三角形：每个顶点都有出入边
  for (int v = 0; v < 3; ++v) {
    REQUIRE(b.HasOutbound(v));
    REQUIRE(b.HasInbound(v));
  }
}

TEST_CASE("越界顶点返回 false") {
  GraphBuilder b(TriangleWpts(), TriangleSegs());
  REQUIRE(!b.HasOutbound(-1));
  REQUIRE(!b.HasOutbound(999));
}

// ===================================================================
// 航路名表
// ===================================================================

TEST_CASE("Airway 表正确") {
  GraphBuilder b(TriangleWpts(), TriangleSegs());
  REQUIRE(b.AirwayName(0) == "DCT");
  REQUIRE(b.AirwayName(1) == "V1");
  REQUIRE(b.AirwayCount() == 2);
}

TEST_CASE("多航路图的名称表") {
  GraphBuilder b(DiamondWpts(), DiamondSegs());
  // DCT + V1 + V2
  REQUIRE(b.AirwayCount() == 3);
  REQUIRE(b.AirwayName(0) == "DCT");
  REQUIRE(b.AirwayName(1) == "V1");
  REQUIRE(b.AirwayName(2) == "V2");
}

// ===================================================================
// 方向编码
// ===================================================================

TEST_CASE("kForward 只有 from→to") {
  std::vector<RawSegment> segs = {
      {"A", "XX", "B", "XX", "V1", AirwayDirection::kForward, AirwayLevel::kHigh, 100, 400},
  };
  GraphBuilder b(TriangleWpts(), segs);
  const auto& g = b.graph();
  // A 有出边→B
  REQUIRE(g.EdgesEnd(0) - g.EdgesBegin(0) == 1);
  // B 无出边
  REQUIRE(g.EdgesEnd(1) - g.EdgesBegin(1) == 0);
}

TEST_CASE("kBackward 只有 to→from") {
  std::vector<RawSegment> segs = {
      {"A", "XX", "B", "XX", "V1", AirwayDirection::kBackward, AirwayLevel::kHigh, 100, 400},
  };
  GraphBuilder b(TriangleWpts(), segs);
  const auto& g = b.graph();
  // A 无出边
  REQUIRE(g.EdgesEnd(0) - g.EdgesBegin(0) == 0);
  // B 有出边→A
  REQUIRE(g.EdgesEnd(1) - g.EdgesBegin(1) == 1);
}

// ===================================================================
// 边属性
// ===================================================================

TEST_CASE("边携带高度层和 FL 范围") {
  GraphBuilder b(TriangleWpts(), TriangleSegs());
  const auto* e = b.graph().EdgesBegin(0);
  REQUIRE(e->level == AirwayLevel::kHigh);
  REQUIRE(e->base_fl == 100);
  REQUIRE(e->top_fl == 400);
}

TEST_CASE("边距离为正") {
  GraphBuilder b(TriangleWpts(), TriangleSegs());
  for (auto* e = b.graph().EdgesBegin(0); e != b.graph().EdgesEnd(0); ++e)
    REQUIRE(e->distance_nm > 0.0f);
}

TEST_CASE("DCT 边的 airway_id == 0 且 base/top == 0") {
  GraphBuilder b(DeadEndWpts(), DeadEndSegs());
  const auto& g = b.graph();
  // B 的出边→C 是 DCT
  for (auto* e = g.EdgesBegin(1); e != g.EdgesEnd(1); ++e) {
    if (e->to == 2) {
      REQUIRE(e->airway_id == 0);
      REQUIRE(e->base_fl == 0);
      REQUIRE(e->top_fl == 0);
    }
  }
}

// ===================================================================
// 边界情况
// ===================================================================

TEST_CASE("空输入不崩溃") {
  GraphBuilder b({}, {});
  REQUIRE(b.graph().VertexCount() == 0);
  REQUIRE(b.VertexByIdent(Ident{"X", "XX"}) == -1);
  REQUIRE(b.AirwayCount() == 1);  // 始终保留 "DCT"
}

TEST_CASE("航段引用不存在的航点——跳过不崩溃") {
  std::vector<RawWaypoint> wpts = {
      {"A", "XX", 0.0, 0.0},
      {"B", "XX", 1.0, 0.0},
  };
  std::vector<RawSegment> segs = {
      {"A", "XX", "B", "XX", "V1", AirwayDirection::kBoth, AirwayLevel::kHigh, 100, 400},
      {"B", "XX", "C", "XX", "V1", AirwayDirection::kBoth, AirwayLevel::kHigh, 100, 400},  // C 不在 wpts 中
  };
  GraphBuilder b(wpts, segs);
  const auto& g = b.graph();
  REQUIRE(g.VertexCount() == 2);
  // 只有 A↔B 被构建
  REQUIRE(g.EdgesEnd(0) - g.EdgesBegin(0) == 1);
  REQUIRE(g.EdgesEnd(1) - g.EdgesBegin(1) == 1);
}

TEST_CASE("航段两端都不存在——图仍正确") {
  std::vector<RawWaypoint> wpts = {{"A", "XX", 0.0, 0.0}};
  std::vector<RawSegment> segs = {
      {"X", "XX", "Y", "XX", "V1", AirwayDirection::kBoth, AirwayLevel::kHigh, 100, 400},
  };
  GraphBuilder b(wpts, segs);
  REQUIRE(b.graph().VertexCount() == 1);
  REQUIRE(b.graph().EdgesEnd(0) - b.graph().EdgesBegin(0) == 0);
}

TEST_CASE("非法 FL 范围存储但不校验——由约束层拒绝") {
  RawWaypoint a{"A", "XX", 0.0, 0.0}, b{"B", "XX", 1.0, 0.0};
  std::vector<RawSegment> segs = {
      {"A", "XX", "B", "XX", "V1", AirwayDirection::kBoth, AirwayLevel::kHigh, 400, 100},  // base > top
  };
  GraphBuilder builder({a, b}, segs);
  const auto* e = builder.graph().EdgesBegin(0);
  REQUIRE(e->base_fl == 400);
  REQUIRE(e->top_fl == 100);
  // 边存在但无法通过 AltitudeBandConstraint
}

TEST_CASE("相同航路名多航段不重复计数") {
  RawWaypoint a{"A", "XX", 0.0, 0.0}, b{"B", "XX", 1.0, 0.0}, c{"C", "XX", 2.0, 0.0};
  std::vector<RawSegment> segs = {
      {"A", "XX", "B", "XX", "J10", AirwayDirection::kBoth, AirwayLevel::kLow, 50, 200},
      {"B", "XX", "C", "XX", "J10", AirwayDirection::kBoth, AirwayLevel::kLow, 50, 200},
  };
  GraphBuilder builder({a, b, c}, segs);
  REQUIRE(builder.AirwayCount() == 2);   // DCT + J10
  REQUIRE(builder.AirwayName(1) == "J10");
}

// ===================================================================
// 真实规模压力测试 (V≈270K, E≈345K)
// ===================================================================

TEST_CASE("真实数据规模——构建时间和内存") {
  // 模拟真实航路图: 线形链 + 稀疏交叉连接
  // V = 270K, 平均出度 ≈ 1.3 → E ≈ 350K
  constexpr int V = 270000;
  constexpr int chain_edges = V - 1;          // 线形链
  constexpr int cross_edges = 80000;          // 稀疏交叉
  constexpr int total_segs = chain_edges + cross_edges;

  // 生成航点: 沿经线排列
  std::vector<RawWaypoint> wpts;
  wpts.reserve(V);
  for (int i = 0; i < V; ++i) {
    double lat = (i % 180) - 90.0;
    double lon = (i / 180) - 180.0;
    wpts.push_back({"W" + std::to_string(i), "XX", lat, lon});
  }

  // 生成航段
  std::vector<RawSegment> segs;
  segs.reserve(total_segs);

  // 线形链: i → i+1 (kBoth), 高空
  for (int i = 0; i < chain_edges; ++i) {
    segs.push_back({"W" + std::to_string(i), "XX",
                    "W" + std::to_string(i + 1), "XX",
                    "J1", AirwayDirection::kBoth, AirwayLevel::kHigh, 100, 400});
  }

  // 稀疏交叉: (i, i+skip) 跳跃连接
  std::srand(42);
  for (int k = 0; k < cross_edges; ++k) {
    int i = std::rand() % (V - 100);
    int j = i + 10 + std::rand() % 90;
    segs.push_back({"W" + std::to_string(i), "XX",
                    "W" + std::to_string(j), "XX",
                    "V" + std::to_string(k % 500), AirwayDirection::kForward,
                    AirwayLevel::kHigh, 0, 999});
  }

  // 构建计时
  auto t0 = std::chrono::steady_clock::now();
  GraphBuilder b(wpts, segs);
  auto t1 = std::chrono::steady_clock::now();
  auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(t1 - t0).count();

  const auto& g = b.graph();
  REQUIRE(g.VertexCount() == V);

  // 线形链: 中间节点出度 = 2 (双向), 两端 = 1
  REQUIRE(g.EdgesEnd(0) - g.EdgesBegin(0) == 1);           // 起点
  REQUIRE(g.EdgesEnd(V / 2) - g.EdgesBegin(V / 2) >= 2);   // 中段
  REQUIRE(b.graph().EdgesBegin(V - 1) != nullptr);          // 终点可访问

  // 航路名去重: DCT + J1 + V0..V499 = 501
  REQUIRE(b.AirwayCount() == 1 + 1 + 500);

  INFO("V=" << V << " E≈350K 构建耗时 " << ms << " ms");
  // 真实数据预期: BravoFinder 冷启动 ~2.27s（含解析+建图）,
  // 纯建图部分应在 500ms 以内
  REQUIRE(ms < 2500);  // 足够覆盖 Debug/Release 差异
}

}  // namespace
}  // namespace px
