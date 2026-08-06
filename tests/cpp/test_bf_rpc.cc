// SPDX-License-Identifier: MIT
// bf::service 薄桥接测试（决策 15/18：status → RpcResult 映射、错误文本
// 提取）。FromBfHandlerResult 为纯函数——transport 无关可测；MakeBfHandlers
// 需真实 NavDatabase（navdata 集成测试 SKIP 纪律），只经编译链接验证。

#include <catch2/catch_test_macros.hpp>
#include <string>

#include "px/service/bf_rpc.h"

TEST_CASE("bf 桥接: 2xx → result 原样透传", "[bf_rpc][unit]") {
  const auto r = px::FromBfHandlerResult(200, R"({"icao":"ZBAA"})");
  REQUIRE(r.ok);
  CHECK(r.json == R"({"icao":"ZBAA"})");
  CHECK(r.error_code == 0);
}

TEST_CASE("bf 桥接: 4xx → 错误码 + JsonError 消息提取", "[bf_rpc][unit]") {
  const auto r = px::FromBfHandlerResult(422, R"({"error":"航路无解"})");
  REQUIRE(!r.ok);
  CHECK(r.error_code == 422);
  CHECK(r.error_message == "航路无解");
}

TEST_CASE("bf 桥接: 错误 body 非 JsonError → 原样消息", "[bf_rpc][unit]") {
  const auto r = px::FromBfHandlerResult(404, "not found");
  REQUIRE(!r.ok);
  CHECK(r.error_code == 404);
  CHECK(r.error_message == "not found");
}

TEST_CASE("bf 桥接: 空 body 错误 → 兜底文本", "[bf_rpc][unit]") {
  const auto r = px::FromBfHandlerResult(400, "");
  REQUIRE(!r.ok);
  CHECK(r.error_code == 400);
  CHECK(r.error_message == "handler error");
}

TEST_CASE("bf 桥接: 错误 body 非 JSON → 原样消息", "[bf_rpc][unit]") {
  // HasParseError() true 侧：非 JSON body 回退原样（404 的 "not found"
  // 是合法 JSON 串，走的是 IsObject() false 侧——两者之前都没测到）。
  const auto r = px::FromBfHandlerResult(500, "{broken");
  REQUIRE(!r.ok);
  CHECK(r.error_code == 500);
  CHECK(r.error_message == "{broken");
}

TEST_CASE("bf 桥接: JsonError 的 error 非字符串 → 原样消息", "[bf_rpc][unit]") {
  const auto r = px::FromBfHandlerResult(422, R"({"error":123})");
  REQUIRE(!r.ok);
  CHECK(r.error_code == 422);
  CHECK(r.error_message == R"({"error":123})");
}
