#pragma once

// 薄封装：使用 tl::expected（C++11 的 std::expected 回退实现）作为
// Result 类型。项目迁移到 C++23 后，将本 include 和别名替换为
// <expected> 和 std::expected 即可。
#include <tl/expected.hpp>

#include "px/core/error.h"

namespace px {

template <class T>
using Result = tl::expected<T, Error>;

// --- 工厂函数 ---

template <class T>
Result<T> Ok(T&& value) {
  return Result<T>(std::forward<T>(value));
}

inline Result<void> Ok() { return {}; }

template <class T>
Result<T> Err(Error error) {
  return tl::make_unexpected(std::move(error));
}

}  // namespace px
