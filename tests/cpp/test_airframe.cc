// SPDX-License-Identifier: MIT
// airframe 校验链测试（S6——决策 21/28：物理不等式链 + perf_source）。

#include <catch2/catch_test_macros.hpp>
#include <string>

#include "px/module/flightplan/airframe.h"

namespace {

px::Airframe MakeValid() {
  px::Airframe a;
  a.type = "A320";
  a.variant = "Fenix A320 CFM";
  a.perf_source = px::PerfSource::kLnm;
  a.dow_kg = 41000.0;
  a.mzfw_kg = 61000.0;
  a.mtow_kg = 77000.0;
  a.mlw_kg = 66000.0;
  a.service_ceiling_ft = 39000.0;
  a.unit_pax_kg = 75.0;
  a.unit_bag_kg = 15.0;
  return a;
}

}  // namespace

TEST_CASE("airframe: 合法档案无问题", "[airframe][unit]") {
  CHECK(px::ValidateAirframe(MakeValid()).empty());
}

TEST_CASE("airframe: 物理不等式链", "[airframe][unit]") {
  {
    auto a = MakeValid();
    a.dow_kg = 62000.0;  // DOW > MZFW
    const auto issues = px::ValidateAirframe(a);
    REQUIRE(issues.size() == 1);
    CHECK(issues[0].field == "dow_kg");
  }
  {
    auto a = MakeValid();
    a.mzfw_kg = 80000.0;  // MZFW > MTOW
    const auto issues = px::ValidateAirframe(a);
    REQUIRE(issues.size() == 1);
    CHECK(issues[0].field == "mzfw_kg");
  }
  {
    auto a = MakeValid();
    a.mlw_kg = 78000.0;  // MLW > MTOW
    const auto issues = px::ValidateAirframe(a);
    REQUIRE(issues.size() == 1);
    CHECK(issues[0].field == "mlw_kg");
  }
}

TEST_CASE("airframe: 单位重量与升限非正", "[airframe][unit]") {
  {
    auto a = MakeValid();
    a.unit_pax_kg = 0.0;
    const auto issues = px::ValidateAirframe(a);
    REQUIRE(issues.size() == 1);
    CHECK(issues[0].field == "unit_pax_kg");
  }
  {
    auto a = MakeValid();
    a.service_ceiling_ft = 0.0;
    const auto issues = px::ValidateAirframe(a);
    REQUIRE(issues.size() == 1);
    CHECK(issues[0].field == "service_ceiling_ft");
  }
  {
    auto a = MakeValid();
    a.unit_bag_kg = 0.0;
    const auto issues = px::ValidateAirframe(a);
    REQUIRE(issues.size() == 1);
    CHECK(issues[0].field == "unit_bag_kg");
  }
}

TEST_CASE("airframe: type 必填", "[airframe][unit]") {
  auto a = MakeValid();
  a.type.clear();
  const auto issues = px::ValidateAirframe(a);
  REQUIRE(issues.size() == 1);
  CHECK(issues[0].field == "type");
}

TEST_CASE("airframe: variant 必填", "[airframe][unit]") {
  auto a = MakeValid();
  a.variant.clear();
  const auto issues = px::ValidateAirframe(a);
  REQUIRE(issues.size() == 1);
  CHECK(issues[0].field == "variant");
}

TEST_CASE("airframe: 巡航速度（决策 38：可选，提供则 > 0）",
          "[airframe][unit]") {
  {
    auto a = MakeValid();
    a.cruise_speed_kt = 437;  // A320 典型巡航 TAS
    CHECK(px::ValidateAirframe(a).empty());
  }
  {
    auto a = MakeValid();
    a.cruise_speed_kt = 0;  // 非法
    const auto issues = px::ValidateAirframe(a);
    REQUIRE(issues.size() == 1);
    CHECK(issues[0].field == "cruise_speed_kt");
  }
  {
    auto a = MakeValid();
    a.cruise_speed_kt = -100;  // 非法
    const auto issues = px::ValidateAirframe(a);
    REQUIRE(issues.size() == 1);
    CHECK(issues[0].field == "cruise_speed_kt");
  }
  {
    auto a = MakeValid();
    CHECK(px::ValidateAirframe(a).empty());  // 未录 = 合法（Prefile 提示缺失）
  }
}

TEST_CASE("airframe: 升限物理上限（审查修复——防 handler int 窄化 UB）",
          "[airframe][unit]") {
  auto a = MakeValid();
  a.service_ceiling_ft = 3e9;  // 超上限（double→int 窄化 UB）
  const auto issues = px::ValidateAirframe(a);
  REQUIRE(issues.size() == 1);
  CHECK(issues[0].field == "service_ceiling_ft");
}
