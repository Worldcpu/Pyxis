#pragma once

// SQLite RAII 工具，供 DFD 加载器使用。
// 提供连接句柄和预处理语句句柄的智能指针封装，以及空值安全的列访问器。
// 参考 bravofinder io/loaders/sqlite_util.h。

#include <sqlite3.h>

#include <memory>
#include <string>
#include <string_view>

#include "px/core/result.h"

namespace px {

// RAII 句柄：sqlite3 连接（析构时 sqlite3_close）。
struct SqliteDeleter {
  void operator()(sqlite3* db) const noexcept;
};
using SqliteHandle = std::unique_ptr<sqlite3, SqliteDeleter>;

// RAII 句柄：预处理语句（析构时 sqlite3_finalize）。
struct StmtDeleter {
  void operator()(sqlite3_stmt* stmt) const noexcept;
};
using SqliteStmt = std::unique_ptr<sqlite3_stmt, StmtDeleter>;

// 以只读方式打开 SQLite 数据库。返回句柄，失败时返回 Error (kDataMissing)。
Result<SqliteHandle> OpenDb(const std::string& db_path);

// 准备一条 SQL 语句。失败时返回 Error (kParseError)。
Result<SqliteStmt> Prepare(sqlite3* conn, std::string_view sql);

// 执行一次 Step。有行时返回 true，已耗尽时返回 false；遇到错误返回 Error (kParseError)。
Result<bool> Step(sqlite3_stmt* stmt);

// 反复对 stmt 调用 Step，每行调用一次 on_row，直至 SQLITE_DONE。
// Step 错误传播为 kParseError。
template <class F>
Result<void> ForEachRow(sqlite3_stmt* stmt, F&& on_row) {
  for (;;) {
    Result<bool> r = Step(stmt);
    if (!r) {
      return Err(std::move(r.error()));
    }
    if (!r.value()) {
      return {};
    }
    on_row();
  }
}

// 列访问器：空值安全。Text 进行空格裁剪（DFD 使用等宽空格填充）。
std::string ColumnText(sqlite3_stmt* stmt, int col);
int ColumnInt(sqlite3_stmt* stmt, int col);
double ColumnDouble(sqlite3_stmt* stmt, int col);

// 不进行裁剪的 ColumnText，保留原始字节偏移，用于需要按 ARINC 424
// 固定列位解析的字段（如 waypoint_description_code 的 'E' 结束标志）。
// 空值时返回空字符串。
std::string ColumnTextRaw(sqlite3_stmt* stmt, int col);

}  // namespace px
