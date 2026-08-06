// SPDX-License-Identifier: MIT
#pragma once
// px::Result → bf::Result 单向转换适配层。

#include "core/result.h"     // bf::Result, bf::Error, bf::ErrorCode
#include "px/core/result.h"  // px::Result, px::Error, px::ErrorCode

namespace px {

// 编译期哨兵：bf ErrorCode 末尾值 kFormatMismatch=7。若 bf 新增枚举值
// 导致此断言失败，请同步更新 FromBfErrorCode() 映射表。
static_assert(static_cast<int>(bf::ErrorCode::kFormatMismatch) == 7,
              "bf ErrorCode enum changed — update FromBfErrorCode");

inline ErrorCode FromBfErrorCode(bf::ErrorCode c) {
  switch (c) {
    case bf::ErrorCode::kUnknown:
      return ErrorCode::kInternalError;
    case bf::ErrorCode::kInvalidArgument:
      return ErrorCode::kInvalidArgument;
    case bf::ErrorCode::kDataMissing:
      return ErrorCode::kDataMissing;
    case bf::ErrorCode::kParseError:
      return ErrorCode::kParseError;
    case bf::ErrorCode::kAirportNotFound:
      return ErrorCode::kNotFound;
    case bf::ErrorCode::kNoRoute:
      return ErrorCode::kNoRouteFound;
    case bf::ErrorCode::kCacheCorrupt:
      return ErrorCode::kCacheCorrupt;
    case bf::ErrorCode::kFormatMismatch:
      return ErrorCode::kFormatMismatch;
  }
  return ErrorCode::kInternalError;
}

inline Error FromBfError(const bf::Error& e) {
  return Error{FromBfErrorCode(e.code), e.message};
}
inline Error FromBfError(bf::Error&& e) {
  return Error{FromBfErrorCode(e.code), std::move(e.message)};
}

template <typename T>
Result<T> FromBf(bf::Result<T>&& r) {
  if (r.has_value()) return Ok(std::move(r).value());
  return Err<T>(FromBfError(std::move(r).error()));
}

inline Result<void> FromBf(bf::Result<void>&& r) {
  if (r.has_value()) return Ok();
  return Err<void>(FromBfError(std::move(r).error()));
}

}  // namespace px
