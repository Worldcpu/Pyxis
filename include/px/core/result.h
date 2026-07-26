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

// 显式类型版本的 Err：auto e = Err<int>(Error(...)) 返回 Result<int>。
// 用于测试等需要明确指定结果类型的场景。
template <class T>
Result<T> Err(Error error) {
  return tl::make_unexpected(std::move(error));
}

// 隐式推导版本的 Err：在 return Err(Error(...)) 语境中，
// 返回 tl::unexpected<Error> 并转换为外层函数的 Result<T>。
inline tl::unexpected<Error> Err(Error error) {
  return tl::make_unexpected(std::move(error));
}

}  // namespace px
