#include <catch2/catch_test_macros.hpp>

#include "altitude_constraints.h"
#include "px/core/constraint.h"
#include "px/core/route_query.h"

namespace {

px::GraphEdge MakeEdge(int base_fl, int top_fl, bool is_high) {
  px::GraphEdge e;
  e.to = 1;
  e.distance_nm = 100.0f;
  e.airway_id = 1;
  e.base_fl = static_cast<int16_t>(base_fl);
  e.top_fl = static_cast<int16_t>(top_fl);
  e.level = is_high ? px::AirwayLevel::kHigh : px::AirwayLevel::kLow;
  return e;
}

px::RouteQuery WithAltitude(int fl) {
  px::RouteQuery q;
  q.altitude = px::FlRange{fl, fl};
  return q;
}

px::RouteQuery WithAltitudeRange(int min_fl, int max_fl) {
  px::RouteQuery q;
  q.altitude = px::FlRange{min_fl, max_fl};
  return q;
}

// ========== 高度带约束 ==========

TEST_CASE("高度带: 无巡航高度放行全部") {
  px::AltitudeBandConstraint c;
  px::RouteQuery r;
  px::EdgeContext ctx{MakeEdge(180, 450, true), {}, {}};
  CHECK(c.Evaluate(ctx, r).allowed);
}

TEST_CASE("高度带: 阻断范围外的边") {
  px::AltitudeBandConstraint c;
  px::EdgeContext ctx{MakeEdge(180, 450, true), {}, {}};
  CHECK(c.Evaluate(ctx, WithAltitude(350)).allowed);
  CHECK_FALSE(c.Evaluate(ctx, WithAltitude(100)).allowed);
  CHECK_FALSE(c.Evaluate(ctx, WithAltitude(500)).allowed);
}

TEST_CASE("高度带: 范围重叠即可通行") {
  px::AltitudeBandConstraint c;
  px::EdgeContext ctx{MakeEdge(180, 450, true), {}, {}};
  CHECK(c.Evaluate(ctx, WithAltitudeRange(300, 400)).allowed);
  CHECK(c.Evaluate(ctx, WithAltitudeRange(100, 200)).allowed);
  CHECK(c.Evaluate(ctx, WithAltitudeRange(400, 600)).allowed);
}

TEST_CASE("高度带: 范围完全在带外则阻断") {
  px::AltitudeBandConstraint c;
  px::EdgeContext ctx{MakeEdge(180, 450, true), {}, {}};
  CHECK_FALSE(c.Evaluate(ctx, WithAltitudeRange(50, 170)).allowed);
  CHECK_FALSE(c.Evaluate(ctx, WithAltitudeRange(460, 600)).allowed);
}

TEST_CASE("高度带: DCT 永远放行") {
  px::AltitudeBandConstraint c;
  px::EdgeContext ctx{MakeEdge(0, 0, false), {}, {}};
  CHECK(c.Evaluate(ctx, WithAltitude(350)).allowed);
}

TEST_CASE("高度带: 开放上限不约束高 FL") {
  px::AltitudeBandConstraint c;
  px::EdgeContext ctx{MakeEdge(200, 0, true), {}, {}};
  CHECK(c.Evaluate(ctx, WithAltitude(400)).allowed);
  CHECK_FALSE(c.Evaluate(ctx, WithAltitude(100)).allowed);
}

TEST_CASE("高度带: 开放下限不约束低 FL") {
  px::AltitudeBandConstraint c;
  px::EdgeContext ctx{MakeEdge(0, 180, false), {}, {}};
  CHECK(c.Evaluate(ctx, WithAltitude(50)).allowed);
  CHECK_FALSE(c.Evaluate(ctx, WithAltitude(250)).allowed);
}

TEST_CASE("高度带: 范围仅触底也放行") {
  px::AltitudeBandConstraint c;
  px::EdgeContext ctx{MakeEdge(180, 450, true), {}, {}};
  CHECK(c.Evaluate(ctx, WithAltitudeRange(180, 180)).allowed);
}

TEST_CASE("高度带: 范围仅触顶也放行") {
  px::AltitudeBandConstraint c;
  px::EdgeContext ctx{MakeEdge(180, 450, true), {}, {}};
  CHECK(c.Evaluate(ctx, WithAltitudeRange(450, 450)).allowed);
}

// ========== 层级偏好约束 ==========

TEST_CASE("层级偏好: 无偏好放行全部") {
  px::LevelPreferenceConstraint c(0.5);
  px::RouteQuery r;  // kNone
  px::EdgeContext high{MakeEdge(180, 450, true), {}, {}};
  px::EdgeContext low{MakeEdge(0, 180, false), {}, {}};
  CHECK(c.Evaluate(high, r).extra_cost == 0.0);
  CHECK(c.Evaluate(low, r).extra_cost == 0.0);
}

TEST_CASE("层级偏好: 非偏好层级受罚") {
  px::LevelPreferenceConstraint c(0.5);
  px::RouteQuery r;
  r.level = px::LevelPreference::kHigh;
  px::EdgeContext high{MakeEdge(180, 450, true), {}, {}};
  px::EdgeContext low{MakeEdge(0, 180, false), {}, {}};
  CHECK(c.Evaluate(high, r).extra_cost == 0.0);
  px::EdgeVerdict v = c.Evaluate(low, r);
  CHECK(v.allowed);
  CHECK(v.extra_cost == 50.0);
}

TEST_CASE("层级偏好: kBoth 永不受罚") {
  px::LevelPreferenceConstraint c(0.5);
  px::GraphEdge e = MakeEdge(180, 450, true);
  e.level = px::AirwayLevel::kBoth;
  px::EdgeContext ctx{e, {}, {}};
  px::RouteQuery high;
  high.level = px::LevelPreference::kHigh;
  CHECK(c.Evaluate(ctx, high).extra_cost == 0.0);
  px::RouteQuery low;
  low.level = px::LevelPreference::kLow;
  CHECK(c.Evaluate(ctx, low).extra_cost == 0.0);
}

}  // namespace
