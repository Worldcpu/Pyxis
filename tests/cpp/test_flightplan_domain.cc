// SPDX-License-Identifier: MIT
// flightplan 域测试（S1 FromBf 转换——决策 31①）。
// 测试构造 bf::Route（px_tests 经 px_core 传递获取 bf 头），在 px 域
// 类型上断言转换行为；期望值来自手算（独立真源，不复算实现逻辑）。

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include "core/routing/route.h"
#include "px/module/flightplan/airframe.h"
#include "px/module/flightplan/altitude_planner.h"
#include "px/module/flightplan/flight_plan.h"
#include "px/module/flightplan/from_bf.h"
#include "px/module/flightplan/payload.h"

namespace {

// 配载测试用档案（重量字段与 MakeValid 相同的手算基准）。
px::Airframe MakePayloadAirframe() {
  px::Airframe a;
  a.dow_kg = 41000.0;
  a.unit_pax_kg = 75.0;
  a.unit_bag_kg = 15.0;
  return a;
}

// 构造典型航线：连续 SID legs → 聚合为一段；W80 航路段；DCT 段；
// 连续 STAR legs → 聚合为一段。
bf::Route MakeSampleRoute() {
  bf::Route r;
  r.legs = {
      {"ZUCK", "GURUN", "SID", 12.5, {}},  {"GURUN", "TONIN", "SID", 8.0, {}},
      {"TONIN", "NIRON", "W80", 60.0, {}}, {"NIRON", "MAKET", "DCT", 25.0, {}},
      {"MAKET", "ZBAA", "STAR", 18.0, {}}, {"ZBAA", "PEK04", "STAR", 12.0, {}},
  };
  return r;
}

}  // namespace

TEST_CASE("FromBf: 段 kind 映射与程序聚合", "[flightplan][unit]") {
  const auto plan = px::FromBf(MakeSampleRoute());

  REQUIRE(plan.segments.size() == 4);

  // 连续 SID legs 聚合为一段（距离求和 12.5 + 8.0）。
  CHECK(plan.segments[0].kind == px::SegmentKind::kSid);
  CHECK(plan.segments[0].from_ident == "ZUCK");
  CHECK(plan.segments[0].to_ident == "TONIN");
  CHECK(plan.segments[0].via == "SID");
  CHECK(plan.segments[0].distance_nm == Catch::Approx(20.5));

  // 航路段逐段映射。
  CHECK(plan.segments[1].kind == px::SegmentKind::kEnroute);
  CHECK(plan.segments[1].via == "W80");
  CHECK(plan.segments[1].distance_nm == Catch::Approx(60.0));

  // DCT 单独段。
  CHECK(plan.segments[2].kind == px::SegmentKind::kEnroute);
  CHECK(plan.segments[2].via == "DCT");
  CHECK(plan.segments[2].distance_nm == Catch::Approx(25.0));

  // 连续 STAR legs 聚合为一段（距离求和 18.0 + 12.0）。
  CHECK(plan.segments[3].kind == px::SegmentKind::kStar);
  CHECK(plan.segments[3].from_ident == "MAKET");
  CHECK(plan.segments[3].to_ident == "PEK04");
  CHECK(plan.segments[3].distance_nm == Catch::Approx(30.0));
}

TEST_CASE("FromBf: DCT-to-IAF 进近段识别", "[flightplan][unit]") {
  bf::Route r;
  r.legs = {
      {"ZUCK", "GURUN", "SID", 12.5, {}},
      {"GURUN", "ZBAA", "DCT", 850.0, {}},
  };
  r.arr_connection = bf::ConnectionKind::kTerminalTransition;
  r.approach = "X18";
  r.approach_iaf = "ZBAA";

  const auto plan = px::FromBf(r);

  REQUIRE(plan.segments.size() == 2);
  CHECK(plan.segments[1].kind == px::SegmentKind::kApproach);
  CHECK(plan.segments[1].via == "DCT");
  CHECK(plan.segments[1].distance_nm == Catch::Approx(850.0));
}

