#pragma once

#include <atomic>

namespace px {

// 协作式取消令牌。worker 线程定期检查，主线程在取消事件时设置。
struct CancelToken {
  std::atomic<bool> cancelled{false};

  void Cancel() { cancelled.store(true, std::memory_order_release); }
  bool IsCancelled() const { return cancelled.load(std::memory_order_acquire); }
};

}  // namespace px
