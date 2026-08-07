// SPDX-License-Identifier: MIT
// plan handler 参数校验测试（S5a——决策 18/契约 7：缺字段 400、起降场
// 相同 422、k 越界 400、navdata 缺失 -32000 决策 47）。db=nullptr 上下
// 文驱动——校验错误在碰 db 前短路，可无真实数据测试；真实数据路径 SKIP。

#include <rapidjson/document.h>

#include <catch2/catch_test_macros.hpp>
#include <filesystem>
#include <random>
#include <string>
#include <unordered_map>

#include "px/service/plan_handlers.h"
#include "px/service/rpc_dispatch.h"

namespace {

px::RpcResult Call(
    const std::unordered_map<std::string, px::RpcHandler>& handlers,
    const char* method, const std::string& params_json) {
  const auto it = handlers.find(method);
  REQUIRE(it != handlers.end());
  rapidjson::Document params;
  params.Parse(params_json.c_str());
  return it->second(params);
}

// 独立临时目录（每用例唯一，结束清理；airframe/profile 文件 IO 测试）。
class TempDir {
 public:
  TempDir() {
    dir_ = std::filesystem::temp_directory_path() /
           ("pyxis-airf-" + std::to_string(std::random_device{}()));
    std::filesystem::create_directories(dir_);
  }
  ~TempDir() { std::filesystem::remove_all(dir_); }
  std::string Path() const { return dir_.string(); }

 private:
  std::filesystem::path dir_;
};

}  // namespace

TEST_CASE("plan.routes: 缺 departure/arrival → 400", "[plan][unit]") {
  const auto handlers = px::MakePlanHandlers({});  // db=nullptr
  {
    const auto r = Call(handlers, "plan.routes", R"({"arrival":"KJFK"})");
    CHECK(!r.ok);
    CHECK(r.error_code == 400);
  }
  {
    const auto r = Call(handlers, "plan.routes", R"({"departure":"KLAX"})");
    CHECK(!r.ok);
    CHECK(r.error_code == 400);
  }
}

TEST_CASE("plan.routes: 起降场相同 → 422", "[plan][unit]") {
  const auto handlers = px::MakePlanHandlers({});
  const auto r =
      Call(handlers, "plan.routes", R"({"departure":"KLAX","arrival":"KLAX"})");
  CHECK(!r.ok);
  CHECK(r.error_code == 422);
}

TEST_CASE("plan.routes: k 非法 → 400", "[plan][unit]") {
  const auto handlers = px::MakePlanHandlers({});
  {
    const auto r = Call(handlers, "plan.routes",
                        R"({"departure":"KLAX","arrival":"KJFK","k":0})");
    CHECK(!r.ok);
    CHECK(r.error_code == 400);
  }
  {
    const auto r = Call(handlers, "plan.routes",
                        R"({"departure":"KLAX","arrival":"KJFK","k":999})");
    CHECK(!r.ok);
    CHECK(r.error_code == 400);
  }
  {
    const auto r = Call(handlers, "plan.routes",
                        R"({"departure":"KLAX","arrival":"KJFK","k":"3"})");
    CHECK(!r.ok);
    CHECK(r.error_code == 400);
  }
}

TEST_CASE("plan.routes: 高度带 min_fl > max_fl → 400", "[plan][unit]") {
  const auto handlers = px::MakePlanHandlers({});
  const auto r = Call(handlers, "plan.routes",
                      R"({"departure":"KLAX","arrival":"KJFK",
                          "min_fl":410,"max_fl":250})");
  CHECK(!r.ok);
  CHECK(r.error_code == 400);
}

TEST_CASE("plan.routes: 校验通过 + navdata 缺失 → -32000（决策 47）",
          "[plan][unit]") {
  const auto handlers = px::MakePlanHandlers({});  // db=nullptr
  const auto r =
      Call(handlers, "plan.routes", R"({"departure":"KLAX","arrival":"KJFK"})");
  CHECK(!r.ok);
  CHECK(r.error_code == -32000);
}

