#pragma once

#include <string>

namespace px {

// 错误类别枚举。调用方通过错误码分支处理，无需解析人类可读消息。
enum class ErrorCode {
  kOk = 0,
  kNotFound,          // 航点/机场/航路在数据库中不存在
  kInvalidInput,      // 用户输入格式错误
  kCacheCorrupt,      // bfdb 文件损坏：魔数错误、截断、无法解析的引用
  kFormatMismatch,    // bfdb 格式版本与当前构建不匹配
  kNoRouteFound,      // 无路径满足所有约束条件
  kCancelled,         // 计算已被取消（用户断开连接或重新发起请求）
  kNotImplemented,    // 功能尚未实现
  kInternalError,     // worker 线程边界捕获的未预期异常
};

// Result 失败路径上携带的轻量错误值。
struct Error {
  ErrorCode code = ErrorCode::kInternalError;
  std::string message;

  Error() = default;
  Error(ErrorCode c, std::string msg) : code(c), message(std::move(msg)) {}
};

}  // namespace px
