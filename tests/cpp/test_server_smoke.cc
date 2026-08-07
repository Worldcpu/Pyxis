// SPDX-License-Identifier: MIT
// HTTP 传输集成冒烟（S4b——决策 50：Catch2 内起 bf_http_server 绑临时
// 端口 + 真实 socket POST /rpc，覆盖 传输→分派→适配 全链路；决策 43
// 传输约定）。注意：libuv 回调是 C ABI——断言一律在 uv_run 后主线程做，
// 回调内只记录状态。

#include <uv.h>

#include <catch2/catch_test_macros.hpp>
#include <cstddef>
#include <string>

#include "conn.h"
#include "px/service/http_rpc.h"
#include "px/service/rpc_dispatch.h"
#include "server.h"
#include "transport.h"

namespace {

std::unordered_map<std::string, px::RpcHandler> MakeHandlers() {
  return {
      {"echo",
       [](const rapidjson::Value&) {
         return px::RpcResult{true, R"({"echoed":true})", 0, ""};
       }},
  };
}

// 服务器端 RequestHandler：同步调 HandleHttpRpc（冒烟用轻 handler；
// QueueWork offload 在 main.cc 组装——决策 44）。
class SmokeHandler : public bf::http_server::RequestHandler {
 public:
  void Handle(std::shared_ptr<bf::http_server::Connection> conn,
              const bf::http_server::HttpRequest& req) override {
    const auto resp =
        px::HandleHttpRpc(req.method, req.path, req.body, handlers_);
    conn->WriteResponse(resp.status, resp.body, req.keep_alive);
  }

 private:
  std::unordered_map<std::string, px::RpcHandler> handlers_ = MakeHandlers();
};

// 客户端状态（回调经指针访问；错误记录在主线程断言）。读缓冲为成员
// （审查修复：原 static 缓冲违反"禁 static 可变状态"，且并发读会互相
// 覆盖）。
struct Client {
  uv_tcp_t socket{};
  uv_connect_t connect_req{};
  uv_write_t write_req{};
  std::string request;
  std::string response;
  std::string error;
  bool connect_ok = false;
  bool full_response = false;
  char read_buf[16384];
};

void OnAlloc(uv_handle_t* handle, size_t /*suggested*/, uv_buf_t* buf) {
  auto* client = static_cast<Client*>(handle->data);
  *buf = uv_buf_init(client->read_buf, sizeof(client->read_buf));
}

void OnRead(uv_stream_t* stream, ssize_t nread, const uv_buf_t* buf) {
  Client* c = static_cast<Client*>(stream->data);
  if (nread < 0) {
    if (nread == UV_EOF) {
      // 请求带 Connection: close——EOF = 响应完整。
      c->full_response = true;
      uv_close(reinterpret_cast<uv_handle_t*>(&c->socket), nullptr);
      uv_stop(stream->loop);
      return;
    }
    // Windows 上服务端取消读可能以 RST（UV_ECONNRESET）呈现而非 FIN——
    // 错误分支同样要 uv_close，否则收尾 uv_loop_close 报 UV_EBUSY 掩盖
    // 真实错误（审查修复）。
    c->error = "read error";
    uv_close(reinterpret_cast<uv_handle_t*>(&c->socket), nullptr);
    uv_stop(stream->loop);
    return;
  }
  if (nread == 0) return;  // EAGAIN
  c->response.append(buf->base, static_cast<size_t>(nread));
}

void OnWrite(uv_write_t* req, int status) {
  Client* c = static_cast<Client*>(req->data);
  if (status != 0) {
    c->error = "write failed";
    uv_close(reinterpret_cast<uv_handle_t*>(&c->socket), nullptr);
    uv_stop(req->handle->loop);
    return;
  }
  const int r = uv_read_start(reinterpret_cast<uv_stream_t*>(&c->socket),
                              OnAlloc, OnRead);
  if (r != 0) {  // 同步失败：无读回调可指望，直接收尾（回调内不能断言）。
    c->error = "read start failed";
    uv_close(reinterpret_cast<uv_handle_t*>(&c->socket), nullptr);
    uv_stop(req->handle->loop);
  }
}

void OnConnect(uv_connect_t* req, int status) {
  Client* c = static_cast<Client*>(req->data);
  if (status != 0) {
    c->error = "connect failed";
    uv_close(reinterpret_cast<uv_handle_t*>(&c->socket), nullptr);
    uv_stop(req->handle->loop);
    return;
  }
  c->connect_ok = true;
  c->write_req.data = c;
  const uv_buf_t buf =
      uv_buf_init(const_cast<char*>(c->request.data()),
                  static_cast<unsigned int>(c->request.size()));
  uv_write(&c->write_req, reinterpret_cast<uv_stream_t*>(&c->socket), &buf, 1,
           OnWrite);
}

// 构造 HTTP/1.1 POST 请求（Content-Length 用 body 长度计算，避免手数错）。
std::string MakePostRequest(const std::string& body) {
  return "POST /rpc HTTP/1.1\r\n"
         "Host: 127.0.0.1\r\n"
         "Content-Type: application/json\r\n"
         "Content-Length: " +
         std::to_string(body.size()) + "\r\n" +
         "Connection: close\r\n"
         "\r\n" +
         body;
}

}  // namespace