TEST_CASE("FromBf: 航路中段 DCT 不标进近（仅到达侧尾部连续 DCT）",
          "[flightplan][unit]") {
  // 无 STAR（kTerminalTransition）时中段 DCT 属航路（其后再有非 DCT leg），
  // 只有尾部连续 DCT 序列（W80 之后）归属 kApproach。
  bf::Route r;
  r.legs = {
      {"ZUCK", "GURUN", "SID", 12.5, {}},
      {"GURUN", "NIRON", "DCT", 200.0, {}},  // 中段 DCT → kEnroute
      {"NIRON", "MAKET", "W80", 60.0, {}},
      {"MAKET", "ZBAA", "DCT", 850.0, {}},  // 尾部 DCT → kApproach
  };
  r.arr_connection = bf::ConnectionKind::kTerminalTransition;

  const auto plan = px::FromBf(r);

  REQUIRE(plan.segments.size() == 4);
  CHECK(plan.segments[1].kind == px::SegmentKind::kEnroute);
  CHECK(plan.segments[1].via == "DCT");
  CHECK(plan.segments[2].kind == px::SegmentKind::kEnroute);
  CHECK(plan.segments[3].kind == px::SegmentKind::kApproach);
}

TEST_CASE("FromBf: 到达侧连续 DCT 聚合为一段进近", "[flightplan][unit]") {
  // IAF 过渡与落地两段连续 DCT（bf 尾部 append IAF→机场 leg）：同语义
  // 进近段，聚合为一段（决策 2c 程序聚合段）。
  bf::Route r;
  r.legs = {
      {"ZUCK", "GURUN", "SID", 12.5, {}},
      {"GURUN", "TONIN", "DCT", 60.0, {}},
      {"TONIN", "ZBAA", "DCT", 40.0, {}},
  };
  r.arr_connection = bf::ConnectionKind::kTerminalTransition;

  const auto plan = px::FromBf(r);

  REQUIRE(plan.segments.size() == 2);
  CHECK(plan.segments[0].kind == px::SegmentKind::kSid);
  CHECK(plan.segments[1].kind == px::SegmentKind::kApproach);
  CHECK(plan.segments[1].from_ident == "GURUN");
  CHECK(plan.segments[1].to_ident == "ZBAA");
  CHECK(plan.segments[1].distance_nm == Catch::Approx(100.0));
}

TEST_CASE("FromBf: 全 DCT 航路仅最后一段标进近", "[flightplan][unit]") {
  // 无 SID 无航路无 STAR：尾部扫描会吞掉出发侧 DCT——防御只标最后一段。
  bf::Route r;
  r.legs = {
      {"ZUCK", "GURUN", "DCT", 200.0, {}},
      {"GURUN", "ZBAA", "DCT", 850.0, {}},
  };
  r.arr_connection = bf::ConnectionKind::kTerminalTransition;

  const auto plan = px::FromBf(r);

  REQUIRE(plan.segments.size() == 2);
  CHECK(plan.segments[0].kind == px::SegmentKind::kEnroute);
  CHECK(plan.segments[1].kind == px::SegmentKind::kApproach);
}

TEST_CASE("FromBf: 程序名与连接 token 字段搬运", "[flightplan][unit]") {
  auto r = MakeSampleRoute();
  r.sid = "DEEZZ5";
  r.star = "CAMRN5";
  r.dep_runway = "RW31L";
  r.arr_runway = "RW18R";
  r.dep_connection = bf::ConnectionKind::kProcedure;
  r.arr_connection = bf::ConnectionKind::kProcedure;

  const auto plan = px::FromBf(r);

  CHECK(plan.sid == "DEEZZ5");
  CHECK(plan.star == "CAMRN5");
  CHECK(plan.dep_runway == "RW31L");
  CHECK(plan.arr_runway == "RW18R");
  CHECK(plan.dep_connection == "procedure");
  CHECK(plan.arr_connection == "procedure");
}

TEST_CASE("FromBf: 空 legs 点序列 segment_index 哨兵", "[flightplan][unit]") {
  // 单顶点路径可产生 legs 空而 points 非空（bf MakeRoute 边界）；点归属
  // 无段可指——segment_index = -1 哨兵，消费端须先查 segments 空。
  bf::Route r;
  r.points = {
      {"ZUCK", {29.7192, 106.6417}},
      {"ZBAA", {40.0801, 116.5846}},
  };

  const auto plan = px::FromBf(r);

  REQUIRE(plan.segments.empty());
  REQUIRE(plan.points.size() == 2);
  CHECK(plan.points[0].segment_index == -1);
  CHECK(plan.points[0].via.empty());
  CHECK(plan.points[1].segment_index == -1);
}