TEST_CASE("plan.alternates: 缺 arrival → 400；非法参数 → 400", "[plan][unit]") {
  const auto handlers = px::MakePlanHandlers({});
  {
    const auto r = Call(handlers, "plan.alternates", R"({})");
    CHECK(!r.ok);
    CHECK(r.error_code == 400);
  }
  {
    const auto r = Call(handlers, "plan.alternates",
                        R"({"arrival":"KLAX","max_distance_nm":0})");
    CHECK(!r.ok);
    CHECK(r.error_code == 400);
  }
}

TEST_CASE("plan.alternates: navdata 缺失 → -32000", "[plan][unit]") {
  const auto handlers = px::MakePlanHandlers({});
  const auto r = Call(handlers, "plan.alternates", R"({"arrival":"KLAX"})");
  CHECK(!r.ok);
  CHECK(r.error_code == -32000);
}

TEST_CASE("list_cycles: 有周期 → [cycle]；无 → 空数组", "[plan][unit]") {
  {
    px::PlanContext ctx;
    ctx.cycle = 2601;
    const auto handlers = px::MakePlanHandlers(ctx);
    const auto r = Call(handlers, "list_cycles", R"({})");
    REQUIRE(r.ok);
    rapidjson::Document doc;
    doc.Parse(r.json.c_str());
    REQUIRE(doc["cycles"].IsArray());
    REQUIRE(doc["cycles"].Size() == 1);
    CHECK(doc["cycles"][0].GetUint() == 2601);
  }
  {
    const auto handlers = px::MakePlanHandlers({});
    const auto r = Call(handlers, "list_cycles", R"({})");
    REQUIRE(r.ok);
    rapidjson::Document doc;
    doc.Parse(r.json.c_str());
    CHECK(doc["cycles"].Empty());
  }
}

TEST_CASE("透传端点: navdata 缺失也注册且返回 -32000", "[plan][unit]") {
  const auto handlers = px::MakePlanHandlers({});  // db=nullptr
  const auto it = handlers.find("lookup_airports");
  REQUIRE(it != handlers.end());
  const auto r = Call(handlers, "lookup_airports", R"({"ids":["KLAX"]})");
  CHECK(!r.ok);
  CHECK(r.error_code == -32000);
}

// ---- 7b：plan.generate（决策 8/14/23/26 + 契约；无 db 校验短路） ----

constexpr const char* kValidAirframe =
    R"({"type":"A320","variant":"Fenix A320 CFM","perf_source":"lnm",
        "dow_kg":41000,"mzfw_kg":61000,"mtow_kg":77000,"mlw_kg":66000,
        "service_ceiling_ft":39000,"unit_pax_kg":75,"unit_bag_kg":15})";

TEST_CASE("plan.generate: 缺 route_string/airframe → 400", "[plan][unit]") {
  const auto handlers = px::MakePlanHandlers({});
  {
    const auto r = Call(handlers, "plan.generate",
                        R"({"airframe":)" + std::string(kValidAirframe) + "}");
    CHECK(!r.ok);
    CHECK(r.error_code == 400);
  }
  {
    const auto r = Call(handlers, "plan.generate",
                        R"({"route_string":"ZUCK TONIN W80 MAKET ZBAA"})");
    CHECK(!r.ok);
    CHECK(r.error_code == 400);
  }
}

TEST_CASE("plan.generate: airframe 校验失败 → 400", "[plan][unit]") {
  const auto handlers = px::MakePlanHandlers({});
  const auto r = Call(handlers, "plan.generate",
                      R"({"route_string":"ZUCK TONIN ZBAA",
                          "airframe":{"type":"A320","variant":"x","dow_kg":90000}})");
  CHECK(!r.ok);
  CHECK(r.error_code == 400);
}

TEST_CASE("plan.generate: pax 与 zfw 互斥 → 400", "[plan][unit]") {
  const auto handlers = px::MakePlanHandlers({});
  const auto r = Call(handlers, "plan.generate",
                      R"({"route_string":"ZUCK TONIN ZBAA",
                          "airframe":)" +
                          std::string(kValidAirframe) +
                          R"(,"pax_count":10,"zfw_kg":50000})");
  CHECK(!r.ok);
  CHECK(r.error_code == 400);
}

