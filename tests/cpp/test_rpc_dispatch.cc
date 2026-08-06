// SPDX-License-Identifier: MIT
// JSON-RPC 2.0 分派测试（S4——决策 16/18：消息模型与错误码分区）。
// 用假 handler 表验证分派逻辑（transport 无关，不依赖 bf 数据库）。

#include <catch2/catch_test_macros.hpp>
#include <string>
#include <unordered_map>

#include "px/service/rpc_dispatch.h"

namespace {

using px::RpcHandler;
using px::RpcResult;

RpcResult OkResult(const std::string& body) { return {true, body, 0, ""}; }
RpcResult ErrResult(int code, const std::string& message = "") {
  return {false, "", code, message};
}

std::unordered_map<std::string, RpcHandler> MakeHandlers() {
  return {
      {"lookup_airports",
       [](const rapidjson::Value&) { return OkResult("{\"icao\":\"ZBAA\"}"); }},
      {"lookup_missing",
       [](const rapidjson::Value&) { return ErrResult(404, "查无此机场"); }},
  };
}

}  // namespace

TEST_CASE("RPC: 合法请求分派到 handler 并回显 id", "[rpc][unit]") {
  const auto resp = px::DispatchRpc(
      R"({"method":"lookup_airports","params":{"ids":["ZBAA"]},"id":7})",
      MakeHandlers());

  REQUIRE(resp.ok);
  // 响应 = result 包装 + 原 id 回显。
  CHECK(resp.json.find("\"id\":7") != std::string::npos);
  CHECK(resp.json.find("\"result\"") != std::string::npos);
  CHECK(resp.json.find("ZBAA") != std::string::npos);
}

TEST_CASE("RPC: 未知 method → -32601", "[rpc][unit]") {
  const auto resp = px::DispatchRpc(
      R"({"method":"no_such_method","params":{},"id":1})", MakeHandlers());
  REQUIRE(!resp.ok);
  CHECK(resp.error_code == -32601);
  CHECK(resp.json.find("\"id\":1") != std::string::npos);
}

TEST_CASE("RPC: 非法 JSON → -32700", "[rpc][unit]") {
  const auto resp = px::DispatchRpc("{not json", MakeHandlers());
  REQUIRE(!resp.ok);
  CHECK(resp.error_code == -32700);
}

TEST_CASE("RPC: 业务错误码透传 + 错误消息透传", "[rpc][unit]") {
  const auto resp = px::DispatchRpc(
      R"({"method":"lookup_missing","params":{"ids":["XXXX"]},"id":3})",
      MakeHandlers());
  REQUIRE(!resp.ok);
  CHECK(resp.error_code == 404);
  CHECK(resp.json.find("\"id\":3") != std::string::npos);
  // handler 的具体错误文本（决策 18：错误码 + 消息）不丢。
  CHECK(resp.json.find("查无此机场") != std::string::npos);
}

TEST_CASE("RPC: 缺 id/method → -32600", "[rpc][unit]") {
  const auto resp = px::DispatchRpc(R"({"params":{}})", MakeHandlers());
  REQUIRE(!resp.ok);
  CHECK(resp.error_code == -32600);
}

TEST_CASE("RPC: 结构合法但非请求 → -32600（非 -32700）", "[rpc][unit]") {
  // JSON 本身合法（非 parse error）；"hello"/42 是无效请求而非解析错误。
  const auto str = px::DispatchRpc("\"hello\"", MakeHandlers());
  REQUIRE(!str.ok);
  CHECK(str.error_code == -32600);
  const auto num = px::DispatchRpc("42", MakeHandlers());
  CHECK(num.error_code == -32600);
}

