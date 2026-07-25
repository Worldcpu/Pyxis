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

// 构建三角形图：A—B—C—A，双向，航路 "V1"
std::vector<RawWaypoint> TriangleWaypoints() {
  return {
      {"A", "XX", 0.0, 0.0},
      {"B", "XX", 0.0, 1.0},
      {"C", "XX", 1.0, 1.0},
  };
}

std::vector<RawSegment> TriangleSegments() {
  return {
      {"A", "XX", "B", "XX", "V1", AirwayDirection::kBoth, AirwayLevel::kHigh, 100, 400},
      {"B", "XX", "C", "XX", "V1", AirwayDirection::kBoth, AirwayLevel::kHigh, 100, 400},
      {"C", "XX", "A", "XX", "V1", AirwayDirection::kBoth, AirwayLevel::kHigh, 100, 400},
  };
}

TEST_CASE("GraphBuilder builds triangle graph") {
  GraphBuilder builder(TriangleWaypoints(), TriangleSegments());
  const auto& g = builder.graph();

  REQUIRE(g.VertexCount() == 3);

  int count = 0;
  for (auto* e = g.EdgesBegin(0); e != g.EdgesEnd(0); ++e) count++;
  REQUIRE(count == 2);  // A→B 和 A←C
}

TEST_CASE("GraphBuilder vertex lookup by ident") {
  GraphBuilder builder(TriangleWaypoints(), TriangleSegments());

  REQUIRE(builder.VertexByIdent(Ident{"A", "XX"}) == 0);
  REQUIRE(builder.VertexByIdent(Ident{"B", "XX"}) == 1);
  REQUIRE(builder.VertexByIdent(Ident{"X", "XX"}) == -1);
}

TEST_CASE("GraphBuilder has outbound/inbound") {
  GraphBuilder builder(TriangleWaypoints(), TriangleSegments());
  REQUIRE(builder.HasOutbound(0));
  REQUIRE(builder.HasInbound(0));
}

TEST_CASE("GraphBuilder airway name table") {
  GraphBuilder builder(TriangleWaypoints(), TriangleSegments());
  REQUIRE(builder.AirwayName(0) == "DCT");
  REQUIRE(builder.AirwayName(1) == "V1");
  REQUIRE(builder.AirwayCount() == 2);
}

TEST_CASE("GraphBuilder forward-only direction") {
  std::vector<RawSegment> segs = {
      {"A", "XX", "B", "XX", "V1", AirwayDirection::kForward, AirwayLevel::kHigh, 100, 400},
  };
  GraphBuilder builder(TriangleWaypoints(), segs);
  const auto& g = builder.graph();

  // A→B 有边
  bool has_ab = false;
  for (auto* e = g.EdgesBegin(0); e != g.EdgesEnd(0); ++e)
    if (e->to == 1) has_ab = true;
  REQUIRE(has_ab);

  // B→A 无边
  int count_b = 0;
  for (auto* e = g.EdgesBegin(1); e != g.EdgesEnd(1); ++e) count_b++;
  REQUIRE(count_b == 0);
}

TEST_CASE("GraphBuilder backward-only direction") {
  std::vector<RawSegment> segs = {
      {"A", "XX", "B", "XX", "V1", AirwayDirection::kBackward, AirwayLevel::kHigh, 100, 400},
  };
  GraphBuilder builder(TriangleWaypoints(), segs);
  const auto& g = builder.graph();

  // A→B 无边
  int count_a = 0;
  for (auto* e = g.EdgesBegin(0); e != g.EdgesEnd(0); ++e) count_a++;
  REQUIRE(count_a == 0);

  // B→A 有边
  bool has_ba = false;
  for (auto* e = g.EdgesBegin(1); e != g.EdgesEnd(1); ++e)
    if (e->to == 0) has_ba = true;
  REQUIRE(has_ba);
}

TEST_CASE("GraphBuilder edge carries level and altitude") {
  GraphBuilder builder(TriangleWaypoints(), TriangleSegments());
  const auto& g = builder.graph();
  const auto* e = g.EdgesBegin(0);
  REQUIRE(e->level == AirwayLevel::kHigh);
  REQUIRE(e->base_fl == 100);
  REQUIRE(e->top_fl == 400);
}

TEST_CASE("GraphBuilder edge distance positive") {
  GraphBuilder builder(TriangleWaypoints(), TriangleSegments());
  const auto& g = builder.graph();
  // A(0) 两条出边均有正距离
  for (auto* e = g.EdgesBegin(0); e != g.EdgesEnd(0); ++e)
    REQUIRE(e->distance_nm > 0.0f);
}

TEST_CASE("GraphBuilder IdentOf") {
  GraphBuilder builder(TriangleWaypoints(), TriangleSegments());
  REQUIRE(builder.IdentOf(0).ident == "A");
  REQUIRE(builder.IdentOf(0).region == "XX");
}

}  // namespace
}  // namespace px