TEST_CASE("plan.generate: cruise_fl 越界 → 400", "[plan][unit]") {
  const auto handlers = px::MakePlanHandlers({});
  const auto r = Call(handlers, "plan.generate",
                      R"({"route_string":"ZUCK TONIN ZBAA",
                          "airframe":)" +
                          std::string(kValidAirframe) + R"(,"cruise_fl":700})");
  CHECK(!r.ok);
  CHECK(r.error_code == 400);
}

TEST_CASE("plan.generate: 无配载输入 → 400（审查修复：缺省 0 假数据）",
          "[plan][unit]") {
  const auto handlers = px::MakePlanHandlers({});
  const auto r = Call(handlers, "plan.generate",
                      R"({"route_string":"ZUCK TONIN ZBAA",
                          "airframe":)" +
                          std::string(kValidAirframe) + "}");
  CHECK(!r.ok);
  CHECK(r.error_code == 400);
}

TEST_CASE("plan.generate: min_fl 类型错/越界 → 400（审查修复）",
          "[plan][unit]") {
  const auto handlers = px::MakePlanHandlers({});
  {
    const auto r = Call(handlers, "plan.generate",
                        R"({"route_string":"ZUCK TONIN ZBAA",
                            "airframe":)" +
                            std::string(kValidAirframe) +
                            R"(,"min_fl":"350","zfw_kg":50000})");
    CHECK(!r.ok);
    CHECK(r.error_code == 400);
  }
  {
    const auto r = Call(handlers, "plan.generate",
                        R"({"route_string":"ZUCK TONIN ZBAA",
                            "airframe":)" +
                            std::string(kValidAirframe) +
                            R"(,"min_fl":900,"max_fl":900,"zfw_kg":50000})");
    CHECK(!r.ok);
    CHECK(r.error_code == 400);
  }
}

TEST_CASE("plan.generate: zfw 低于 DOW → 400（审查修复：物理不可能值）",
          "[plan][unit]") {
  const auto handlers = px::MakePlanHandlers({});
  // kValidAirframe dow_kg = 41000——zfw 40000 < DOW。
  const auto r = Call(handlers, "plan.generate",
                      R"({"route_string":"ZUCK TONIN ZBAA",
                          "airframe":)" +
                          std::string(kValidAirframe) + R"(,"zfw_kg":40000})");
  CHECK(!r.ok);
  CHECK(r.error_code == 400);
}

// ---- profile 四端点（决策 55：data_dir/profiles.json） ----

TEST_CASE("profile: upsert → list/get 回读；delete → 404/空", "[plan][unit]") {
  TempDir tmp;
  px::PlanContext ctx;
  ctx.data_dir = tmp.Path();
  const auto handlers = px::MakePlanHandlers(ctx);

  auto r = Call(handlers, "profile.list", "{}");
  REQUIRE(r.ok);
  REQUIRE(r.json == "[]");

  r = Call(handlers, "profile.upsert",
           R"({"profile":{"name":"默认","k":8,"level":"high",
                          "min_fl":300,"max_fl":410,
                          "avoid_waypoints":["TONIN"]}})");
  REQUIRE(r.ok);
  {
    rapidjson::Document doc;
    doc.Parse(r.json.c_str());
    CHECK(std::string(doc["name"].GetString()) == "默认");
    CHECK(doc["k"].GetInt() == 8);
  }

  r = Call(handlers, "profile.get", R"({"name":"默认"})");
  REQUIRE(r.ok);
  {
    rapidjson::Document doc;
    doc.Parse(r.json.c_str());
    CHECK(std::string(doc["level"].GetString()) == "high");
    CHECK(doc["min_fl"].GetInt() == 300);
    REQUIRE(doc["avoid_waypoints"].IsArray());
    CHECK(doc["avoid_waypoints"][0].GetString() == std::string("TONIN"));
  }

  // upsert 覆盖同 name。
  r = Call(handlers, "profile.upsert", R"({"profile":{"name":"默认","k":3}})");
  REQUIRE(r.ok);
  r = Call(handlers, "profile.list", "{}");
  REQUIRE(r.ok);
  {
    rapidjson::Document doc;
    doc.Parse(r.json.c_str());
    REQUIRE(doc.Size() == 1);
    CHECK(doc[0]["k"].GetInt() == 3);
  }

  r = Call(handlers, "profile.delete", R"({"name":"默认"})");
  REQUIRE(r.ok);
  r = Call(handlers, "profile.delete", R"({"name":"默认"})");
  CHECK(!r.ok);
  CHECK(r.error_code == 404);
  r = Call(handlers, "profile.list", "{}");
  REQUIRE(r.ok);
  REQUIRE(r.json == "[]");
}