TEST_CASE("RPC: 大整数/浮点 id 回显（非 null）", "[rpc][unit]") {
  // int64 id（Date.now() 风格）与 double id 必须原样回显——客户端靠 id
  // 关联请求/响应。
  const auto big = px::DispatchRpc(
      R"({"method":"lookup_airports","params":{},"id":1700000000000})",
      MakeHandlers());
  REQUIRE(big.ok);
  CHECK(big.json.find("\"id\":1700000000000") != std::string::npos);

  const auto frac = px::DispatchRpc(
      R"({"method":"lookup_airports","params":{},"id":1.5})", MakeHandlers());
  REQUIRE(frac.ok);
  CHECK(frac.json.find("\"id\":1.5") != std::string::npos);
}

TEST_CASE("RPC: 通知（无 id）不返回响应", "[rpc][unit]") {
  // JSON-RPC §2.2：通知必须无响应——json 空串 = 调用方不发。
  const auto resp = px::DispatchRpc(
      R"({"method":"lookup_airports","params":{}})", MakeHandlers());
  CHECK(resp.ok);
  CHECK(resp.json.empty());

  // 未知方法通知同样静默（§2.2：通知对错误也不响应）。
  const auto unknown = px::DispatchRpc(R"({"method":"nope"})", MakeHandlers());
  CHECK(unknown.ok);
  CHECK(unknown.json.empty());
}

TEST_CASE("RPC: batch 数组逐元素分派", "[rpc][unit]") {
  const auto resp = px::DispatchRpc(
      R"([{"method":"lookup_airports","params":{},"id":1},
          {"method":"lookup_missing","params":{},"id":2}])",
      MakeHandlers());
  REQUIRE(resp.ok);
  // 数组响应：两个元素，各自成对。
  CHECK(resp.json.find("\"id\":1") != std::string::npos);
  CHECK(resp.json.find("\"id\":2") != std::string::npos);
  CHECK(resp.json.find("-32601") == std::string::npos);  // 两个都找到 handler
  CHECK(resp.json.find("查无此机场") != std::string::npos);
}

TEST_CASE("RPC: batch 中通知元素省略", "[rpc][unit]") {
  const auto resp = px::DispatchRpc(
      R"([{"method":"lookup_airports","params":{},"id":1},
          {"method":"lookup_airports","params":{}}])",
      MakeHandlers());
  REQUIRE(resp.ok);
  CHECK(resp.json.find("\"id\":1") != std::string::npos);
  // 数组只含 1 个响应对象（通知无占位）。
  CHECK(resp.json.find("\"id\":2") == std::string::npos);
}

TEST_CASE("RPC: batch 中无效元素 → -32600 条目", "[rpc][unit]") {
  const auto resp = px::DispatchRpc(
      R"([{"method":"lookup_airports","params":{},"id":1}, 42])",
      MakeHandlers());
  REQUIRE(resp.ok);
  CHECK(resp.json.find("\"id\":1") != std::string::npos);
  CHECK(resp.json.find("-32600") != std::string::npos);
}

TEST_CASE("RPC: 空 batch → -32600", "[rpc][unit]") {
  const auto resp = px::DispatchRpc("[]", MakeHandlers());
  REQUIRE(!resp.ok);
  CHECK(resp.error_code == -32600);
}

TEST_CASE("RPC: handler 返回数组 result（候选列表形状）", "[rpc][unit]") {
  // plan.routes 的 result 是候选数组——result 包装必须按原样嵌入
  // （RawValue 类型判定：数组首字符 '[' → kArrayType）。
  std::unordered_map<std::string, RpcHandler> handlers = {
      {"plan_routes",
       [](const rapidjson::Value&) {
         return OkResult(R"([{"index":0,"route_string":"A B"}])");
       }},
  };
  const auto resp = px::DispatchRpc(
      R"({"method":"plan_routes","params":{},"id":5})", handlers);
  REQUIRE(resp.ok);
  CHECK(resp.json.find("\"id\":5") != std::string::npos);
  // 数组 result 原样嵌入且 JSON 合法（解析成功即证明配对无缺）。
  rapidjson::Document doc;
  REQUIRE_FALSE(doc.Parse(resp.json.c_str()).HasParseError());
  REQUIRE(doc["result"].IsArray());
  CHECK(doc["result"][0]["route_string"].GetString() == std::string("A B"));
}
