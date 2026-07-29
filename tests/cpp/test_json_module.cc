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
    return ctx.route != nullptr;  // ctx.route == nullptr → skip
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
