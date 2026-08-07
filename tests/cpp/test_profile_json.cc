// SPDX-License-Identifier: MIT
// profile JSON 转换与 profiles.json 文件存取测试（决策 55：偏好后端
// profile——airframe 同款 Load/Store + 原子改名模式）。

#include <catch2/catch_test_macros.hpp>
#include <filesystem>
#include <fstream>
#include <random>

#include "px/core/error.h"
#include "px/service/profile_json.h"

namespace {

// 独立临时目录（每用例唯一，结束清理）。
class TempDir {
 public:
  TempDir() {
    dir_ = std::filesystem::temp_directory_path() /
           ("pyxis-prof-" + std::to_string(std::random_device{}()));
    std::filesystem::create_directories(dir_);
  }
  ~TempDir() { std::filesystem::remove_all(dir_); }
  std::string Path() const { return dir_.string(); }

 private:
  std::filesystem::path dir_;
};

TEST_CASE("profile JSON: 全字段往返（可选字段序列化）") {
  px::Profile profile;
  profile.name = "默认";
  profile.k = 8;
  profile.level = "high";
  profile.min_fl = 300;
  profile.max_fl = 410;
  profile.avoid_waypoints = {"TONIN", "MAKET"};

  rapidjson::StringBuffer buffer;
  rapidjson::Writer<rapidjson::StringBuffer> writer(buffer);
  px::WriteProfileJson(writer, profile);

  rapidjson::Document doc;
  REQUIRE_FALSE(doc.Parse(buffer.GetString()).HasParseError());
  CHECK(std::string(doc["name"].GetString()) == "默认");
  CHECK(doc["k"].GetInt() == 8);
  CHECK(std::string(doc["level"].GetString()) == "high");
  CHECK(doc["min_fl"].GetInt() == 300);
  CHECK(doc["max_fl"].GetInt() == 410);
  REQUIRE(doc["avoid_waypoints"].IsArray());
  CHECK(doc["avoid_waypoints"].Size() == 2);

  px::Profile parsed;
  REQUIRE(px::ParseProfile(doc, &parsed));
  CHECK(parsed.name == profile.name);
  CHECK(parsed.k == profile.k);
  CHECK(parsed.level == profile.level);
  CHECK(parsed.min_fl == profile.min_fl);
  CHECK(parsed.max_fl == profile.max_fl);
  CHECK(parsed.avoid_waypoints == profile.avoid_waypoints);
}

TEST_CASE("profile JSON: 可选字段缺省解析") {
  rapidjson::Document doc;
  doc.Parse(R"({"name":"简单"})");
  px::Profile parsed;
  REQUIRE(px::ParseProfile(doc, &parsed));
  CHECK(parsed.name == "简单");
  CHECK(!parsed.k.has_value());
  CHECK(!parsed.level.has_value());
  CHECK(parsed.avoid_waypoints.empty());
}

TEST_CASE("profile JSON: 形状非法返回 false（字段错不崩溃）") {
  px::Profile parsed;
  {
    rapidjson::Document doc;
    doc.Parse(R"({"k":"8"})");  // 缺 name + k 类型错
    CHECK(!px::ParseProfile(doc, &parsed));
  }
  {
    rapidjson::Document doc;
    doc.Parse(R"({"name":"x","avoid_waypoints":[42]})");
    CHECK(!px::ParseProfile(doc, &parsed));
  }
}

TEST_CASE("profiles.json: 存取往返 + 缺失文件空列表 + 原子写") {
  TempDir tmp;
  const std::string file = tmp.Path() + "/profiles.json";

  CHECK(px::LoadProfiles(file).empty());  // 缺失 → 空

  std::vector<px::Profile> profiles(2);
  profiles[0].name = "默认";
  profiles[1].name = "高原";
  profiles[1].k = 6;
  REQUIRE(px::StoreProfiles(file, profiles).has_value());

  const auto loaded = px::LoadProfiles(file);
  REQUIRE(loaded.size() == 2);
  CHECK(loaded[0].name == "默认");
  CHECK(loaded[1].name == "高原");
  CHECK(loaded[1].k == 6);

  // 覆盖写（upsert 语义）不残留旧内容。
  std::vector<px::Profile> single;
  single.push_back(profiles[0]);
  REQUIRE(px::StoreProfiles(file, single).has_value());
  REQUIRE(px::LoadProfiles(file).size() == 1);
}

}  // namespace
