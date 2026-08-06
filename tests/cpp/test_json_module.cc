#include <rapidjson/stringbuffer.h>
#include <rapidjson/writer.h>

#include <catch2/catch_test_macros.hpp>

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
    return ctx.flightplan != nullptr;  // ctx.flightplan == nullptr → skip
  }
};

}  // namespace
}  // namespace px

using namespace px;

TEST_CASE("Registry Compose empty modules outputs empty object") {
  JsonModuleRegistry registry;
  rapidjson::StringBuffer buf;
  rapidjson::Writer<rapidjson::StringBuffer> writer(buf);

  writer.StartObject();
  registry.Compose(writer, {}, JsonContext{});
  writer.EndObject();

  REQUIRE(std::string(buf.GetString()) == "{}");
}

TEST_CASE("Registry Compose outputs in names order") {
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

TEST_CASE("Registry skips unregistered module names") {
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

TEST_CASE("Registry skips when WriteFields returns false") {
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

TEST_CASE("Registry duplicate names in request output once") {
  // names 重复时同一模块只输出一次（重复 key 会被 JSON.parse 静默吞掉
  // 第一个——显式去重；含不相邻重复 "a","b","a"）。
  JsonModuleRegistry registry;
  registry.Add(std::make_unique<MockModule>("a", "first"));
  registry.Add(std::make_unique<MockModule>("b", "second"));

  rapidjson::StringBuffer buf;
  rapidjson::Writer<rapidjson::StringBuffer> writer(buf);

  writer.StartObject();
  registry.Compose(writer, {"a", "b", "a"}, JsonContext{});
  writer.EndObject();

  const std::string json(buf.GetString());
  REQUIRE(json.find("\"a\":") != std::string::npos);
  REQUIRE(json.find("\"b\":") != std::string::npos);
  // a 只出现一次（当前实现只防相邻重复——"a","b","a" 会双写）。
  const size_t first = json.find("\"a\":");
  CHECK(json.find("\"a\":", first + 1) == std::string::npos);
}

TEST_CASE("Registry same-name modules: last wins") {
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
