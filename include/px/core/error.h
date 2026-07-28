#pragma once

#include <string>

namespace px {

// 错误类别枚举。调用方通过错误码分支处理，无需解析人类可读消息。
enum class ErrorCode {
  kNotFound,        // 航点/机场/航路在数据库中不存在
  kInvalidInput,    // 用户输入格式错误
  kInvalidArgument, // 传递给函数/加载器的参数无效
  kDataMissing,     // 数据文件或目录不存在
  kParseError,      // 数据解析/反序列化失败
  kCacheCorrupt,    // bfdb 文件损坏
  kFormatMismatch,  // bfdb 格式版本不匹配
  kNoRouteFound,    // 无路径满足约束条件
  kCancelled,       // 计算已被取消
  kNotImplemented,  // 功能尚未实现
  kInternalError,   // 未预期错误
};

// Result 失败路径上携带的轻量错误值。
struct Error {
  ErrorCode code = ErrorCode::kInternalError;
  std::string message;

  Error() = default;
  Error(ErrorCode c, std::string msg) : code(c), message(std::move(msg)) {}
};

}  // namespace px