TEST_CASE("FromBf: 空 legs + kTerminalTransition 无下溢",
          "[flightplan][unit]") {
  // 防御回归：ApproachStart 尾部扫描在 legs 空时若命中 kTerminalTransition
  // 分支，`size() - 1` 会下溢为 SIZE_MAX——须空守卫（行为上段序列空，
  // 点序列哨兵不变）。
  bf::Route r;
  r.arr_connection = bf::ConnectionKind::kTerminalTransition;
  r.points = {
      {"ZUCK", {29.7192, 106.6417}},
      {"ZBAA", {40.0801, 116.5846}},
  };

  const auto plan = px::FromBf(r);

  CHECK(plan.segments.empty());
  REQUIRE(plan.points.size() == 2);
  CHECK(plan.points[0].segment_index == -1);
}

TEST_CASE("配载: pax/cargo → ZFW 单向链", "[flightplan][unit]") {
  // 手算：140×75 = 10500；140×15 = 2100；payload = 12600；
  // ZFW = 41000 + 12600 = 53600。
  const px::Airframe airframe = MakePayloadAirframe();
  const auto result = px::ComputePayload(airframe, 140, 0.0);

  CHECK(result.pax_count == 140);
  CHECK(result.cargo_kg == 0.0);
  CHECK(result.payload_kg == Catch::Approx(12600.0));
  CHECK(result.zfw_kg == Catch::Approx(53600.0));
  // 配载计算链 ≠ ZFW 直接输入（决策 13 双入口可区分）。
  CHECK(result.from_zfw_input == false);
}

TEST_CASE("配载: 带货物", "[flightplan][unit]") {
  // 手算：100×75 + 100×15 + 500 = 9500；ZFW = 41000 + 9500 = 50500。
  const px::Airframe airframe = MakePayloadAirframe();
  const auto result = px::ComputePayload(airframe, 100, 500.0);

  CHECK(result.payload_kg == Catch::Approx(9500.0));
  CHECK(result.zfw_kg == Catch::Approx(50500.0));
}

TEST_CASE("配载: ZFW 直接输入跳过配载", "[flightplan][unit]") {
  const auto result = px::ComputePayloadFromZfw(54000.0);
  CHECK(result.zfw_kg == Catch::Approx(54000.0));
  CHECK(result.payload_kg == 0.0);
  CHECK(result.pax_count == 0);
  // 显式标志：与默认构造/空配载可区分（前端知道跳过配载链）。
  CHECK(result.from_zfw_input == true);
}

TEST_CASE("altitude: ICAO 半球规则层集（东行奇数/西行偶数）",
          "[flightplan][unit]") {
  // 东行 090：FL250-410 间奇数层（手算：250,270,...,410）。
  const auto east =
      px::CandidateLevels(px::AltitudeRule::kIcao, 90.0, 41000, 250, 410);
  REQUIRE(east.size() == 9);
  CHECK(east[0].fl == 250);
  CHECK(east[4].fl == 330);
  CHECK(east[8].fl == 410);

  // 西行 270：偶数层（260,280,...,400）。
  const auto west =
      px::CandidateLevels(px::AltitudeRule::kIcao, 270.0, 41000, 250, 410);
  REQUIRE(west.size() == 8);
  CHECK(west[0].fl == 260);
  CHECK(west[7].fl == 400);

  // ICAO 层米制等价 = FL×30.48。
  CHECK(east[4].meters == Catch::Approx(10058).margin(2));
}

