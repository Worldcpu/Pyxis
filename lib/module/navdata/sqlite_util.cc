#include "sqlite_util.h"

#include <string>
#include <utility>

namespace px {

void SqliteDeleter::operator()(sqlite3* db) const noexcept {
  if (db != nullptr) {
    sqlite3_close(db);
  }
}

void StmtDeleter::operator()(sqlite3_stmt* stmt) const noexcept {
  if (stmt != nullptr) {
    sqlite3_finalize(stmt);
  }
}

Result<SqliteHandle> OpenDb(const std::string& db_path) {
  sqlite3* raw = nullptr;
  const int rc = sqlite3_open_v2(db_path.c_str(), &raw,
                                  SQLITE_OPEN_READONLY | SQLITE_OPEN_NOMUTEX,
                                  nullptr);
  if (rc != SQLITE_OK) {
    const std::string msg =
        raw != nullptr ? sqlite3_errmsg(raw) : "cannot open database";
    if (raw != nullptr) {
      sqlite3_close(raw);
    }
    return Err(Error(ErrorCode::kDataMissing, "SQLite: " + msg));
  }
  return Ok(SqliteHandle(raw));
}

Result<SqliteStmt> Prepare(sqlite3* conn, std::string_view sql) {
  sqlite3_stmt* stmt = nullptr;
  const int rc = sqlite3_prepare_v2(conn, sql.data(),
                                     static_cast<int>(sql.size()), &stmt,
                                     nullptr);
  if (rc != SQLITE_OK) {
    return Err(Error(ErrorCode::kParseError,
                     std::string("SQLite prepare failed: ") +
                         sqlite3_errmsg(conn)));
  }
  return Ok(SqliteStmt(stmt));
}

Result<bool> Step(sqlite3_stmt* stmt) {
  const int rc = sqlite3_step(stmt);
  if (rc == SQLITE_ROW) {
    return Ok(true);
  }
  if (rc == SQLITE_DONE) {
    return Ok(false);
  }
  return Err(
      Error(ErrorCode::kParseError, "SQLite step failed"));
}

std::string ColumnText(sqlite3_stmt* stmt, int col) {
  const unsigned char* t = sqlite3_column_text(stmt, col);
  if (t == nullptr) {
    return {};
  }
  std::string s(reinterpret_cast<const char*>(t));
  // DFD 用空格填充定宽 TEXT 列；进行裁剪。
  const auto b = s.find_first_not_of(' ');
  if (b == std::string::npos) {
    return {};
  }
  const auto e = s.find_last_not_of(' ');
  return s.substr(b, e - b + 1);
}

std::string ColumnTextRaw(sqlite3_stmt* stmt, int col) {
  const unsigned char* t = sqlite3_column_text(stmt, col);
  if (t == nullptr) {
    return {};
  }
  // 不进行裁剪：保留前导/后置空格，使 ARINC 424 固定列位对齐。
  const int n = sqlite3_column_bytes(stmt, col);
  return std::string(reinterpret_cast<const char*>(t),
                     static_cast<size_t>(n < 0 ? 0 : n));
}

int ColumnInt(sqlite3_stmt* stmt, int col) {
  if (sqlite3_column_type(stmt, col) == SQLITE_NULL) {
    return 0;
  }
  return sqlite3_column_int(stmt, col);
}

double ColumnDouble(sqlite3_stmt* stmt, int col) {
  if (sqlite3_column_type(stmt, col) == SQLITE_NULL) {
    return 0.0;
  }
  return sqlite3_column_double(stmt, col);
}

}  // namespace px
