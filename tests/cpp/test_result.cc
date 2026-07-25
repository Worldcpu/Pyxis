#include <catch2/catch_test_macros.hpp>

#include "px/core/error.h"
#include "px/core/result.h"

namespace px {
namespace {

TEST_CASE("Result Ok holds a value") {
  auto r = Ok<int>(42);
  REQUIRE(r.has_value());
  REQUIRE(r.value() == 42);
}

TEST_CASE("Result Err holds an error") {
  auto r = Err<int>({ErrorCode::kNotFound, "not found"});
  REQUIRE(!r.has_value());
  REQUIRE(r.error().code == ErrorCode::kNotFound);
  REQUIRE(r.error().message == "not found");
}

TEST_CASE("Result value_or returns fallback on error") {
  auto e = Err<int>({ErrorCode::kNoRouteFound, ""});
  REQUIRE(e.value_or(99) == 99);

  auto ok = Ok<int>(7);
  REQUIRE(ok.value_or(99) == 7);
}

TEST_CASE("Result<void> Ok returns no value") {
  auto r = Ok();
  REQUIRE(r.has_value());
}

TEST_CASE("Result<void> Err holds error") {
  auto r = Err<void>({ErrorCode::kInvalidInput, "bad"});
  REQUIRE(!r.has_value());
  REQUIRE(r.error().message == "bad");
}

TEST_CASE("Result<bool> operator bool") {
  auto ok = Ok<int>(1);
  auto err = Err<int>({ErrorCode::kInternalError, ""});
  REQUIRE(ok);
  REQUIRE(!err);
}

TEST_CASE("Result move semantics") {
  auto r = Ok<std::string>("hello");
  std::string v = std::move(r).value();
  REQUIRE(v == "hello");
}

}  // namespace
}  // namespace px
