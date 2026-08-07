// tests/cpp/ — 测试套件入口：接管 Catch2 main（默认 Catch2WithMain 不提供
// 钩子），打开 stdout/stderr 即时冲刷（unitbuf）。CI 管道下 std::cout 默认
// 全缓冲——若某测试挂死（如 Windows 上 uv_run 死循环）进程被 ctest 超时
// 杀死，缓冲中的进度与失败信息全部丢失，无从定位卡点；unitbuf 使每一
// 行测试进度实时可见。
#include <catch2/catch_session.hpp>
#include <iostream>

int main(int argc, char* argv[]) {
  std::cout.setf(std::ios::unitbuf);
  std::cerr.setf(std::ios::unitbuf);
  return Catch::Session().run(argc, argv);
}
