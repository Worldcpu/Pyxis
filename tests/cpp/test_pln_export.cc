// SPDX-License-Identifier: MIT
// .PLN 导出测试（S7——决策 17：MSFS/FSX 格式，后端生成）。
// 坐标 DMS 期望值手算（29.7192° → 29°43'9.12"；106.6417° → 106°38'30.12"）。

#include <catch2/catch_test_macros.hpp>
#include <string>
#include <vector>

#include "px/module/flightplan/flight_plan.h"
#include "px/service/pln_export.h"

TEST_CASE("PLN: 十进制度 → DMS 格式化", "[pln][unit]") {
  CHECK(px::FormatDms(29.7192, true) == "N29°43'9.12\"");
  CHECK(px::FormatDms(106.6417, false) == "E106°38'30.12\"");
  CHECK(px::FormatDms(-33.9461, true) == "S33°56'45.96\"");
}

TEST_CASE("PLN: DMS 秒进位（59.99\" 不得溢出 60）", "[pln][unit]") {
  // 手算：29.999999° → 分 59.99994'、秒 59.9964" → 进位 → 30°0'0.00"。
  CHECK(px::FormatDms(29.999999, true) == "N30°0'0.00\"");
  // 跨分进位：43'59.995" → 44'0.00"。
  CHECK(px::FormatDms(44.733332, false) == "E44°44'0.00\"");
  // 负值同样进位。
  CHECK(px::FormatDms(-29.999999, true) == "S30°0'0.00\"");
}

TEST_CASE("PLN: XML 转义用户可控字段", "[pln][unit]") {
  const px::PlnExportParams params{
      "ZUCK & ZBAA <RETURN>",
      "IFR",
      35000.0,
      "ZUCK\"A",
      "ZBAA",
      29.7192,
      106.6417,
      40.0801,
      116.5846,
  };
  const std::vector<px::FlightPoint> points = {
      {"GURUN", "DCT", 29.0, 105.0, 0},
      {"TON'IN", "DCT", 28.5, 104.2, 1},
  };

  const auto xml = px::RenderPlnXml(params, points);

  // & < > 与引号在属性/元素文本中转义（非法 XML 会被 MSFS 拒载）。
  CHECK(xml.find("ZUCK &amp; ZBAA &lt;RETURN&gt;") != std::string::npos);
  CHECK(xml.find("<DepartureID>ZUCK&quot;A</DepartureID>") !=
        std::string::npos);
  CHECK(xml.find("<ATCWaypoint id=\"TON&apos;IN\">") != std::string::npos);
  CHECK(xml.find("ZUCK & ZBAA <RETURN>") == std::string::npos);
}

TEST_CASE("PLN: XML 生成含起降场与航路点", "[pln][unit]") {
  const px::PlnExportParams params{
      "ZUCK - ZBAA", "IFR",    35000.0, "ZUCK",   "ZBAA",
      29.7192,       106.6417, 40.0801, 116.5846,
  };
  const std::vector<px::FlightPoint> points = {
      {"GURUN", "DCT", 29.0, 105.0, 0},
      {"TONIN", "DCT", 28.5, 104.2, 1},
  };

  const auto xml = px::RenderPlnXml(params, points);

  CHECK(xml.find("<DepartureID>ZUCK</DepartureID>") != std::string::npos);
  CHECK(xml.find("<DestinationID>ZBAA</DestinationID>") != std::string::npos);
  CHECK(xml.find("<CruisingAltitude>35000</CruisingAltitude>") !=
        std::string::npos);
  CHECK(xml.find("<ATCWaypoint id=\"GURUN\">") != std::string::npos);
  CHECK(xml.find("<ATCWaypoint id=\"TONIN\">") != std::string::npos);
  // WorldPosition 含 DMS 坐标。
  CHECK(xml.find("N29°43'9.12\"") != std::string::npos);
  CHECK(xml.find("E105°0'0.00\"") != std::string::npos);
}
