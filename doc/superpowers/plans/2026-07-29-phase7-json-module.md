# Phase 7: 模块化 JSON 框架 + Route 序列化 — 实施计划

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 建立模块化 JSON 组合框架，实现 RouteJsonModule 包装 bf engine 的 Route 序列化，预留 FlightPlanJsonModule 骨架。

**Architecture:** `JsonModule` 接口 + `JsonModuleRegistry` 组合器。每个模块独立贡献 JSON 片段，service 层按请求组合。Phase 8 添加 FlightPlan 时接口零改动（前向声明 `px::FlightPlan`）。

**Tech Stack:** C++20, RapidJSON Writer<StringBuffer> (SAX), bf engine route_json.h

## Global Constraints

- 接口文件 `snake_case`、`.h`/`.cc`、`#pragma once`
- 命名空间 `px`，bf engine 类型在 `bf::` 下
- JSON 输出使用 `rapidjson::Writer<rapidjson::StringBuffer>`（SAX）
- KISS：直接 include RapidJSON 头文件，不前置声明
- 修改 `.h/.cc` 后运行 clang-format
- 提交使用 English Conventional Commits
- 构建零错误，7 个已有测试 + 新增测试全部通过

---

### Task 1: 创建 json_module.h + json_module.cc — 接口 + Registry 实现

**Files:**
- Create: `include/px/service/json_module.h`
- Create: `service/json_module.cc`

- [ ] **Step 1: json_module.h**

```cpp
// SPDX-License-Identifier: MIT
#pragma once
// 模块化 JSON 组合框架。

#include <memory>
#include <string>
#include <vector>

#include <rapidjson/stringbuffer.h>
#include <rapidjson/writer.h>

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
```

- [ ] **Step 2: json_module.cc**

```cpp
// SPDX-License-Identifier: MIT
#include "px/service/json_module.h"

#include <algorithm>

namespace px {

void JsonModuleRegistry::Add(std::unique_ptr<JsonModule> module) {
  // 同名覆盖：找到已有同名模块直接替换。
  auto it =
      std::find_if(modules_.begin(), modules_.end(), [&](const auto& m) {
        return std::strcmp(m->Name(), module->Name()) == 0;
      });
  if (it != modules_.end()) {
    *it = std::move(module);
    return;
  }
  modules_.push_back(std::move(module));
}

void JsonModuleRegistry::Compose(
    rapidjson::Writer<rapidjson::StringBuffer>& writer,
    const std::vector<std::string>& names, const JsonContext& ctx) const {
  for (const auto& name : names) {
    // 按 names 顺序查找同名模块。
    auto it =
        std::find_if(modules_.begin(), modules_.end(), [&](const auto& m) {
          return std::strcmp(m->Name(), name.c_str()) == 0;
        });
    if (it == modules_.end()) continue;

    if (!(*it)->WriteFields(writer, ctx)) continue;
  }
}

}  // namespace px
```

- [ ] **Step 3: clang-format + 提交**

```bash
clang-format -i include/px/service/json_module.h service/json_module.cc
git add include/px/service/json_module.h service/json_module.cc
git commit -m "feat(service): add JsonModule interface, Registry with impl"
```

---

### Task 2: RouteJsonModule

**Files:**
- Create: `service/route_json_module.h`
- Create: `service/route_json_module.cc`

- [ ] **Step 1: route_json_module.h**

```cpp
// SPDX-License-Identifier: MIT
#pragma once
#include "px/service/json_module.h"

namespace px {

class RouteJsonModule : public JsonModule {
 public:
  const char* Name() const override { return "route"; }
  bool WriteFields(rapidjson::Writer<rapidjson::StringBuffer>& writer,
                   const JsonContext& ctx) const override;
};

}  // namespace px
```

- [ ] **Step 2: route_json_module.cc**

```cpp
// SPDX-License-Identifier: MIT
#include "route_json_module.h"
#include "core/routing/route_json.h"  // bf::WriteRouteJson

namespace px {

bool RouteJsonModule::WriteFields(
    rapidjson::Writer<rapidjson::StringBuffer>& writer,
    const JsonContext& ctx) const {
  if (!ctx.route) return false;
  writer.Key("route");
  bf::WriteRouteJson(writer, *ctx.route);
  return true;
}

}  // namespace px
```

- [ ] **Step 3: 提交**

```bash
clang-format -i service/route_json_module.h service/route_json_module.cc
git add service/route_json_module.h service/route_json_module.cc
git commit -m "feat(service): add RouteJsonModule wrapping bf::WriteRouteJson"
```

---

### Task 3: FlightPlanJsonModule 骨架

**Files:**
- Create: `service/flightplan_json_module.h`
- Create: `service/flightplan_json_module.cc`

- [ ] **Step 1: flightplan_json_module.h**

```cpp
// SPDX-License-Identifier: MIT
#pragma once
#include "px/service/json_module.h"

namespace px {

class FlightPlanJsonModule : public JsonModule {
 public:
  const char* Name() const override { return "flightplan"; }
  bool WriteFields(rapidjson::Writer<rapidjson::StringBuffer>& writer,
                   const JsonContext& ctx) const override;
};

}  // namespace px
```

- [ ] **Step 2: flightplan_json_module.cc**

```cpp
// SPDX-License-Identifier: MIT
#include "flightplan_json_module.h"

namespace px {

bool FlightPlanJsonModule::WriteFields(
    rapidjson::Writer<rapidjson::StringBuffer>& writer,
    const JsonContext& ctx) const {
  if (!ctx.flightplan) return false;
  // Phase 8: 序列化 flightplan 字段
  return true;
}

}  // namespace px
```

