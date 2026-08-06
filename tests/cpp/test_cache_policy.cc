// SPDX-License-Identifier: MIT
// 缓存决策测试（S8——决策 19：先服务后建缓存；provenance 匹配）。

#include <catch2/catch_test_macros.hpp>
#include <optional>
#include <string>

#include "px/module/flightplan/cache_policy.h"

namespace {

px::CacheProvenance MakeExpected() { return {2601, "fenix", "/data/navdata"}; }

}  // namespace

TEST_CASE("cache: 有缓存且 provenance 匹配 → 用缓存", "[cache][unit]") {
  const auto decision = px::DecideCache(
      px::CacheProvenance{2601, "fenix", "/data/navdata"}, MakeExpected());
  CHECK(decision == px::CacheDecision::kUseCache);
}

TEST_CASE("cache: 无缓存 → 源数据构建", "[cache][unit]") {
  const auto decision = px::DecideCache(std::nullopt, MakeExpected());
  CHECK(decision == px::CacheDecision::kBuildFromSource);
}

TEST_CASE("cache: cycle/loader/data_dir 任一失配 → 重建", "[cache][unit]") {
  CHECK(px::DecideCache(px::CacheProvenance{2602, "fenix", "/data/navdata"},
                        MakeExpected()) == px::CacheDecision::kBuildFromSource);
  CHECK(px::DecideCache(px::CacheProvenance{2601, "dfd1", "/data/navdata"},
                        MakeExpected()) == px::CacheDecision::kBuildFromSource);
  CHECK(px::DecideCache(px::CacheProvenance{2601, "fenix", "/other/dir"},
                        MakeExpected()) == px::CacheDecision::kBuildFromSource);
}
