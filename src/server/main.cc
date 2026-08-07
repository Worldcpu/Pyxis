// SPDX-License-Identifier: MIT
// px_server — HTTP/1.1 + JSON-RPC over POST 服务入口（决策 43/44/46/47/49：
// 复用 bf_http_server 传输、QueueWork 线程池 offload、默认端口 19100、
// navdata 缺失服务照起、同步 WriteUnified 建 bfdb）。
#include <uv.h>

#include <atomic>
#include <cstdio>
#include <cstdlib>
#include <memory>
#include <optional>
#include <string>
#include <unordered_map>

#include "conn.h"
#include "io/cache/bfdb_naming.h"
#include "io/cache/unified_cache.h"
#include "io/nav_database.h"
#include "px/service/http_rpc.h"
#include "px/service/plan_handlers.h"
#include "server.h"
#include "transport.h"
#include "work.h"

namespace {

using bf::http_server::Connection;
using bf::http_server::HttpRequest;
using bf::http_server::WorkResult;

// 启动参数（决策 46：--port 默认 19100，127.0.0.1 回环）。
struct Args {
  int port = 19100;
  std::string data_dir;     // airframe 档案目录
  std::string navdata_dir;  // X-Plane 12 Custom Data（决策 47：可缺失）
  std::string cache_dir;    // bfdb 缓存目录（决策 49）
};

// 简单参数解析：--key value 或 --key=value。port 用 strtol（禁用异常）。
bool ParseArgs(int argc, char** argv, Args* args) {
  for (int i = 1; i < argc; ++i) {
    const std::string arg = argv[i];
    const size_t eq = arg.find('=');
    const std::string key = eq == std::string::npos ? arg : arg.substr(0, eq);
    const std::string value =
        eq == std::string::npos
            ? (i + 1 < argc ? std::string(argv[++i]) : std::string())
            : arg.substr(eq + 1);
    if (key == "--port") {
      char* end = nullptr;
      const long parsed = std::strtol(value.c_str(), &end, 10);
      if (end == value.c_str() || *end != '\0' || parsed <= 0 ||
          parsed > 65535) {
        return false;
      }
      args->port = static_cast<int>(parsed);
    } else if (key == "--data-dir") {
      args->data_dir = value;
    } else if (key == "--navdata-dir") {
      args->navdata_dir = value;
    } else if (key == "--cache-dir") {
      args->cache_dir = value;
    } else {
      return false;
    }
  }
  return true;
}

// CORS（决策 43：WebView 跨源 fetch）。回环服务 + 写端点（airframe.upsert
// 等）——通配 * 会让任意网站可调用本地 API，白名单反射 Origin 收窄面：
// Vite dev（5173）与 Tauri WebView（tauri://localhost）。
constexpr std::string_view kAllowedOrigins[] = {
    "http://localhost:5173", "http://127.0.0.1:5173", "tauri://localhost",
    "http://tauri.localhost"};

// 白名单内返回反射 Origin 头；其余返回空（空 origin = 非浏览器，放行）。
bf::http_server::Headers CorsHeaders(const std::string& origin) {
  bf::http_server::Headers headers;
  if (origin.empty()) return headers;  // curl 等本地工具
  for (const auto allowed : kAllowedOrigins) {
    if (origin == allowed) {
      headers.emplace_back("Access-Control-Allow-Origin", origin);
      return headers;
    }
  }
  return headers;
}

// 服务器端 RequestHandler（决策 44）：传输层快速失败同步响应；
// 有效请求 QueueWork offload libuv 线程池，loop 线程只做传输分派。
class RpcRequestHandler : public bf::http_server::RequestHandler {
 public:
  RpcRequestHandler(uv_loop_t* loop,
                    std::unordered_map<std::string, px::RpcHandler> handlers)
      : loop_(loop),
        handlers_(
            std::make_shared<std::unordered_map<std::string, px::RpcHandler>>(
                std::move(handlers))) {}

