#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <cmath>

#include "px/core/coordinate.h"

namespace px {
namespace {

TEST_CASE("Coordinate::DistanceTo JFK→LAX") {
  // 纽约肯尼迪 KJFK → 洛杉矶 KLAX，实际距离约 2147 海里
  Coordinate jfk{40.64, -73.78};
  Coordinate lax{33.94, -118.41};
  double d = jfk.DistanceTo(lax);
  REQUIRE(d == Catch::Approx(2147.0).margin(15.0));
}

TEST_CASE("Coordinate::DistanceTo same point is zero") {
  Coordinate a{34.5, 135.0};
  REQUIRE(a.DistanceTo(a) == Catch::Approx(0.0).margin(1e-6));
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
  REQUIRE(bng == Catch::Approx(0.0).margin(0.5));
}

TEST_CASE("Coordinate::BearingTo east") {
  Coordinate a{0.0, 0.0};
  Coordinate b{0.0, 10.0};
  double bng = a.BearingTo(b);
  REQUIRE(bng == Catch::Approx(90.0).margin(0.5));
}

}  // namespace
}  // namespace px
