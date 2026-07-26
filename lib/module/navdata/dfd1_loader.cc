#include "dfd1_loader.h"

#include <sqlite3.h>

#include <algorithm>
#include <charconv>
#include <cmath>
#include <cstdint>
#include <string>
#include <unordered_set>
#include <utility>

#include "px/core/ident.h"
#include "sqlite_util.h"

namespace px {
namespace {

// 每飞行高度的英尺数（1 FL = 100 ft）；DFD 以英尺存储高度。
constexpr int kFeetPerFlightLevel = 100;
// DFD 中 99999 表示"无上限"。
constexpr int kUnknownAltitudeFt = 99999;

// 映射 DFD direction_restriction 字符串到 AirwayDirection。
AirwayDirection ParseDirection(const std::string& token) {
  if (token == "F") return AirwayDirection::kForward;
  if (token == "B") return AirwayDirection::kBackward;
  return AirwayDirection::kBoth;
}

// 映射 DFD flightlevel 字符串到 AirwayLevel。
AirwayLevel ParseAirwayLevel(const std::string& token) {
  if (token == "H") return AirwayLevel::kHigh;
  if (token == "B") return AirwayLevel::kBoth;
  return AirwayLevel::kLow;  // 'L' 或未识别值
}

// --- 逐表加载器。每个加载器执行一条 SQL 查询并将结果追加到 data。 ---

Result<void> LoadEnrouteWaypoints(sqlite3* conn, NavDataIR& data,
                                  std::unordered_set<Ident>& seen) {
  // 0=icao_code 1=waypoint_identifier 2=lat 3=lon
  Result<SqliteStmt> s = Prepare(
      conn,
      "SELECT icao_code, waypoint_identifier, waypoint_latitude, "
      "waypoint_longitude FROM tbl_enroute_waypoints");
  if (!s) return Err(std::move(s).error());
  sqlite3_stmt* stmt = s.value().get();
  return ForEachRow(stmt, [&]() {
    const Ident key(ColumnText(stmt, 1), ColumnText(stmt, 0));
    if (!seen.insert(key).second) return;
    data.waypoints.push_back(RawWaypoint{
        key.ident, key.region, ColumnDouble(stmt, 2), ColumnDouble(stmt, 3),
        WaypointKind::kFix});
  });
}

Result<void> LoadTerminalWaypoints(sqlite3* conn, NavDataIR& data,
                                   std::unordered_set<Ident>& seen) {
  // 0=icao_code 1=waypoint_identifier 2=lat 3=lon
  // 使用 icao_code（2 字符 ICAO 区域），而非 region_code（机场专属4字符码）。
  Result<SqliteStmt> s = Prepare(
      conn,
      "SELECT icao_code, waypoint_identifier, waypoint_latitude, "
      "waypoint_longitude FROM tbl_terminal_waypoints");
  if (!s) return Err(std::move(s).error());
  sqlite3_stmt* stmt = s.value().get();
  return ForEachRow(stmt, [&]() {
    const Ident key(ColumnText(stmt, 1), ColumnText(stmt, 0));
    if (!seen.insert(key).second) return;
    data.waypoints.push_back(RawWaypoint{
        key.ident, key.region, ColumnDouble(stmt, 2), ColumnDouble(stmt, 3),
        WaypointKind::kFix});
  });
}

Result<void> LoadVhfNavaids(sqlite3* conn, NavDataIR& data,
                            std::unordered_set<Ident>& seen) {
  // 0=icao_code 1=vor_identifier 2=lat 3=lon
  Result<SqliteStmt> s = Prepare(
      conn,
      "SELECT icao_code, vor_identifier, vor_latitude, vor_longitude "
      "FROM tbl_vhfnavaids");
  if (!s) return Err(std::move(s).error());
  sqlite3_stmt* stmt = s.value().get();
  return ForEachRow(stmt, [&]() {
    const Ident key(ColumnText(stmt, 1), ColumnText(stmt, 0));
    if (!seen.insert(key).second) return;
    data.waypoints.push_back(RawWaypoint{
        key.ident, key.region, ColumnDouble(stmt, 2), ColumnDouble(stmt, 3),
        WaypointKind::kVor});
  });
}

Result<void> LoadNdbNavaids(sqlite3* conn, NavDataIR& data,
                            const char* table,
                            std::unordered_set<Ident>& seen) {
  // 0=icao_code 1=ndb_identifier 2=lat 3=lon
  const std::string sql = std::string(
      "SELECT icao_code, ndb_identifier, ndb_latitude, ndb_longitude FROM ") +
      table;
  Result<SqliteStmt> s = Prepare(conn, sql);
  if (!s) return Err(std::move(s).error());
  sqlite3_stmt* stmt = s.value().get();
  return ForEachRow(stmt, [&]() {
    const Ident key(ColumnText(stmt, 1), ColumnText(stmt, 0));
    if (!seen.insert(key).second) return;
    data.waypoints.push_back(RawWaypoint{
        key.ident, key.region, ColumnDouble(stmt, 2), ColumnDouble(stmt, 3),
        WaypointKind::kNdb});
  });
}

Result<void> LoadAirways(sqlite3* conn, NavDataIR& data) {
  // 每行是航路上的一个点。连续的同一 route_identifier（按 seqno 排序）
  // 的行构成航段。
  // 注意 waypoint_description_code 第 2 字节（索引 1）== 'E' 表示航路结束。
  Result<SqliteStmt> s = Prepare(
      conn,
      "SELECT route_identifier, seqno, waypoint_identifier, icao_code, "
      "direction_restriction, flightlevel, minimum_altitude1, "
      "maximum_altitude, waypoint_description_code "
      "FROM tbl_enroute_airways ORDER BY route_identifier, seqno");
  if (!s) return Err(std::move(s).error());
  sqlite3_stmt* stmt = s.value().get();

  bool have_prev = false;
  bool prev_is_awy_end = false;
  std::string prev_route, prev_ident, prev_icao, prev_dir, prev_level;
  int prev_min_alt = 0, prev_max_alt = 0;

  return ForEachRow(stmt, [&]() {
    const std::string route = ColumnText(stmt, 0);
    const std::string ident = ColumnText(stmt, 2);
    const std::string icao = ColumnText(stmt, 3);
    if (have_prev && route == prev_route && !prev_is_awy_end) {
      // 航段属性取自前一行：direction/level/alt 描述离开前一个点的出航航段。
      RawSegment seg;
      seg.from_ident = prev_ident;
      seg.from_region = prev_icao;
      seg.to_ident = ident;
      seg.to_region = icao;
      seg.airway = prev_route;
      seg.direction = ParseDirection(prev_dir);
      seg.level = ParseAirwayLevel(prev_level);
      seg.base_fl = prev_min_alt / kFeetPerFlightLevel;
      // 99999 表示无上限，映射为 0
      seg.top_fl = (prev_max_alt >= kUnknownAltitudeFt)
                       ? 0
                       : (prev_max_alt / kFeetPerFlightLevel);
      data.segments.push_back(std::move(seg));
    }
    prev_route = route;
    prev_ident = ident;
    prev_icao = icao;
    prev_dir = ColumnText(stmt, 4);
    prev_level = ColumnText(stmt, 5);
    prev_min_alt = ColumnInt(stmt, 6);
    prev_max_alt = ColumnInt(stmt, 7);
    // waypoint_description_code 第 2 字节 == 'E' 标记航路字符串末尾。
    // 使用 ColumnTextRaw 保留原始字节偏移，不裁剪空格。
    const std::string desc = ColumnTextRaw(stmt, 8);
    prev_is_awy_end = desc.size() > 1 && desc[1] == 'E';
    have_prev = true;
  });
}

Result<void> LoadAirports(sqlite3* conn, NavDataIR& data) {
  // 0=airport_identifier 1=airport_name 2=lat 3=lon 4=elevation
  Result<SqliteStmt> s = Prepare(
      conn,
      "SELECT airport_identifier, airport_name, airport_ref_latitude, "
      "airport_ref_longitude, elevation FROM tbl_airports");
  if (!s) return Err(std::move(s).error());
  sqlite3_stmt* stmt = s.value().get();
  return ForEachRow(stmt, [&]() {
    RawAirport ap;
    ap.icao = ColumnText(stmt, 0);
    ap.name = ColumnText(stmt, 1);
    ap.latitude = ColumnDouble(stmt, 2);
    ap.longitude = ColumnDouble(stmt, 3);
    ap.elevation_ft = static_cast<double>(ColumnInt(stmt, 4));
    data.airports.push_back(std::move(ap));
  });
}

Result<void> LoadGridMora(sqlite3* conn, NavDataIR& data) {
  // v1: starting_latitude/longitude 为 INTEGER；mora01..mora30 为 TEXT(3)
  // （"010" 或空白）。每行覆盖经度上 30 个 1 度单元格。
  Result<SqliteStmt> s = Prepare(
      conn,
      "SELECT starting_latitude, starting_longitude, "
      "mora01, mora02, mora03, mora04, mora05, mora06, mora07, mora08, "
      "mora09, mora10, "
      "mora11, mora12, mora13, mora14, mora15, mora16, mora17, mora18, "
      "mora19, mora20, "
      "mora21, mora22, mora23, mora24, mora25, mora26, mora27, mora28, "
      "mora29, mora30 "
      "FROM tbl_grid_mora");
  if (!s) return Err(std::move(s).error());
  sqlite3_stmt* stmt = s.value().get();
  return ForEachRow(stmt, [&]() {
    const int lat = ColumnInt(stmt, 0);
    const int lon0 = ColumnInt(stmt, 1);
    for (int i = 0; i < 30; ++i) {
      const std::string v = ColumnText(stmt, 2 + i);
      if (v.empty()) continue;
      int value = 0;
      const auto [ptr, ec] =
          std::from_chars(v.data(), v.data() + v.size(), value);
      if (ec != std::errc{}) continue;
      if (value > 0) {
        data.mora.SetCell(lat, lon0 + i, static_cast<int16_t>(value));
      }
    }
  });
}

}  // namespace

Result<NavDataIR> Dfd1Loader::LoadNavData(
    const std::string& source_path) const {
  Result<SqliteHandle> conn = OpenDb(source_path);
  if (!conn) {
    return Err(std::move(conn).error());
  }

  NavDataIR data;
  data.source_loader = Name();

  // AIRAC 周期来自 tbl_header.current_airac（如 "2607"）。
  Result<SqliteStmt> s =
      Prepare(conn.value().get(),
              "SELECT current_airac FROM tbl_header LIMIT 1");
  if (s) {
    Result<bool> row = Step(s.value().get());
    if (row && row.value()) {
      data.airac_cycle = ColumnText(s.value().get(), 0);
    }
  }

  // 各表加载器链。每个返回 Result<void>；Step 错误（损坏、IO）传播为加载失败。
  std::unordered_set<Ident> seen;
  Result<void> load = Ok();

  // 航路航点先加载，使 (ident, region) 优先被占用。
  load = LoadEnrouteWaypoints(conn.value().get(), data, seen);
  if (!load) return Err(std::move(load).error());

  // VOR 导航台：可能覆盖已存在的航点，但 seen 防止重复。
  load = LoadVhfNavaids(conn.value().get(), data, seen);
  if (!load) return Err(std::move(load).error());

  // NDB 导航台（航路 + 终端）。
  load = LoadNdbNavaids(conn.value().get(), data, "tbl_enroute_ndbnavaids",
                        seen);
  if (!load) return Err(std::move(load).error());

  load = LoadNdbNavaids(conn.value().get(), data, "tbl_terminal_ndbnavaids",
                        seen);
  if (!load) return Err(std::move(load).error());

  // 终端航点最后加载：已用 icao_code 作为区域码，能与同键航路点共享 seen 集，
  // 先写入的航路点/导航台条目优先。
  load = LoadTerminalWaypoints(conn.value().get(), data, seen);
  if (!load) return Err(std::move(load).error());

  // 航路。
  load = LoadAirways(conn.value().get(), data);
  if (!load) return Err(std::move(load).error());

  // 机场。
  load = LoadAirports(conn.value().get(), data);
  if (!load) return Err(std::move(load).error());

  // MORA 网格。
  load = LoadGridMora(conn.value().get(), data);
  if (!load) return Err(std::move(load).error());

  return Ok(std::move(data));
}

std::optional<ProcedureData> Dfd1Loader::LoadProcedure(
    const std::string& source_path,
    const std::string& icao) const {
  // Phase 6 桩实现。
  (void)source_path;
  (void)icao;
  return std::nullopt;
}

}  // namespace px