  void Handle(std::shared_ptr<Connection> conn,
              const HttpRequest& req) override {
    // Origin 白名单（本地服务安全：任意网站发起的 CSRF 写操作防护）。
    const std::string origin = req.Header("origin");
    const auto cors = CorsHeaders(origin);
    if (!origin.empty() && cors.empty()) {
      conn->WriteResponse(403, "origin not allowed", req.keep_alive,
                          "application/json", cors);
      return;
    }
    // 传输层快速失败（非 POST / 非 /rpc / 空 body——同步，不 offload）。
    if (req.method != "POST" || req.path != "/rpc" || req.body.empty()) {
      const auto resp =
          px::HandleHttpRpc(req.method, req.path, req.body, *handlers_);
      conn->WriteResponse(resp.status, resp.body, req.keep_alive,
                          "application/json", cors);
      return;
    }
    // 有效请求 → 线程池（决策 44；闭包可拷贝——shared_ptr 持有 handler 表）。
    QueueWork(
        conn, loop_,
        [handlers = handlers_, req, cors]() -> WorkResult {
          const auto resp =
              px::HandleHttpRpc(req.method, req.path, req.body, *handlers);
          WorkResult out;
          out.status = resp.status;
          out.body = resp.body;
          out.content_type = "application/json";
          out.extra_headers = cors;
          return out;
        },
        req.keep_alive, inflight_);
  }

 private:
  uv_loop_t* loop_;
  std::shared_ptr<const std::unordered_map<std::string, px::RpcHandler>>
      handlers_;
  std::atomic<int> inflight_{0};
};

int RunServer(const Args& args, const px::PlanContext& ctx) {
  uv_loop_t loop;
  if (uv_loop_init(&loop) != 0) {
    std::fprintf(stderr, "uv_loop_init 失败\n");
    return 1;
  }
  RpcRequestHandler handler(&loop, px::MakePlanHandlers(ctx));
  bf::http_server::Server server(&loop, handler, {});
  if (server.Listen("127.0.0.1", args.port) != 0) {
    std::fprintf(stderr, "无法监听 127.0.0.1:%d\n", args.port);
    return 1;
  }
  std::printf("px_server listening on http://127.0.0.1:%d/rpc\n",
              server.BoundPort());
  uv_run(&loop, UV_RUN_DEFAULT);
  server.Close();  // loop 线程（uv_run 返回后同线程）——bf 契约
  uv_loop_close(&loop);
  return 0;
}

}  // namespace

int main(int argc, char** argv) {
  Args args;
  if (!ParseArgs(argc, argv, &args)) {
    std::fprintf(stderr,
                 "用法: px_server --navdata-dir <dir> [--data-dir <dir>] "
                 "[--cache-dir <dir>] [--port <port>]\n");
    return 2;
  }
  if (args.data_dir.empty()) args.data_dir = ".";
  if (args.cache_dir.empty()) args.cache_dir = args.data_dir;

  // 生命周期：db/airports 在 main 全程存活（ctx 指针指向它们）。
  std::optional<bf::NavDatabase> db;
  std::optional<px::AirportIndex> airports;
  px::PlanContext ctx;
  ctx.data_dir = args.data_dir;

  // 决策 47：navdata 缺失服务照起（db 空 → plan/lookup 查询 -32000）。
  if (!args.navdata_dir.empty()) {
    auto opened = bf::NavDatabase::Open(args.navdata_dir, "xplane12");
    if (opened.has_value()) {
      // 决策 49：同步 WriteUnified 落盘 bfdb（首启秒级）→ px 机场索引。
      const std::string tmp_cache = args.cache_dir + "/nav_new.bfdb";
      auto written = opened.value().WriteUnified(tmp_cache);
      if (written.has_value()) {
        const uint32_t cycle = written.value();
        const std::string final_path =
            args.cache_dir + "/" + bf::FormatBfdbName(cycle);
        std::rename(tmp_cache.c_str(), final_path.c_str());
        auto index = px::AirportIndex::Open(final_path);
        if (index.has_value()) {
          airports.emplace(std::move(index.value()));
          ctx.airports = &*airports;
        } else {
          std::fprintf(stderr, "警告: 机场索引构建失败（%s）\n",
                       index.error().message.c_str());
        }
        auto header = bf::UnifiedCache::ReadHeader(final_path);
        if (header.has_value()) ctx.cycle = header.value().cycle;
      } else {
        std::fprintf(stderr, "警告: bfdb 构建失败（%s）\n",
                     written.error().message.c_str());
      }
      db.emplace(std::move(opened.value()));
      ctx.db = &*db;
    } else {
      std::fprintf(stderr, "警告: navdata 加载失败（%s）——查询将返回 -32000\n",
                   opened.error().message.c_str());
    }
  }
  return RunServer(args, ctx);
}
