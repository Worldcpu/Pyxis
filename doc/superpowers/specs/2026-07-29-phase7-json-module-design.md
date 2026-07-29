# Phase 7: 模块化 JSON 组合框架 + Route 序列化

## 背景

Pyxis engine 集成完毕（Phase 5-6）。需要将 bf engine 的 Route 搜索结果序列化为 JSON 供前端消费。Phase 7 建立模块化 JSON 框架——每个 lib 模块独立贡献 JSON 片段，service 层组合输出。为 Phase 8 FlightPlan 预留注册点。

## 架构

```
Handler (service)
  ├─ bf::NavDatabase::FindRoutes() → Route
  ├─ Phase 8: FlightPlanBuilder::Build(route) → FlightPlan
  │
  ├─ JsonContext{ .route=&route, .flightplan=nullptr }
  └─ Registry.Compose({"route", "flightplan"}, ctx)
       ├─ RouteJsonModule      → {"route": {...}}
       └─ FlightPlanJsonModule → skip (ctx.flightplan == null)
```

## 接口

### JsonContext —— 前向声明，Phase 8 零改动

```cpp
namespace px { struct FlightPlan; }  // Phase 8 定义

struct JsonContext {
  const bf::Route* route = nullptr;
  const FlightPlan* flightplan = nullptr;
};
```

### JsonModule

```cpp
class JsonModule {
 public:
  virtual ~JsonModule() = default;
  virtual const char* Name() const = 0;
  virtual bool WriteFields(Writer& w, const JsonContext& ctx) const = 0;
};
```

### JsonModuleRegistry

```cpp
class JsonModuleRegistry {
 public:
  void Add(std::unique_ptr<JsonModule> module);
  void Compose(Writer& w, const std::vector<std::string>& names,
               const JsonContext& ctx);
};
```

## RouteJsonModule

轻量包装 bf::WriteRouteJson（bf engine 已有，SAX 流式输出）——ctx.route 为 null 时返回 false，Registry 跳过。

输出字段：route_string、total_distance_nm、各阶段距离、sid/star、points(ident/lat/lon)、legs(from/to/via/distance) 等。

## FlightPlanJsonModule

Phase 7 提供骨架（WriteFields 始终返回 false，ctx.flightplan 为 null），Phase 8 填入实现。接口零改动。

## 文件清单

| 文件 | 内容 |
|------|------|
| `include/px/service/json_module.h` | JsonModule + Registry + JsonContext |
| `service/route_json_module.h` | RouteJsonModule 声明 |
| `service/route_json_module.cc` | RouteJsonModule 实现 |
| `service/flightplan_json_module.h` | FlightPlanJsonModule 骨架声明 |
| `service/flightplan_json_module.cc` | FlightPlanJsonModule 骨架实现 |
| `service/CMakeLists.txt` | 添加新源文件 |

## Phase 8 迁移

1. `include/px/module/flightplan/` 定义 FlightPlan
2. `flightplan_json_module.cc` 填入序列化实现
3. Handler 赋 `ctx.flightplan = &fp`
4. JsonContext、JsonModule、Registry 接口零改动

## 测试

- Registry 按 names 顺序组合 mock 模块
- ctx 为 null 时模块返回 false，Registry 跳过
- RouteJsonModule 输出与 bf WriteRouteJson 字段对照
