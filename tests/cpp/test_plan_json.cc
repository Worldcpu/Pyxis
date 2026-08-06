// SPDX-License-Identifier: MIT
// plan JSON 渲染测试（S5a——决策 8/14：候选/完整计划 JSON 形状）。
// 从 FlightPlan 域对象渲染（纯函数，不依赖 NavDatabase）。

#include <rapidjson/document.h>

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>
#include <string>

#include "px/module/flightplan/flight_plan.h"
#include "px/service/plan_json.h"

namespace {

px::FlightPlan MakePlan() {
  px::FlightPlan plan;
  plan.sid = "DEEZZ5";
  plan.star = "CAMRN5";
  plan.dep_runway = "RW31L";
  plan.arr_runway = "RW18R";
  plan.dep_connection = "procedure";
  plan.arr_connection = "procedure";
  plan.segments = {
      {px::SegmentKind::kSid, "ZUCK", "TONIN", "SID", 20.5},
      {px::SegmentKind::kEnroute, "TONIN", "MAKET", "W80", 850.0},
      {px::SegmentKind::kStar, "MAKET", "ZBAA", "STAR", 30.0},
  };
  plan.points = {
      {"ZUCK", "SID", 29.7192, 106.6417, 0},
      {"TONIN", "SID", 28.5, 104.2, 0},
      {"MAKET", "W80", 27.0, 101.5, 1},
      {"ZBAA", "STAR", 40.0801, 116.5846, 2},
  };
  return plan;
}

rapidjson::Document Parse(const std::string& json) {
  rapidjson::Document doc;
  doc.Parse(json.c_str());
  return doc;
}

}  // namespace

TEST_CASE("plan JSON: 候选条目契约（决策 9）", "[plan][unit]") {
  const auto json = px::RenderPlanCandidatesJson({MakePlan()}, 12345);
  const auto doc = Parse(json);

  REQUIRE(doc.IsArray());
  REQUIRE(doc.Size() == 1);
  const auto& candidate = doc[0];

  // index + route_string + seed（决策 7：seed 响应回传）。
  CHECK(candidate["index"].GetInt() == 0);
  REQUIRE(candidate.HasMember("route_string"));
  CHECK(candidate["seed"].GetUint() == 12345);

  // 距离：总分 + 分阶段（dep = SID 段和，enroute = 航路段和，
  // arr = STAR+approach 段和——手算：20.5 + 850 + 30 = 900.5）。
  CHECK(candidate["total_distance_nm"].GetDouble() == Catch::Approx(900.5));
  REQUIRE(candidate.HasMember("distances"));
  CHECK(candidate["distances"]["dep_nm"].GetDouble() == Catch::Approx(20.5));
  CHECK(candidate["distances"]["enroute_nm"].GetDouble() ==
        Catch::Approx(850.0));
  CHECK(candidate["distances"]["arr_nm"].GetDouble() == Catch::Approx(30.0));

  // 程序名/跑道/连接 token（决策 9）。
  CHECK(std::string(candidate["sid"].GetString()) == "DEEZZ5");
  CHECK(std::string(candidate["star"].GetString()) == "CAMRN5");
  CHECK(std::string(candidate["dep_runway"].GetString()) == "RW31L");
  CHECK(std::string(candidate["arr_runway"].GetString()) == "RW18R");
  CHECK(std::string(candidate["dep_connection"].GetString()) == "procedure");
  CHECK(std::string(candidate["arr_connection"].GetString()) == "procedure");

  // 决策 9：候选不带 segments（generate 阶段才物化）——只有点序列。
  CHECK(!candidate.HasMember("segments"));
  REQUIRE(candidate.HasMember("points"));
  CHECK(candidate["points"].Size() == 4);
  CHECK(candidate["points"][0]["segment_index"].GetInt() == 0);
  CHECK(candidate["points"][3]["segment_index"].GetInt() == 2);
}

TEST_CASE("plan JSON: 多候选 index 递增", "[plan][unit]") {
  const auto json = px::RenderPlanCandidatesJson({MakePlan(), MakePlan()}, 0);
  const auto doc = Parse(json);
  REQUIRE(doc.Size() == 2);
  CHECK(doc[0]["index"].GetInt() == 0);
  CHECK(doc[1]["index"].GetInt() == 1);
}

TEST_CASE("plan JSON: 空候选数组", "[plan][unit]") {
  const auto json = px::RenderPlanCandidatesJson({}, 0);
  const auto doc = Parse(json);
  REQUIRE(doc.IsArray());
  CHECK(doc.Empty());
}

