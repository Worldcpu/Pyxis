#pragma once

// Thin wrapper: use tl::expected (C++11 backport of std::expected) as our
// Result type.  When the project migrates to C++23, replace this include
// and alias with <expected> and std::expected.
#include <tl/expected.hpp>

#include "px/core/error.h"

namespace px {

template <class T>
using Result = tl::expected<T, Error>;

// --- Factory functions ---

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
