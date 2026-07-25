#pragma once

#include <functional>
#include <string>
#include <utility>

namespace px {

// 导航点标识符。ident 字符串（如 "JFK"、"DEEZZ"）并非全局唯一：
// 同一代码在不同 ICAO 地区中重复使用。因此 (ident, region) 二元组
// 才是唯一标识一个导航点的键，建模为单一值类型贯穿整个查询系统。
struct Ident {
  std::string ident{};
  std::string region{};  // 两字母 ICAO 地区码，如 "K6"

  Ident() = default;
  Ident(std::string id, std::string reg)
      : ident(std::move(id)), region(std::move(reg)) {}

  bool operator==(const Ident& other) const {
    return ident == other.ident && region == other.region;
  }
};

}  // namespace px

namespace std {

template <>
struct hash<px::Ident> {
  size_t operator()(const px::Ident& key) const noexcept {
    size_t h1 = std::hash<std::string>{}(key.ident);
    size_t h2 = std::hash<std::string>{}(key.region);
    // 合并两个哈希值（boost 风格混合）。
    return h1 ^ (h2 + 0x9e3779b97f4a7c15ULL + (h1 << 6) + (h1 >> 2));
  }
};

}  // namespace std