TEST_CASE("plan JSON: 完整计划渲染（generate 形状）", "[plan][unit]") {
  auto plan = MakePlan();
  plan.altitude = {350, 10668, px::AltitudeRule::kIcao, "东行奇数层", false};
  plan.fuel.block_kg = 12000.0;
  plan.mora_checked = false;
  plan.experimental = true;
  plan.checks.status = px::CheckStatus::kWarning;
  plan.checks.warnings = {"应急油比例异常高"};

  const auto json = px::RenderPlanJson(plan);
  const auto doc = Parse(json);

  REQUIRE(doc.HasMember("altitude"));
  CHECK(doc["altitude"]["fl"].GetInt() == 350);
  CHECK(doc["altitude"]["meters"].GetInt() == 10668);
  CHECK(std::string(doc["altitude"]["rule"].GetString()) == "icao");

  REQUIRE(doc.HasMember("fuel"));
  CHECK(doc["fuel"]["block_kg"].GetDouble() == Catch::Approx(12000.0));

  CHECK(doc["mora_checked"].GetBool() == false);
  CHECK(doc["experimental"].GetBool() == true);

  REQUIRE(doc.HasMember("checks"));
  CHECK(doc["checks"]["status"].GetInt() ==
        static_cast<int>(px::CheckStatus::kWarning));
  REQUIRE(doc["checks"]["warnings"].Size() == 1);

  // 决策 14：generate 响应含航路（route_string + segments + 点序列）。
  REQUIRE(doc.HasMember("route"));
  CHECK(doc["route"]["segments"].Size() == 3);
  CHECK(doc["route"]["segments"][1]["kind"].GetString() ==
        std::string("enroute"));
  CHECK(doc["route"]["points"].Size() == 4);
  // generate 是选定后完整计划：程序/跑道/连接信息随航路输出（与候选
  // 契约一致，前端生成页显示 SID/STAR 无需另取）。
  CHECK(std::string(doc["route"]["sid"].GetString()) == "DEEZZ5");
  CHECK(std::string(doc["route"]["star"].GetString()) == "CAMRN5");
  CHECK(std::string(doc["route"]["dep_runway"].GetString()) == "RW31L");
  CHECK(std::string(doc["route"]["arr_runway"].GetString()) == "RW18R");
  CHECK(std::string(doc["route"]["dep_connection"].GetString()) == "procedure");
  CHECK(std::string(doc["route"]["arr_connection"].GetString()) == "procedure");
}

TEST_CASE("plan JSON: 进近/备降段渲染", "[plan][unit]") {
  auto plan = MakePlan();
  plan.segments = {
      {px::SegmentKind::kSid, "ZUCK", "TONIN", "SID", 20.5},
      {px::SegmentKind::kEnroute, "TONIN", "MAKET", "W80", 850.0},
      {px::SegmentKind::kStar, "MAKET", "PANKI", "STAR", 30.0},
      {px::SegmentKind::kApproach, "PANKI", "ZBAA", "ILS01R", 12.0},
      {px::SegmentKind::kAlternate, "ZBAA", "ZSSS", "DCT", 350.0},
  };

  // 候选：arr = STAR + 进近段和；备降段不计入分阶段（只进 total）。
  {
    const auto doc = Parse(px::RenderPlanCandidatesJson({plan}, 0));
    REQUIRE(doc.Size() == 1);
    REQUIRE(doc[0].HasMember("distances"));
    CHECK(doc[0]["distances"]["dep_nm"].GetDouble() == Catch::Approx(20.5));
    CHECK(doc[0]["distances"]["enroute_nm"].GetDouble() ==
          Catch::Approx(850.0));
    CHECK(doc[0]["distances"]["arr_nm"].GetDouble() == Catch::Approx(42.0));
    CHECK(doc[0]["total_distance_nm"].GetDouble() == Catch::Approx(1262.5));
  }

  // 完整计划：kind 字符串覆盖 approach/alternate。
  {
    const auto doc = Parse(px::RenderPlanJson(plan));
    REQUIRE(doc["route"]["segments"].Size() == 5);
    CHECK(std::string(doc["route"]["segments"][3]["kind"].GetString()) ==
          "approach");
    CHECK(std::string(doc["route"]["segments"][4]["kind"].GetString()) ==
          "alternate");
  }
}

TEST_CASE("plan JSON: altitude 规则三态渲染", "[plan][unit]") {
  // 决策 27 三态：kAuto 不塌缩为 "icao"。
  {
    auto plan = MakePlan();
    plan.altitude = {0, 0, px::AltitudeRule::kAuto, "", false};
    const auto doc = Parse(px::RenderPlanJson(plan));
    CHECK(std::string(doc["altitude"]["rule"].GetString()) == "auto");
  }
  {
    auto plan = MakePlan();
    plan.altitude = {350, 10668, px::AltitudeRule::kChina, "", false};
    const auto doc = Parse(px::RenderPlanJson(plan));
    CHECK(std::string(doc["altitude"]["rule"].GetString()) == "china");
  }
}
