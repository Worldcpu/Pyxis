// SPDX-License-Identifier: MIT
#pragma once
// 模块化 JSON 组合框架。

#include <rapidjson/stringbuffer.h>
#include <rapidjson/writer.h>

#include <memory>
#include <string>
#include <vector>

namespace px {

struct FlightPlan;  // Phase 8 定义

// 组合上下文：px 域数据（无 bf 类型——分层纪律：跨命名空间边界一律经
// FromBf() 转换）。
struct JsonContext {
  const FlightPlan* flightplan = nullptr;
};

class JsonModule {
 public:
  virtual ~JsonModule() = default;
  virtual const char* Name() const = 0;
  // 返回 false = ctx 不完整，Registry 跳过此模块。
  virtual bool WriteFields(rapidjson::Writer<rapidjson::StringBuffer>& writer,
                           const JsonContext& ctx) const = 0;
};

class JsonModuleRegistry {
 public:
  // 注册模块。同名模块后者覆盖前者。
  void Add(std::unique_ptr<JsonModule> module);

  // 依 names 顺序调用各模块 WriteFields 输出字段片段——name 键由模块
  // 自行写出（模块自写 `writer.Key(Name())` 后跟值对象）；ctx 不完整
  // 则跳过该模块；names 中未注册的名称忽略，重复名称只输出一次。
  void Compose(rapidjson::Writer<rapidjson::StringBuffer>& writer,
               const std::vector<std::string>& names,
               const JsonContext& ctx) const;

 private:
  std::vector<std::unique_ptr<JsonModule>> modules_;
};

}  // namespace px
