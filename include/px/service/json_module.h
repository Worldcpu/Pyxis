// SPDX-License-Identifier: MIT
#pragma once
// 模块化 JSON 组合框架。

#include <rapidjson/stringbuffer.h>
#include <rapidjson/writer.h>

#include <memory>
#include <string>
#include <vector>

#include "core/routing/route.h"  // bf::Route

namespace px {

struct FlightPlan;  // Phase 8 定义

struct JsonContext {
  const bf::Route* route = nullptr;
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

  // 按 names 顺序输出 {"name":{...}}。ctx 不完整则跳过该模块。
  // names 中未注册的名称被忽略。
  void Compose(rapidjson::Writer<rapidjson::StringBuffer>& writer,
               const std::vector<std::string>& names,
               const JsonContext& ctx) const;

 private:
  std::vector<std::unique_ptr<JsonModule>> modules_;
};

}  // namespace px