TEST_CASE("profile: 校验 400（name/k/level/min-max）与无 data_dir -32000",
          "[plan][unit]") {
  {
    const auto handlers = px::MakePlanHandlers({});  // 无 data_dir
    const auto r = Call(handlers, "profile.list", "{}");
    CHECK(!r.ok);
    CHECK(r.error_code == -32000);
  }
  TempDir tmp;
  px::PlanContext ctx;
  ctx.data_dir = tmp.Path();
  const auto handlers = px::MakePlanHandlers(ctx);
  {
    const auto r =
        Call(handlers, "profile.upsert", R"({"profile":{"name":""}})");
    CHECK(!r.ok);
    CHECK(r.error_code == 400);
  }
  {
    const auto r =
        Call(handlers, "profile.upsert", R"({"profile":{"name":"x","k":99}})");
    CHECK(!r.ok);
    CHECK(r.error_code == 400);
  }
  {
    const auto r = Call(handlers, "profile.upsert",
                        R"({"profile":{"name":"x","level":"weird"}})");
    CHECK(!r.ok);
    CHECK(r.error_code == 400);
  }
  {
    const auto r =
        Call(handlers, "profile.upsert",
             R"({"profile":{"name":"x","min_fl":410,"max_fl":250}})");
    CHECK(!r.ok);
    CHECK(r.error_code == 400);
  }
  {
    const auto r = Call(handlers, "profile.get", R"({})");
    CHECK(!r.ok);
    CHECK(r.error_code == 400);
  }
}

// ---- plan.analyze（决策 54：航路合法性检查；bf ParseRoute 语义解析） ----

TEST_CASE("plan.analyze: 缺 route_string / 非字符串 → 400", "[plan][unit]") {
  const auto handlers = px::MakePlanHandlers({});
  {
    const auto r = Call(handlers, "plan.analyze", R"({})");
    CHECK(!r.ok);
    CHECK(r.error_code == 400);
  }
  {
    const auto r = Call(handlers, "plan.analyze", R"({"route_string":42})");
    CHECK(!r.ok);
    CHECK(r.error_code == 400);
  }
}

TEST_CASE("plan.analyze: 校验通过 + navdata 缺失 → -32000（决策 47）",
          "[plan][unit]") {
  const auto handlers = px::MakePlanHandlers({});
  const auto r =
      Call(handlers, "plan.analyze", R"({"route_string":"ZUCK TONIN ZBAA"})");
  CHECK(!r.ok);
  CHECK(r.error_code == -32000);
}

TEST_CASE("plan.generate: 校验通过 + navdata 缺失 → -32000", "[plan][unit]") {
  const auto handlers = px::MakePlanHandlers({});
  const auto r = Call(handlers, "plan.generate",
                      R"({"route_string":"ZUCK TONIN ZBAA",
                          "airframe":)" +
                          std::string(kValidAirframe) + R"(,"zfw_kg":50000})");
  CHECK(!r.ok);
  CHECK(r.error_code == -32000);
}

// ---- 7b：plan.export（决策 17：前端回传 FlightPlan JSON + format） ----

constexpr const char* kValidFlightplan =
    R"({"route":{"points":[
          {"ident":"ZUCK","lat":29.7192,"lon":106.6417,"segment_index":0},
          {"ident":"TONIN","lat":28.5,"lon":104.2,"segment_index":1},
          {"ident":"ZBAA","lat":40.0801,"lon":116.5846,"segment_index":2}]},
        "altitude":{"fl":350}})";

TEST_CASE("plan.export: 缺 format / 不支持 → 400", "[plan][unit]") {
  const auto handlers = px::MakePlanHandlers({});
  {
    const auto r = Call(handlers, "plan.export", R"({"flightplan":{}})");
    CHECK(!r.ok);
    CHECK(r.error_code == 400);
  }
  {
    const auto r = Call(handlers, "plan.export",
                        R"({"flightplan":)" + std::string(kValidFlightplan) +
                            R"(,"format":"fsx"})");
    CHECK(!r.ok);
    CHECK(r.error_code == 400);
  }
}

