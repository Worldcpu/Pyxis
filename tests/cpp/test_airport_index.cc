// SPDX-License-Identifier: MIT
// px_navdata 备降过滤测试（S1a——决策 12 修订：距离过滤 + 4 字 ICAO +
// 排除列表纯函数）。FilterAlternates 不依赖 bfdb（输入为内存索引条目）；
// 真实 bfdb 读取路径走 SKIP 纪律（集成 seam）。

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>
#include <filesystem>
#include <fstream>
#include <random>
#include <string>
#include <vector>

#include "px/core/error.h"
#include "px/module/navdata/airport_index.h"

namespace {

px::AirportEntry Airport(std::string icao, double lat, double lon) {
  px::AirportEntry e;
  e.icao = std::move(icao);
  e.coord = {lat, lon};
  return e;
}

constexpr px::GeoCoord kArrival{0.0, 0.0};  // 假想到达场（赤道）

TEST_CASE("DistanceNm: 大圆距离已知真源（KLAX→KSFO ≈ 294NM）") {
  const px::GeoCoord klax{33.9425, -118.4081};
  const px::GeoCoord ksfo{37.6213, -122.3790};
  CHECK(px::DistanceNm(klax, ksfo) == Catch::Approx(294.0).margin(5.0));
  CHECK(px::DistanceNm(ksfo, klax) ==
        Catch::Approx(294.0).margin(5.0));  // 对称
  CHECK(px::DistanceNm(klax, klax) == Catch::Approx(0.0).margin(1e-6));  // 零距
}

TEST_CASE("FilterAlternates: 距离过滤 + 升序（1° 纬度 ≈ 60NM）") {
  // 独立真源：1 NM = 1 弧分，球面弧长 ≈ R_nm × 弧度。
  const std::vector<px::AirportEntry> airports = {
      Airport("AAAA", 2.0, 0.0),   // ≈120NM
      Airport("BBBB", 5.0, 0.0),   // ≈300NM
      Airport("CCCC", 8.0, 0.0),   // ≈480NM
      Airport("DDDD", 20.0, 0.0),  // ≈1200NM
  };
  px::AlternatesParams params;
  params.max_distance_nm = 400.0;
  params.limit = 5;

  const auto result = px::FilterAlternates(airports, kArrival, params);
  REQUIRE(result.size() == 2);
  CHECK(result[0].icao == "AAAA");
  CHECK(result[1].icao == "BBBB");
  CHECK(result[0].distance_nm < result[1].distance_nm);  // 距离升序
  CHECK(result[0].distance_nm == Catch::Approx(120.0).margin(2.0));
  CHECK(result[1].distance_nm == Catch::Approx(300.0).margin(2.0));
}

TEST_CASE("FilterAlternates: 仅 4 字 ICAO（FAA LID 等短码剔除）") {
  const std::vector<px::AirportEntry> airports = {
      Airport("KLAX", 2.0, 0.0),
      Airport("LAX", 2.1, 0.0),  // 3 字 LID 剔除
      Airport("B45", 2.2, 0.0),  // 3 字剔除
      Airport("ZUUU", 2.3, 0.0),
  };
  px::AlternatesParams params;
  params.max_distance_nm = 400.0;

  const auto result = px::FilterAlternates(airports, kArrival, params);
  REQUIRE(result.size() == 2);
  CHECK(result[0].icao == "KLAX");
  CHECK(result[1].icao == "ZUUU");
}

TEST_CASE("FilterAlternates: 排除列表 + 截断 limit") {
  std::vector<px::AirportEntry> airports;
  for (int i = 0; i < 8; ++i) {
    // 4 字 ICAO（决策 12 修订：短码剔除）。
    airports.push_back(Airport("EA0" + std::to_string(i), 1.0 + i * 0.5, 0.0));
  }
  px::AlternatesParams params;
  params.max_distance_nm = 400.0;
  params.avoid_icaos = {"EA01"};
  params.limit = 3;

  const auto result = px::FilterAlternates(airports, kArrival, params);
  REQUIRE(result.size() == 3);
  CHECK(result[0].icao == "EA00");
  CHECK(result[1].icao == "EA02");  // EA01 被排除
  CHECK(result[2].icao == "EA03");
  for (size_t i = 1; i < result.size(); ++i) {
    CHECK(result[i - 1].distance_nm <= result[i].distance_nm);  // 全程升序
  }
}

TEST_CASE("FilterAlternates: 无候选（距离内无机场）返回空") {
  const std::vector<px::AirportEntry> airports = {
      Airport("FAR", 50.0, 0.0),
  };
  px::AlternatesParams params;
  const auto result = px::FilterAlternates(airports, kArrival, params);
  CHECK(result.empty());
}

TEST_CASE("FilterAlternates: 到达场自身排除（0 NM 假候选，审查修复）") {
  const std::vector<px::AirportEntry> airports = {
      Airport("ZUUU", 0.0, 0.0),  // 到达场自身（距离 0）
      Airport("ZUCK", 2.0, 0.0),  // ≈120NM
      Airport("ZBAA", 5.0, 0.0),  // ≈300NM
  };
  px::AlternatesParams params;
  params.exclude_icao = "ZUUU";
  const auto result = px::FilterAlternates(airports, kArrival, params);
  REQUIRE(result.size() == 2);
  CHECK(result[0].icao == "ZUCK");
  CHECK(result[0].distance_nm > 0.0);
}

// S1b：索引构建错误路径（不依赖真实 bfdb——空/损坏文件）。
TEST_CASE("AirportIndex::Open: 不存在的 bfdb 返回 kDataMissing") {
  const auto result =
      px::AirportIndex::Open("/nonexistent/pyxis-test/never.bfdb");
  REQUIRE(!result.has_value());
  CHECK(result.error().code == px::ErrorCode::kDataMissing);
}

TEST_CASE("AirportIndex::Open: 损坏的 bfdb 返回 kCacheCorrupt") {
  namespace fs = std::filesystem;
  // 测试运行时创建独立临时目录（不暴露本地开发环境）。
  const fs::path dir = fs::temp_directory_path() /
                       ("pyxis-test-" + std::to_string(std::random_device{}()));
  fs::create_directories(dir);
  const fs::path file = dir / "nav_2601.bfdb";
  {
    std::ofstream out(file, std::ios::binary);
    out << "garbage bytes, not a bfdb container";
  }
  const auto result = px::AirportIndex::Open(file.string());
  REQUIRE(!result.has_value());
  CHECK(result.error().code == px::ErrorCode::kCacheCorrupt);
  fs::remove_all(dir);
}

}  // namespace
