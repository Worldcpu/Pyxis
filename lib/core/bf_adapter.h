// SPDX-License-Identifier: MIT
#pragma once
// px::Result → bf::Result 单向转换适配层。

#include "core/result.h"    // bf::Result, bf::Error, bf::ErrorCode
#include "px/core/result.h"  // px::Result, px::Error, px::ErrorCode

namespace px {

// 编译期哨兵：bf ErrorCode 末尾值 kFormatMismatch=7。若 bf 新增枚举值
// 导致此断言失败，请同步更新 ToBfErrorCode() 映射表。
static_assert(static_cast<int>(bf::ErrorCode::kFormatMismatch) == 7,
              "bf ErrorCode enum changed — update ToBfErrorCode");

inline bf::ErrorCode ToBfErrorCode(ErrorCode c) {
  switch (c) {
    case ErrorCode::kInvalidArgument:  return bf::ErrorCode::kInvalidArgument;
    case ErrorCode::kDataMissing:      return bf::ErrorCode::kDataMissing;
    case ErrorCode::kParseError:       return bf::ErrorCode::kParseError;
    case ErrorCode::kNotFound:         return bf::ErrorCode::kAirportNotFound;
    case ErrorCode::kNoRouteFound:     return bf::ErrorCode::kNoRoute;
    case ErrorCode::kCacheCorrupt:     return bf::ErrorCode::kCacheCorrupt;
    case ErrorCode::kFormatMismatch:   return bf::ErrorCode::kFormatMismatch;
  }
  return bf::ErrorCode::kUnknown;
}

inline bf::Error ToBfError(const Error& e) {
  return bf::Error{ToBfErrorCode(e.code), e.message};
}
inline bf::Error ToBfError(Error&& e) {
  return bf::Error{ToBfErrorCode(e.code), std::move(e.message)};
}

template <typename T>
bf::Result<T> ToBf(Result<T>&& r) {
  if (r.has_value()) return bf::Result<T>::Ok(std::move(r).value());
  return bf::Result<T>::Err(ToBfError(std::move(r).error()));
}

inline bf::Result<void> ToBf(Result<void>&& r) {
  if (r.has_value()) return bf::Result<void>::Ok();
  return bf::Result<void>::Err(ToBfError(std::move(r).error()));
}

}  // namespace px
