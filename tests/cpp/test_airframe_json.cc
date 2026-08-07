// SPDX-License-Identifier: MIT
// airframe JSON 转换与文件存取测试（S5b——决策 21/28/38；临时目录运行时
// 传参，不暴露本地环境）。

#include <rapidjson/document.h>

#include <catch2/catch_test_macros.hpp>
#include <filesystem>
#include <random>
#include <string>

#include "px/service/airframe_json.h"

namespace {

px::Airframe MakeAirframe() {
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
  a.cruise_speed_kt = 437;
  return a;
}

// 独立临时目录（每次调用唯一，结束清理）。
class TempDir {
 public:
  TempDir() {
    dir_ = std::filesystem::temp_directory_path() /
           ("pyxis-airframe-" + std::to_string(std::random_device{}()));
    std::filesystem::create_directories(dir_);
  }
  ~TempDir() { std::filesystem::remove_all(dir_); }
  std::string Path() const { return (dir_ / "airframes.json").string(); }

 private:
  std::filesystem::path dir_;
};

}  // namespace

TEST_CASE("airframe JSON: 合法对象解析 + perf_source token",
          "[airframe][unit]") {
  rapidjson::Document doc;
  doc.Parse(R"({"type":"A320","variant":"Fenix A320 CFM","perf_source":"openap",
                "dow_kg":41000,"mzfw_kg":61000,"mtow_kg":77000,"mlw_kg":66000,
                "service_ceiling_ft":39000,"unit_pax_kg":75,"unit_bag_kg":15,
                "cruise_speed_kt":437})");
  px::Airframe airframe;
  REQUIRE(px::ParseAirframe(doc, &airframe));
  CHECK(airframe.type == "A320");
  CHECK(airframe.perf_source == px::PerfSource::kOpenAp);
  REQUIRE(airframe.cruise_speed_kt.has_value());
  CHECK(*airframe.cruise_speed_kt == 437);
}

TEST_CASE("airframe JSON: 形状非法拒绝", "[airframe][unit]") {
  rapidjson::Document doc;
  px::Airframe airframe;
  doc.Parse(R"({"type":"A320"})");
  CHECK(!px::ParseAirframe(doc, &airframe));  // 缺字段
  doc.Parse(R"({"type":"A320","variant":"x","perf_source":"nope",
                "dow_kg":1,"mzfw_kg":1,"mtow_kg":1,"mlw_kg":1,
                "service_ceiling_ft":1,"unit_pax_kg":1,"unit_bag_kg":1})");
  CHECK(!px::ParseAirframe(doc, &airframe));  // perf_source 非法
  doc.Parse(R"([1,2])");
  CHECK(!px::ParseAirframe(doc, &airframe));  // 非对象
}

TEST_CASE("airframe JSON: 往返一致（含 cruise_speed_kt）", "[airframe][unit]") {
  const auto airframe = MakeAirframe();
  rapidjson::StringBuffer buffer;
  rapidjson::Writer<rapidjson::StringBuffer> writer(buffer);
  px::WriteAirframeJson(writer, airframe);
  rapidjson::Document doc;
  doc.Parse(buffer.GetString());
  px::Airframe parsed;
  REQUIRE(px::ParseAirframe(doc, &parsed));
  CHECK(parsed.type == airframe.type);
  CHECK(parsed.variant == airframe.variant);
  CHECK(parsed.perf_source == airframe.perf_source);
  CHECK(parsed.dow_kg == airframe.dow_kg);
  CHECK(parsed.cruise_speed_kt == airframe.cruise_speed_kt);
}

TEST_CASE("airframe 文件: 缺失 → 空；Store→Load 回读一致", "[airframe][unit]") {
  TempDir tmp;
  CHECK(px::LoadAirframes(tmp.Path()).empty());  // 缺失 = 空档案
  CHECK(px::StoreAirframes(tmp.Path(), {MakeAirframe()}).has_value());
  const auto loaded = px::LoadAirframes(tmp.Path());
  REQUIRE(loaded.size() == 1);
  CHECK(loaded[0].type == "A320");
  CHECK(loaded[0].cruise_speed_kt == 437);
}