- [ ] **Step 3: 提交**

```bash
clang-format -i service/flightplan_json_module.h service/flightplan_json_module.cc
git add service/flightplan_json_module.h service/flightplan_json_module.cc
git commit -m "feat(service): add FlightPlanJsonModule skeleton"
```

---

### Task 4: 更新 service/CMakeLists.txt

**Files:**
- Modify: `service/CMakeLists.txt`

- [ ] **Step 1: 添加新源文件**

```cmake
add_library(px_service STATIC
  service.cc
  json_module.cc
  route_json_module.cc
  flightplan_json_module.cc
)
```

- [ ] **Step 2: 验证构建**

```bash
cmake --build build/debug -j $(nproc) 2>&1 | tail -5
```

- [ ] **Step 3: 提交**

```bash
git add service/CMakeLists.txt
git commit -m "build(service): add json module sources"
```

---

### Task 5: 测试

**Files:**
- Create: `tests/cpp/test_json_module.cc`
- Modify: `tests/cpp/CMakeLists.txt`

- [ ] **Step 1: test_json_module.cc**

```cpp
#include <catch2/catch_test_macros.hpp>

#include <rapidjson/stringbuffer.h>
#include <rapidjson/writer.h>

#include "px/service/json_module.h"

namespace px {
namespace {

class MockModule : public JsonModule {
 public:
  MockModule(const char* name, const char* value)
      : name_(name), value_(value) {}

  const char* Name() const override { return name_; }

  bool WriteFields(rapidjson::Writer<rapidjson::StringBuffer>& writer,
                   const JsonContext& ctx) const override {
    (void)ctx;
    writer.Key(name_);
    writer.StartObject();
    writer.Key("msg");
    writer.String(value_);
    writer.EndObject();
    return true;
  }

 private:
  const char* name_;
  const char* value_;
};

class NullSkipModule : public JsonModule {
 public:
  const char* Name() const override { return "skip_me"; }

  bool WriteFields(rapidjson::Writer<rapidjson::StringBuffer>& writer,
                   const JsonContext& ctx) const override {
    (void)writer;
    return ctx.route != nullptr;  // ctx.route == nullptr → 跳过
  }
};

}  // namespace
}  // namespace px

using namespace px;

TEST_CASE("Registry Compose 空模块输出空对象") {
  JsonModuleRegistry registry;
  rapidjson::StringBuffer buf;
  rapidjson::Writer<rapidjson::StringBuffer> writer(buf);

  writer.StartObject();
  registry.Compose(writer, {}, JsonContext{});
  writer.EndObject();

  REQUIRE(std::string(buf.GetString()) == "{}");
}

TEST_CASE("Registry Compose 按 names 顺序输出") {
  JsonModuleRegistry registry;
  registry.Add(std::make_unique<MockModule>("a", "first"));
  registry.Add(std::make_unique<MockModule>("b", "second"));

  rapidjson::StringBuffer buf;
  rapidjson::Writer<rapidjson::StringBuffer> writer(buf);

  writer.StartObject();
  registry.Compose(writer, {"a", "b"}, JsonContext{});
  writer.EndObject();

  std::string json(buf.GetString());
  REQUIRE(json.find("\"a\":") < json.find("\"b\":"));
}

TEST_CASE("Registry 跳过未注册的模块名") {
  JsonModuleRegistry registry;
  registry.Add(std::make_unique<MockModule>("known", "val"));

  rapidjson::StringBuffer buf;
  rapidjson::Writer<rapidjson::StringBuffer> writer(buf);

  writer.StartObject();
  registry.Compose(writer, {"unknown", "known"}, JsonContext{});
  writer.EndObject();

  std::string json(buf.GetString());
  REQUIRE(json.find("unknown") == std::string::npos);
  REQUIRE(json.find("known") != std::string::npos);
}

TEST_CASE("WriteFields 返回 false 时 Registry 跳过") {
  JsonModuleRegistry registry;
  registry.Add(std::make_unique<NullSkipModule>());
  registry.Add(std::make_unique<MockModule>("ok", "present"));

  rapidjson::StringBuffer buf;
  rapidjson::Writer<rapidjson::StringBuffer> writer(buf);

  writer.StartObject();
  registry.Compose(writer, {"skip_me", "ok"}, JsonContext{});
  writer.EndObject();

  std::string json(buf.GetString());
  REQUIRE(json.find("skip_me") == std::string::npos);
  REQUIRE(json.find("ok") != std::string::npos);
}

TEST_CASE("Registry 同名模块后者覆盖") {
  JsonModuleRegistry registry;
  registry.Add(std::make_unique<MockModule>("x", "first"));
  registry.Add(std::make_unique<MockModule>("x", "second"));

  rapidjson::StringBuffer buf;
  rapidjson::Writer<rapidjson::StringBuffer> writer(buf);

  writer.StartObject();
  registry.Compose(writer, {"x"}, JsonContext{});
  writer.EndObject();

  std::string json(buf.GetString());
  REQUIRE(json.find("second") != std::string::npos);
}
```

- [ ] **Step 2: 更新 tests/cpp/CMakeLists.txt**——`px_tests` 源文件中加 `test_json_module.cc`

- [ ] **Step 3: 构建 + 测试**

```bash
cmake --build build/debug -j $(nproc)
ctest --test-dir build/debug --output-on-failure
```

预期：12 个测试全部通过（7 已有 + 5 新增）。

- [ ] **Step 4: 提交**

```bash
git add tests/cpp/test_json_module.cc tests/cpp/CMakeLists.txt
git commit -m "test(service): add JsonModuleRegistry composition tests"
```
