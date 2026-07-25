#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <vector>

#include "px/core/graph_edge.h"
#include "px/core/ident.h"
#include "px/core/nav_data_ir.h"
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

}  // namespace
}  // namespace px
