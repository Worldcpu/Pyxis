// SPDX-License-Identifier: MIT
#pragma once

#include <cstdint>
#include <optional>
#include <string>

namespace px {

// 缓存决策（决策 19：先服务后建缓存——有 .bfdb 且 provenance 匹配 →
// OpenCached；缺失/失配 → Open(源数据) + 后台 WriteUnified）。
// TODO(T7 缓存架构)：bf 统一缓存头还含 format_version/program_version，
// OpenCached 时会自行校验（kFormatMismatch）——T7 接线时以 bf OpenCached
// 为真源做格式兜底，本决策层只作 pre-open 快速通道（见
// doc/flightplan/glossary.md 决策 19）。
enum class CacheDecision { kUseCache, kBuildFromSource };

// 缓存 provenance（bf 统一缓存头字段：cycle/loader/data_dir——镜像
// bf::UnifiedHeader 子集；完整映射由 T7 的 FromBf 桥接落地）。
struct CacheProvenance {
  uint32_t cycle = 0;
  std::string loader;  // bf 侧对应 source_loader
  std::string data_dir;
};

// provenance 匹配决策：cached 为 nullopt（无缓存）或任一字段与 expected
// 失配 → kBuildFromSource；完全匹配 → kUseCache。
CacheDecision DecideCache(const std::optional<CacheProvenance>& cached,
                          const CacheProvenance& expected);

}  // namespace px