TEST_CASE("plan.export: 缺 flightplan / 形状非法 → 400", "[plan][unit]") {
  const auto handlers = px::MakePlanHandlers({});
  {
    const auto r = Call(handlers, "plan.export", R"({"format":"msfs2024"})");
    CHECK(!r.ok);
    CHECK(r.error_code == 400);
  }
  {
    const auto r = Call(handlers, "plan.export",
                        R"({"format":"msfs2024","flightplan":{"route":{}}})");
    CHECK(!r.ok);
    CHECK(r.error_code == 400);
  }
}

TEST_CASE("plan.export: 合法请求 → 200 + {format, filename, content}（XML）",
          "[plan][unit]") {
  const auto handlers = px::MakePlanHandlers({});
  const auto r = Call(handlers, "plan.export",
                      R"({"format":"msfs2024","flightplan":)" +
                          std::string(kValidFlightplan) + "}");
  REQUIRE(r.ok);
  rapidjson::Document doc;
  doc.Parse(r.json.c_str());
  REQUIRE(!doc.HasParseError());
  CHECK(std::string(doc["format"].GetString()) == "msfs2024");
  CHECK(std::string(doc["filename"].GetString()) == "pyxis-ZUCK-ZBAA.pln");
  CHECK(doc["content"].GetString() != nullptr);
  CHECK(std::string(doc["content"].GetString()).find("<FlightPlan") !=
        std::string::npos);
}

// ---- 7c：airframe 四端点（决策 21：data_dir 文件 IO，临时目录） ----

TEST_CASE("airframe: upsert → list/get 回读；delete → 空", "[plan][unit]") {
  TempDir tmp;
  px::PlanContext ctx;
  ctx.data_dir = tmp.Path();
  const auto handlers = px::MakePlanHandlers(ctx);

  auto r = Call(handlers, "airframe.list", "{}");
  REQUIRE(r.ok);
  REQUIRE(r.json == "[]");  // 空档案

  r = Call(handlers, "airframe.upsert",
           R"({"airframe":)" + std::string(kValidAirframe) + "}");
  REQUIRE(r.ok);

  r = Call(handlers, "airframe.get",
           R"({"type":"A320","variant":"Fenix A320 CFM"})");
  REQUIRE(r.ok);
  rapidjson::Document doc;
  doc.Parse(r.json.c_str());
  CHECK(std::string(doc["type"].GetString()) == "A320");
  CHECK(std::string(doc["perf_source"].GetString()) == "lnm");

  r = Call(handlers, "airframe.list", "{}");
  REQUIRE(r.ok);
  doc.Parse(r.json.c_str());
  REQUIRE(doc.Size() == 1);

  r = Call(handlers, "airframe.delete",
           R"({"type":"A320","variant":"Fenix A320 CFM"})");
  REQUIRE(r.ok);
  r = Call(handlers, "airframe.list", "{}");
  REQUIRE(r.ok);
  REQUIRE(r.json == "[]");
}

TEST_CASE("airframe: get 不存在 404；upsert 非法 400；无 data_dir -32000",
          "[plan][unit]") {
  {
    const auto handlers = px::MakePlanHandlers({});  // 无 data_dir
    const auto r = Call(handlers, "airframe.list", "{}");
    CHECK(!r.ok);
    CHECK(r.error_code == -32000);
  }
  TempDir tmp;
  px::PlanContext ctx;
  ctx.data_dir = tmp.Path();
  const auto handlers = px::MakePlanHandlers(ctx);
  {
    const auto r =
        Call(handlers, "airframe.get", R"({"type":"XXX","variant":"YYY"})");
    CHECK(!r.ok);
    CHECK(r.error_code == 404);
  }
  {
    const auto r = Call(handlers, "airframe.upsert",
                        R"({"airframe":{"type":"A320","variant":"x",
                                        "dow_kg":90000}})");
    CHECK(!r.ok);
    CHECK(r.error_code == 400);
  }
  {
    const auto r = Call(handlers, "airframe.get", R"({})");
    CHECK(!r.ok);
    CHECK(r.error_code == 400);
  }
}