TEST_CASE("server: POST /rpc 集成冒烟（真实 socket 全链路）",
          "[server][integration]") {
  uv_loop_t loop;
  REQUIRE(uv_loop_init(&loop) == 0);

  SmokeHandler handler;
  bf::http_server::Server server(&loop, handler, {});
  REQUIRE(server.Listen("127.0.0.1", 0) == 0);
  const int port = server.BoundPort();
  REQUIRE(port > 0);

  Client client;
  client.request = MakePostRequest(
      R"({"jsonrpc":"2.0","id":1,"method":"echo","params":{}})");
  REQUIRE(uv_tcp_init(&loop, &client.socket) == 0);
  client.socket.data = &client;

  sockaddr_in addr{};
  REQUIRE(uv_ip4_addr("127.0.0.1", port, &addr) == 0);
  client.connect_req.data = &client;
  // 同步失败（如 socket 状态非法）不会触发 OnConnect——返回值必须检查，
  // 否则只能等 10s 看门狗（审查修复）。
  REQUIRE(uv_tcp_connect(&client.connect_req, &client.socket,
                         reinterpret_cast<const sockaddr*>(&addr),
                         OnConnect) == 0);

  // 看门狗：EOF 若 10s 未达（uv_stop 永不触发，uv_run 死循环——Windows
  // 上服务端 close 语义与 Linux 不同，曾致 CI 300s 超时），强制退出并
  // 留下可诊断标志；正常路径不会触发。
  uv_timer_t watchdog{};
  bool watchdog_fired = false;
  REQUIRE(uv_timer_init(&loop, &watchdog) == 0);
  watchdog.data = &watchdog_fired;
  uv_timer_start(
      &watchdog,
      [](uv_timer_t* t) {
        *static_cast<bool*>(t->data) = true;
        uv_stop(t->loop);
      },
      10'000, 0);

  uv_run(&loop, UV_RUN_DEFAULT);
  uv_timer_stop(&watchdog);
  server.Close();  // 与 uv_run 同线程（loop 线程）——bf 契约
  uv_close(reinterpret_cast<uv_handle_t*>(&watchdog), nullptr);
  // EOF 分支的 uv_stop 使 uv_run 在 close 回调（uv__finish_close）执行前
  // 返回——此时所有 uv_close 排队的 handle（客户端 socket + 服务器连接
  // tcp/timer + listener + 看门狗）仍计入 active_handles，直接
  // uv_loop_close 会返回 UV_EBUSY 并泄漏 loop 内部结构（LSan 704B）。
  // Linux 上单次 NOWAIT 即可 finalize 全部；Windows 上 listener 的
  // AcceptEx 取消完成经 IOCP 异步到达，可能晚于一轮 NOWAIT 的 poll
  // 窗口——drain 到无活 handle（uv_loop_alive 计入 closing）为止，
  // 有界迭代防 IOCP 挂起死循环。
  int drain_rounds = 0;
  while (uv_loop_alive(&loop) && drain_rounds++ < 1000) {
    uv_run(&loop, UV_RUN_NOWAIT);
  }
  REQUIRE(uv_loop_close(&loop) == 0);

  // 断言全在主线程（回调只记录状态）。
  REQUIRE_FALSE(watchdog_fired);

  // 断言全在主线程（回调只记录状态）。
  REQUIRE(client.error.empty());
  REQUIRE(client.connect_ok);
  REQUIRE(client.full_response);
  REQUIRE(!client.response.empty());
  const size_t hdr_end = client.response.find("\r\n\r\n");
  REQUIRE(hdr_end != std::string::npos);
  const std::string headers = client.response.substr(0, hdr_end);
  CHECK(headers.find("HTTP/1.1 200") != std::string::npos);
  const std::string body = client.response.substr(hdr_end + 4);
  rapidjson::Document doc;
  doc.Parse(body.c_str());
  REQUIRE(!doc.HasParseError());
  CHECK(doc["id"].GetInt() == 1);
  CHECK(doc["result"]["echoed"].GetBool());
}