TEST_CASE("altitude: 中国 RVSM 米制层集（东向/西向表）", "[flightplan][unit]") {
  // 东向 EAST 表（参考实现 FLAS）：8100m/26600ft、8900/29100、9500/31100、
  // 10100/33100、10700/35100、11300/37100、11900/39100——过滤 [250,410]
  // 后 266-391（41100ft 超带滤掉）。
  const auto east =
      px::CandidateLevels(px::AltitudeRule::kChina, 90.0, 41000, 250, 410);
  REQUIRE(east.size() == 7);
  CHECK(east[0].fl == 266);
  CHECK(east[0].meters == 8100);
  CHECK(east[2].fl == 311);
  CHECK(east[2].meters == 9500);
  CHECK(east.back().fl == 391);

  // 西向 WEST 表：7800m/25600ft、8400/27600、9200/30100、9800/32100、
  // 10400/34100、11000/36100、11600/38100、12200/40100。
  const auto west =
      px::CandidateLevels(px::AltitudeRule::kChina, 270.0, 41000, 250, 410);
  REQUIRE(west.size() == 8);
  CHECK(west[0].fl == 256);
  CHECK(west[0].meters == 7800);
  CHECK(west.back().fl == 401);
}

TEST_CASE("altitude: 负航向归一化（-90 ≡ 270 西行）", "[flightplan][unit]") {
  // fmod(-90, 360) = -90 曾被误判东行；归一化后 -90 应取西行偶层。
  const auto west =
      px::CandidateLevels(px::AltitudeRule::kIcao, -90.0, 41000, 250, 410);
  REQUIRE(west.size() == 8);
  CHECK(west[0].fl == 260);

  // 350 ≡ 西行；10 ≡ 东行。
  const auto west350 =
      px::CandidateLevels(px::AltitudeRule::kIcao, 350.0, 41000, 250, 410);
  CHECK(west350[0].fl == 260);
  const auto east10 =
      px::CandidateLevels(px::AltitudeRule::kIcao, 10.0, 41000, 250, 410);
  CHECK(east10[0].fl == 250);
}

TEST_CASE("altitude: 中国 RVSM 完整表（含中低空段）", "[flightplan][unit]") {
  // FLAS 表含高空段（8100m/26600ft 起）与中低空段。低带 [100,200] 手算
  // 逐层核对（east：108/128/148/167/187；west：118/138/157/177/197——
  // 注意 5100/16700、5700/18700、5400/17700、6000/19700 也在带内）。
  const auto east =
      px::CandidateLevels(px::AltitudeRule::kChina, 90.0, 41000, 100, 200);
  REQUIRE(east.size() == 5);
  CHECK(east[0].fl == 108);
  CHECK(east[0].meters == 3300);
  CHECK(east[1].fl == 128);
  CHECK(east[1].meters == 3900);
  CHECK(east[2].fl == 148);
  CHECK(east[2].meters == 4500);
  CHECK(east[3].fl == 167);
  CHECK(east[3].meters == 5100);
  CHECK(east[4].fl == 187);
  CHECK(east[4].meters == 5700);

  const auto west =
      px::CandidateLevels(px::AltitudeRule::kChina, 270.0, 41000, 100, 200);
  REQUIRE(west.size() == 5);
  CHECK(west[0].fl == 118);
  CHECK(west[0].meters == 3600);
  CHECK(west[1].fl == 138);
  CHECK(west[1].meters == 4200);
  CHECK(west[2].fl == 157);
  CHECK(west[2].meters == 4800);
  CHECK(west[3].fl == 177);
  CHECK(west[3].meters == 5400);
  CHECK(west[4].fl == 197);
  CHECK(west[4].meters == 6000);
}

TEST_CASE("altitude: 单点带短路（手动巡航高锁定 [FL,FL]）",
          "[flightplan][unit]") {
  // 决策 25：手动 cruise_fl → 高度带联动锁 [FL,FL]；决策 8：手动只校验
  // 提示不拦截——即使 FL 不在规则层表内/超升限也返回该单层（ICAO/China
  // 一致）。手算：FL300 × 30.48 = 9144。
  {
    const auto locked =
        px::CandidateLevels(px::AltitudeRule::kIcao, 90.0, 41000, 300, 300);
    REQUIRE(locked.size() == 1);
    CHECK(locked[0].fl == 300);
    CHECK(locked[0].meters == 9144);
  }
  {
    // 中国表内层：命中表值。
    const auto locked =
        px::CandidateLevels(px::AltitudeRule::kChina, 90.0, 41000, 266, 266);
    REQUIRE(locked.size() == 1);
    CHECK(locked[0].fl == 266);
    CHECK(locked[0].meters == 8100);
  }
  {
    // 中国表外层：回退 ICAO 近似。
    const auto locked =
        px::CandidateLevels(px::AltitudeRule::kChina, 270.0, 41000, 300, 300);
    REQUIRE(locked.size() == 1);
    CHECK(locked[0].meters == 9144);
  }
  {
    // 超升限：手动不拦截（提示走 PlanChecks，不在层集拦截）。
    const auto locked =
        px::CandidateLevels(px::AltitudeRule::kIcao, 90.0, 30000, 310, 310);
    REQUIRE(locked.size() == 1);
    CHECK(locked[0].fl == 310);
  }
  {
    // kAuto + 手动锁：用户覆盖优先于规则解析。
    const auto locked =
        px::CandidateLevels(px::AltitudeRule::kAuto, 90.0, 41000, 300, 300);
    REQUIRE(locked.size() == 1);
    CHECK(locked[0].fl == 300);
  }
}

