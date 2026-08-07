// SPDX-License-Identifier: MIT
// JSON-RPC 2.0 分派实现（决策 16/18）。RapidJSON Writer 输出，自动转义。
#include "px/service/rpc_dispatch.h"

#include <rapidjson/document.h>
#include <rapidjson/stringbuffer.h>
#include <rapidjson/writer.h>

#include <cstring>
#include <optional>

namespace px {

namespace {

// JSON-RPC 标准错误码。
constexpr int kParseError = -32700;
constexpr int kInvalidRequest = -32600;
constexpr int kMethodNotFound = -32601;

using Writer = rapidjson::Writer<rapidjson::StringBuffer>;

// id 回显（§2.3：String/Number/Null 原样回显；对象/数组 id 非法 → null）。
void WriteId(Writer& writer, const rapidjson::Value* id) {
  if (id == nullptr) {
    writer.Null();
    return;
  }
  if (id->IsNull() || id->IsString() || id->IsInt() || id->IsUint() ||
      id->IsInt64() || id->IsUint64() || id->IsDouble()) {
    id->Accept(writer);
    return;
  }
  writer.Null();
}

// 组装 error 响应（含 id 回显；id 不可用时 null）。
std::string BuildErrorImpl(
    int code, const std::string& message,
    const rapidjson::Value* id) {  // px 命名空间（头声明）
  rapidjson::StringBuffer buffer;
  Writer writer(buffer);
  writer.StartObject();
  writer.Key("jsonrpc");
  writer.String("2.0");
  writer.Key("error");
  writer.StartObject();
  writer.Key("code");
  writer.Int(code);
  writer.Key("message");
  writer.String(message.c_str());
  writer.EndObject();
  writer.Key("id");
  WriteId(writer, id);
  writer.EndObject();
  return buffer.GetString();
}

// 组装 result 响应。result 可能是对象（{...}）或数组（[...]——如
// plan.routes 候选列表）：RawValue 的类型参数按首字符判定，保持类型
// 标记正确。
std::string BuildResult(const std::string& result_json,
                        const rapidjson::Value* id) {
  rapidjson::StringBuffer buffer;
  Writer writer(buffer);
  writer.StartObject();
  writer.Key("jsonrpc");
  writer.String("2.0");
  writer.Key("result");
  if (result_json.empty()) {
    // 空 result body → null 兜底（审查修复：RawValue 写 0 字节产出
    // 畸形 JSON "result""id"）。
    writer.Null();
  } else {
    const auto result_type =
        result_json[0] == '[' ? rapidjson::kArrayType : rapidjson::kObjectType;
    writer.RawValue(result_json.c_str(), result_json.size(), result_type);
  }
  writer.Key("id");
  WriteId(writer, id);
  writer.EndObject();
  return buffer.GetString();
}

// 单请求分派：nullopt = 通知（无 id，§2.2 必须无响应——无论成败）。
std::optional<RpcResult> DispatchOne(
    const rapidjson::Value& doc,
    const std::unordered_map<std::string, RpcHandler>& handlers) {
  const bool has_id = doc.HasMember("id");
  const auto* id = has_id ? &doc["id"] : nullptr;
  // JSON-RPC §2.0 版本校验（审查修复：jsonrpc 字段存在但非 "2.0" →
  // 无效请求，决策 18 结构无效语义；缺失时宽容向后兼容）。
  // 通知（无 id）豁免响应——§2.2 静默，与未知方法通知一致。
  if (doc.HasMember("jsonrpc") &&
      (!doc["jsonrpc"].IsString() ||
       std::strcmp(doc["jsonrpc"].GetString(), "2.0") != 0)) {
    if (!has_id) return std::nullopt;  // 通知：静默。
    return RpcResult{false,
                     BuildErrorImpl(kInvalidRequest, "invalid request", id),
                     kInvalidRequest, ""};
  }
  const auto* method = doc.HasMember("method") && doc["method"].IsString()
                           ? &doc["method"]
                           : nullptr;
  // 无效请求必须响应（id null）——通知豁免不适用。
  if (method == nullptr) {
    return RpcResult{false,
                     BuildErrorImpl(kInvalidRequest, "invalid request", id),
                     kInvalidRequest, ""};
  }

  const auto it = handlers.find(method->GetString());
  if (it == handlers.end()) {
    if (!has_id) return std::nullopt;  // 通知：未知方法静默。
    return RpcResult{false,
                     BuildErrorImpl(kMethodNotFound, "method not found", id),
                     kMethodNotFound, ""};
  }

  // 本项目 handler 契约：params 为对象（缺省空对象）。非对象（数组/
  // 标量/null）会触发 rapidjson 断言（handler 内 HasMember）——先拒。
  if (doc.HasMember("params") && !doc["params"].IsObject()) {
    if (!has_id) return std::nullopt;  // 通知：静默。
    return RpcResult{
        false, BuildErrorImpl(kInvalidRequest, "params must be an object", id),
        kInvalidRequest, ""};
  }
  const rapidjson::Value empty(rapidjson::kObjectType);
  const auto result =
      it->second(doc.HasMember("params") ? doc["params"] : empty);
  if (!has_id) return std::nullopt;  // 通知：成败均静默。
  if (result.ok) {
    return RpcResult{true, BuildResult(result.json, id), 0, ""};
  }

  // 业务错误：透传错误码与 handler 消息（空则兜底文本）。
  const std::string message =
      result.error_message.empty() ? "handler error" : result.error_message;
  return RpcResult{false, BuildErrorImpl(result.error_code, message, id),
                   result.error_code, message};
}

}  // namespace

RpcResult DispatchRpc(
    const std::string& request_json,
    const std::unordered_map<std::string, RpcHandler>& handlers) {
  rapidjson::Document doc;
  if (doc.Parse(request_json.c_str()).HasParseError()) {
    return {false, BuildErrorImpl(kParseError, "parse error", nullptr),
            kParseError, ""};
  }

  if (doc.IsArray()) {
    // batch：逐元素分派；通知元素无占位；空 batch 是无效请求（§2.0）。
    if (doc.Empty()) {
      return {false,
              BuildErrorImpl(kInvalidRequest, "invalid request", nullptr),
              kInvalidRequest, ""};
    }
    rapidjson::StringBuffer buffer;
    Writer writer(buffer);
    writer.StartArray();
    for (const auto& element : doc.GetArray()) {
      if (!element.IsObject()) {
        // 无效元素 → 错误条目（id null，§2.0）。
        const std::string error =
            BuildErrorImpl(kInvalidRequest, "invalid request", nullptr);
        writer.RawValue(error.c_str(), error.size(), rapidjson::kObjectType);
        continue;
      }
      const auto result = DispatchOne(element, handlers);
      if (result.has_value()) {
        writer.RawValue(result->json.c_str(), result->json.size(),
                        rapidjson::kObjectType);
      }
    }
    writer.EndArray();
    if (buffer.GetSize() == 2) {  // "[]"——全通知 batch（§2.2 无响应）。
      return {true, "", 0, ""};
    }
    return {true, buffer.GetString(), 0, ""};
  }

  if (!doc.IsObject()) {
    // 结构合法但非请求（字符串/数字）——无效请求而非解析错误。
    return {false, BuildErrorImpl(kInvalidRequest, "invalid request", nullptr),
            kInvalidRequest, ""};
  }

  const auto result = DispatchOne(doc, handlers);
  if (!result.has_value()) {
    return {true, "", 0, ""};  // 单通知：无响应。
  }
  return *result;
}

// 公开信封（头声明）：传输层与分派层共用同一 error 序列化（审查修复）。
std::string BuildError(int code, const std::string& message,
                       const rapidjson::Value* id) {
  return BuildErrorImpl(code, message, id);
}

}  // namespace px
