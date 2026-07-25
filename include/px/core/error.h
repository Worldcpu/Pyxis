#pragma once

#include <string>

namespace px {

// Category of an error, used by callers to branch on failure kinds without
// parsing the human-readable message.
enum class ErrorCode {
  kOk = 0,
  kNotFound,          // airport/waypoint/airway not in database
  kInvalidInput,      // malformed user input
  kCacheCorrupt,      // bfdb file: bad magic, truncated, unresolvable reference
  kFormatMismatch,    // bfdb format version does not match this build
  kNoRouteFound,      // no path satisfies all constraints
  kCancelled,         // computation cancelled (user disconnected or re-requested)
  kNotImplemented,    // feature not yet implemented
  kInternalError,     // unexpected exception caught at worker boundary
};

// A lightweight error value carried by Result on the failure path.
struct Error {
  ErrorCode code = ErrorCode::kInternalError;
  std::string message;

  Error() = default;
  Error(ErrorCode c, std::string msg) : code(c), message(std::move(msg)) {}
};

}  // namespace px
