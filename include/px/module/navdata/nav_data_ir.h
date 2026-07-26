#pragma once

#include <string>
#include <vector>

#include "px/core/graph_edge.h"
#include "px/core/mora_grid.h"

namespace px {

// 程序数据占位 —— Phase 6/7 将添加完整类型。
struct ProcedureData {};

// 航点类型——区分航路点与各类导航台。参考 bravofinder WaypointKind。
// 底层类型 uint8_t：bfdb 顶点 record 存储为 U8，与 bravofinder 一致。
enum class WaypointKind : uint8_t {
  kFix,   // 航路点或终端区航点
  kVor,   // VOR / VOR-DME
  kNdb,   // NDB
  kDme,   // DME / TACAN
  kOther  // 其他导航台 (保留为可路由点)
};

// 原始航点记录——解析器产出，GraphBuilder 消费。
struct RawWaypoint {
  std::string ident;
  std::string region;  // ICAO 区域码 (2字符)
  double latitude = 0.0;
  double longitude = 0.0;
  WaypointKind kind = WaypointKind::kFix;
};

// 原始航段记录。
struct RawSegment {
  std::string from_ident;
  std::string from_region;
  std::string to_ident;
  std::string to_region;
  std::string airway;  // 航路名，空串 = DCT
  AirwayDirection direction = AirwayDirection::kBoth;
  AirwayLevel level = AirwayLevel::kLow;
  int base_fl = 0;  // 底高（FL），0 = 不限
  int top_fl = 0;   // 顶高（FL），0 = 不限
};

// 机场记录——GraphBuilder 据此填充 airport_index_，
// 备降场选择 (Phase 8) 也需要机场坐标/标高。
struct RawAirport {
  std::string icao;       // 4字母 ICAO 代码
  std::string name;       // 机场名称 (如 "PPM’s Airport")
  double latitude = 0.0;
  double longitude = 0.0;
  double elevation_ft = 0.0;
};

// 从 Loader 产出的统一中间表示。GraphBuilder 和 ProcedureConnector 不感知
// 数据来源 (PMDG SQLite / Fenix SQLite / 未来其他格式)。
// 参考了 bravofinder NavData。
struct NavDataIR {
  std::vector<RawWaypoint> waypoints;
  std::vector<RawSegment> segments;
  std::vector<RawAirport> airports;
  MoraGrid mora;
  std::string source_loader;
  std::string airac_cycle;
};

}  // namespace px
