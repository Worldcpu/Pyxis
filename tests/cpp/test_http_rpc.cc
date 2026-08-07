// SPDX-License-Identifier: MIT
// HTTP 适配纯函数测试（S4a——决策 43：单路径 POST /rpc、全 200 + error
// body；传输层错误——非 POST / 非 /rpc / body 非 JSON——HTTP 状态码）。

#include <rapidjson/document.h>

#include <catch2/catch_test_macros.hpp>
#include <string>
#include <unordered_map>

#include "px/service/http_rpc.h"
#include "px/service/rpc_dispatch.h"

namespace {

// 假 handler 表（S4a——分派逻辑经 test_rpc_dispatch 覆盖，此处验证
// HTTP 适配层的状态码/错误体映射）。
std::unordered_map<std::string, px::RpcHandler> MakeHandlers() {
  return {
      {"echo",
       [](const rapidjson::Value&) {
         return px::RpcResult{true, R"({"echoed":true})", 0, ""};
       }},
      {"boom",
       [](const rapidjson::Value&) {
         return px::RpcResult{false, "", 422, "no route"};
       }},
  };
}

rapidjson::Document Parse(const std::string& json) {
  rapidjson::Document doc;
  doc.Parse(json.c_str());
  return doc;
}

}  // namespace

TEST_CASE("http: POST /rpc 有效请求 → 200 + result", "[http][unit]") {
  const auto resp = px::HandleHttpRpc(
      "POST", "/rpc", R"({"jsonrpc":"2.0","id":1,"method":"echo","params":{}})",
      MakeHandlers());
  CHECK(resp.status == 200);
  const auto doc = Parse(resp.body);
  CHECK(doc["id"].GetInt() == 1);
  CHECK(doc["result"]["echoed"].GetBool());
}

TEST_CASE("http: 非 POST → 400 + 协议级错误", "[http][unit]") {
  const auto resp = px::HandleHttpRpc(
      "GET", "/rpc", R"({"jsonrpc":"2.0","id":1,"method":"echo"})",
      MakeHandlers());
  CHECK(resp.status == 400);
  const auto doc = Parse(resp.body);
  CHECK(doc["error"]["code"].GetInt() == -32600);
}

TEST_CASE("http: 非 /rpc 路径 → 404", "[http][unit]") {
  const auto resp = px::HandleHttpRpc(
      "POST", "/other", R"({"jsonrpc":"2.0","id":1,"method":"echo"})",
      MakeHandlers());
  CHECK(resp.status == 404);
}

TEST_CASE("http: body 非 JSON → 400 + -32700", "[http][unit]") {
  const auto resp =
      px::HandleHttpRpc("POST", "/rpc", "not json", MakeHandlers());
  CHECK(resp.status == 400);
  const auto doc = Parse(resp.body);
  CHECK(doc["error"]["code"].GetInt() == -32700);
}

TEST_CASE("http: 业务错误 → 200 + error body（决策 43 单通道）",
          "[http][unit]") {
  const auto resp = px::HandleHttpRpc(
      "POST", "/rpc", R"({"jsonrpc":"2.0","id":2,"method":"boom","params":{}})",
      MakeHandlers());
  CHECK(resp.status == 200);  // 错误语义在 body，不用 HTTP 状态码
  const auto doc = Parse(resp.body);
  CHECK(doc["id"].GetInt() == 2);
  CHECK(doc["error"]["code"].GetInt() == 422);
  CHECK(std::string(doc["error"]["message"].GetString()) == "no route");
}

TEST_CASE("http: 通知（无 id）→ 200 + 空 body", "[http][unit]") {
  const auto resp = px::HandleHttpRpc(
      "POST", "/rpc", R"({"jsonrpc":"2.0","method":"echo","params":{}})",
      MakeHandlers());
  CHECK(resp.status == 200);
  CHECK(resp.body.empty());
}
