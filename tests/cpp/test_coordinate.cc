#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>
#include <cmath>

#include "px/core/coordinate.h"

namespace px {
namespace {

// ===================================================================
// 基本正确性
// ===================================================================

TEST_CASE("Coordinate::DistanceTo JFK→LAX") {
  Coordinate jfk{40.64, -73.78};
  Coordinate lax{33.94, -118.41};
  double d = jfk.DistanceTo(lax);
  // 精确值 2145.999 NM（haversine, R=6371.0088 km）
  REQUIRE(d == Catch::Approx(2146.0).margin(0.5));
}

TEST_CASE("Coordinate::DistanceTo same point is zero") {
  Coordinate a{34.5, 135.0};
  REQUIRE(a.DistanceTo(a) == Catch::Approx(0.0).margin(1e-9));
}

TEST_CASE("Coordinate::DistanceTo near-antipodal no NaN") {
  Coordinate a{0.0, 0.0};
  Coordinate b{0.0, 179.9};
  double d = a.DistanceTo(b);
  REQUIRE(!std::isnan(d));
  REQUIRE(d > 0.0);
}

TEST_CASE("Coordinate::BearingTo north") {
  Coordinate a{0.0, 0.0};
  Coordinate b{10.0, 0.0};
  double bng = a.BearingTo(b);
  REQUIRE(bng == Catch::Approx(0.0).margin(0.001));
}

TEST_CASE("Coordinate::BearingTo east") {
  Coordinate a{0.0, 0.0};
  Coordinate b{0.0, 10.0};
  double bng = a.BearingTo(b);
  REQUIRE(bng == Catch::Approx(90.0).margin(0.001));
}

// ===================================================================
// 已知航空城市对——用 Python 验证的精确值
// ===================================================================

TEST_CASE("Coordinate 已知航线 北京→上海") {
  Coordinate zbaa{40.08, 116.58};
  Coordinate zsss{31.20, 121.34};
  double d = zbaa.DistanceTo(zsss);
  REQUIRE(d == Catch::Approx(581.31).margin(0.1));
}

TEST_CASE("Coordinate 已知航线 东京→新加坡") {
  Coordinate rjtt{35.55, 139.78};
  Coordinate wsss{1.36, 103.99};
  double d = rjtt.DistanceTo(wsss);
  REQUIRE(d == Catch::Approx(2861.27).margin(0.1));
}

TEST_CASE("Coordinate 已知航线 伦敦→纽约") {
  Coordinate egll{51.47, -0.46};
  Coordinate kjfk{40.64, -73.78};
  double d = egll.DistanceTo(kjfk);
  REQUIRE(d == Catch::Approx(2991.28).margin(0.1));
}

TEST_CASE("Coordinate 已知航线 迪拜→悉尼") {
  Coordinate omdb{25.25, 55.36};
  Coordinate yssy{-33.95, 151.18};
  double d = omdb.DistanceTo(yssy);
  REQUIRE(d == Catch::Approx(6503.52).margin(0.1));
}

// ===================================================================
// 极点：经线弧长 = 角度 × π/180 × R
// ===================================================================

TEST_CASE("Coordinate 北极→赤道") {
  Coordinate north{90.0, 0.0};
  Coordinate equator{0.0, 0.0};
  double d = north.DistanceTo(equator);
  // 理论值 = (π/2) × 6371.0088 / 1.852 = 5403.6486 NM
  REQUIRE(d == Catch::Approx(5403.65).margin(0.05));
}

TEST_CASE("Coordinate 南极→北极") {
  Coordinate south{-90.0, 0.0};
  Coordinate north{90.0, 0.0};
  double d = south.DistanceTo(north);
  // 理论值 = π × 6371.0088 / 1.852 = 10807.2972 NM
  REQUIRE(d == Catch::Approx(10807.30).margin(0.05));
}

TEST_CASE("Coordinate 近极点不产生 NaN") {
  Coordinate near_north{89.9999, 0.0};
  Coordinate equator{0.0, 180.0};
  double d = near_north.DistanceTo(equator);
  REQUIRE(!std::isnan(d));
  REQUIRE(d > 0.0);
}

// ===================================================================
// 精确对趾点：距离 = 地球半周长
// ===================================================================

TEST_CASE("Coordinate 赤道对趾点") {
  Coordinate a{0.0, 0.0};
  Coordinate b{0.0, 180.0};
  double d = a.DistanceTo(b);
  // π × R = 10807.2972 NM
  REQUIRE(d == Catch::Approx(10807.30).margin(0.05));
}

TEST_CASE("Coordinate 非零纬度对趾点") {
  // (lat, lon) ↔ (-lat, lon+180°) 互相对趾
  Coordinate a{30.0, -120.0};
  Coordinate b{-30.0, 60.0};
  double d = a.DistanceTo(b);
  REQUIRE(d == Catch::Approx(10807.30).margin(0.05));
}

// ===================================================================
// 国际日期变更线
// ===================================================================

TEST_CASE("Coordinate 日期变更线穿越 2° 经差") {
  Coordinate a{0.0, 179.0};
  Coordinate b{0.0, -179.0};
  double d = a.DistanceTo(b);
  // 2° 赤道弧 = 2 × 60.04054 = 120.0811 NM
  REQUIRE(d == Catch::Approx(120.081).margin(0.05));
}

TEST_CASE("Coordinate +180 与 -180 是同一经线") {
  Coordinate a{45.0, 180.0};
  Coordinate b{45.0, -180.0};
  double d = a.DistanceTo(b);
  // 浮点误差级的微小距离，应为 0
  REQUIRE(d == Catch::Approx(0.0).margin(1e-6));
}

// ===================================================================
// 赤道：1° 经度严格等于赤道弧长
// ===================================================================

TEST_CASE("Coordinate 赤道 1° 经度") {
  Coordinate a{0.0, 0.0};
  Coordinate b{0.0, 1.0};
  double d = a.DistanceTo(b);
  // (π/180) × R = 60.04054 NM
  REQUIRE(d == Catch::Approx(60.041).margin(0.01));
}

TEST_CASE("Coordinate 赤道 15° 经度") {
  Coordinate a{0.0, 50.0};
  Coordinate b{0.0, 65.0};
  double d = a.DistanceTo(b);
  // 15 × 60.04054 = 900.6081 NM
  REQUIRE(d == Catch::Approx(900.608).margin(0.05));
}

// ===================================================================
// 极小距离：亚海里精度
// ===================================================================

TEST_CASE("Coordinate 极小距离") {
  Coordinate a{0.0, 0.0};
  Coordinate b{0.0, 0.00001};
  double d = a.DistanceTo(b);
  // 0.00001° ≈ 0.0006004 NM
  REQUIRE(d > 0.0);
  REQUIRE(d == Catch::Approx(0.0006004).margin(1e-7));
}

// ===================================================================
// 距离对称性
// ===================================================================

TEST_CASE("Coordinate 对称性 跨赤道") {
  Coordinate a{34.5, 135.0};
  Coordinate b{-23.4, -46.7};
  double d_ab = a.DistanceTo(b);
  double d_ba = b.DistanceTo(a);
  REQUIRE(d_ab == Catch::Approx(d_ba).margin(1e-9));
}

TEST_CASE("Coordinate 对称性 跨本初子午线") {
  Coordinate a{45.0, -120.0};
  Coordinate b{-30.0, 45.0};
  double d_ab = a.DistanceTo(b);
  double d_ba = b.DistanceTo(a);
  REQUIRE(d_ab == Catch::Approx(d_ba).margin(1e-9));
}

// ===================================================================
// 三角形不等式
// ===================================================================

TEST_CASE("Coordinate 三角形不等式") {
  Coordinate a{0.0, 0.0};
  Coordinate b{10.0, 0.0};
  Coordinate c{10.0, 10.0};
  //    d(A,B) = 600.4054
  //    d(B,C) = 591.2612
  //    d(A,C) = 846.9345  ≤  d(A,B) + d(B,C) = 1191.6666
  REQUIRE(a.DistanceTo(c) <= a.DistanceTo(b) + b.DistanceTo(c) + 1e-9);
}

// ===================================================================
// 方位角极端情况
// ===================================================================

TEST_CASE("Coordinate::BearingTo 正南") {
  Coordinate a{10.0, 0.0};
  Coordinate b{0.0, 0.0};
  double bng = a.BearingTo(b);
  REQUIRE(bng == Catch::Approx(180.0).margin(0.001));
}

TEST_CASE("Coordinate::BearingTo 正西") {
  Coordinate a{0.0, 10.0};
  Coordinate b{0.0, 0.0};
  double bng = a.BearingTo(b);
  REQUIRE(bng == Catch::Approx(270.0).margin(0.001));
}

TEST_CASE("Coordinate::BearingTo 同一点在 0到360") {
  Coordinate a{35.0, 140.0};
  double bng = a.BearingTo(a);
  // 退化情形：atan2(0,0) → 0
  REQUIRE(bng >= 0.0);
  REQUIRE(bng < 360.0);
}

TEST_CASE("Coordinate::BearingTo 北极出发") {
  Coordinate north{90.0, 0.0};
  Coordinate target{80.0, 45.0};
  double bng = north.BearingTo(target);
  // 从北极出发，所有方向沿经线指向南；往东经 45° 走即方位角 135°
  REQUIRE(bng == Catch::Approx(135.0).margin(0.1));
}

}  // namespace
}  // namespace px