TEST_CASE("altitude: kAuto 未解析规则不生成层集", "[flightplan][unit]") {
  // 决策 27：auto 由上层按起降场区域推断为 kChina/kIcao 后再调用——
  // 直接传入是上层缺陷，显式空返回优于静默按 ICAO 处理。
  CHECK(px::CandidateLevels(px::AltitudeRule::kAuto, 90.0, 41000, 250, 410)
            .empty());
}

TEST_CASE("altitude: 升限硬过滤", "[flightplan][unit]") {
  // 升限 360 → 滤掉 >FL360 的层。
  const auto east =
      px::CandidateLevels(px::AltitudeRule::kIcao, 90.0, 36000, 250, 410);
  REQUIRE(east.size() == 6);
  CHECK(east.back().fl == 350);

  // 中国西向：256/276/301/321/341/361/381/401——升限 36000ft 滤掉 36100+。
  const auto west =
      px::CandidateLevels(px::AltitudeRule::kChina, 270.0, 36000, 250, 410);
  REQUIRE(west.size() == 5);
  CHECK(west[0].fl == 256);
  CHECK(west.back().fl == 341);
}

TEST_CASE("FromBf: 空 legs 边界", "[flightplan][unit]") {
  const auto plan = px::FromBf(bf::Route{});
  CHECK(plan.segments.empty());
}

TEST_CASE("FromBf: 点序列与 segment_index", "[flightplan][unit]") {
  bf::Route r;
  r.points = {
      {"ZUCK", {29.7192, 106.6417}}, {"GURUN", {29.0, 105.0}},
      {"TONIN", {28.5, 104.2}},      {"NIRON", {27.8, 103.0}},
      {"MAKET", {27.0, 101.5}},      {"ZBAA", {40.0801, 116.5846}},
      {"PEK04", {40.1, 116.6}},
  };
  r.legs = {
      {"ZUCK", "GURUN", "SID", 12.5, {}},  {"GURUN", "TONIN", "SID", 8.0, {}},
      {"TONIN", "NIRON", "W80", 60.0, {}}, {"NIRON", "MAKET", "DCT", 25.0, {}},
      {"MAKET", "ZBAA", "STAR", 18.0, {}}, {"ZBAA", "PEK04", "STAR", 12.0, {}},
  };

  const auto plan = px::FromBf(r);

  REQUIRE(plan.points.size() == 7);
  // 首点（起飞机场）归属爬升段 0（决策 30③）。
  CHECK(plan.points[0].ident == "ZUCK");
  CHECK(plan.points[0].segment_index == 0);
  CHECK(plan.points[0].latitude == Catch::Approx(29.7192));
  CHECK(plan.points[0].longitude == Catch::Approx(106.6417));
  // SID 聚合段内点（含段末点）同属段 0。
  CHECK(plan.points[1].segment_index == 0);
  CHECK(plan.points[2].segment_index == 0);
  // 段边界推进：W80 → 1、DCT → 2、STAR → 3。
  CHECK(plan.points[3].segment_index == 1);
  CHECK(plan.points[4].segment_index == 2);
  CHECK(plan.points[5].segment_index == 3);
  // 最后点（落地机场）归属末段。
  CHECK(plan.points[6].segment_index == 3);
  CHECK(plan.points[6].ident == "PEK04");
}
