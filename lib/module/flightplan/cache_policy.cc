// SPDX-License-Identifier: MIT
// 缓存决策（决策 19：provenance 匹配——cycle/loader/data_dir 全匹配才用
// 缓存；任一失配或缺失 → 源数据构建 + 后台落盘）。
#include "px/module/flightplan/cache_policy.h"

namespace px {

CacheDecision DecideCache(const std::optional<CacheProvenance>& cached,
                          const CacheProvenance& expected) {
  if (!cached.has_value()) return CacheDecision::kBuildFromSource;
  if (cached->cycle != expected.cycle) return CacheDecision::kBuildFromSource;
  if (cached->loader != expected.loader) {
    return CacheDecision::kBuildFromSource;
  }
  if (cached->data_dir != expected.data_dir) {
    return CacheDecision::kBuildFromSource;
  }
  return CacheDecision::kUseCache;
}

}  // namespace px
