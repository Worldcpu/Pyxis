// SPDX-License-Identifier: MIT
#pragma once

#include <string>
#include <vector>

#include "px/module/flightplan/flight_plan.h"

namespace px {

// 十进制度 → DMS 字符串（"N29°43'9.12\""；lat=true 纬度 N/S，否则 E/W）。
// 秒四舍五入到厘秒并进位（59.99" 不得溢出为 60）。
std::string FormatDms(double decimal_degrees, bool is_latitude);

// .PLN 导出参数（决策 17：起降场与元数据；字符串字段在渲染时 XML 转义）。
struct PlnExportParams {
  std::string title;
  std::string fp_type;  // "IFR" / "VFR"
  double cruising_altitude_ft = 0.0;
  std::string departure_id;
  std::string destination_id;
  double dep_lat = 0.0, dep_lon = 0.0;
  double dest_lat = 0.0, dest_lon = 0.0;
};

// 生成 MSFS/FSX/P3D .PLN XML（决策 17：后端生成，前端只下载/写盘）。
// 航路点直接取 FlightPlan 点序列（坐标 + ident；segment_index 哨兵点跳过）。
// 契约：points 应为**除起降场外**的航路点序列——起降场由
// DepartureID/DestinationPosition 表示，重复导出会生成冗余 waypoint。
std::string RenderPlnXml(const PlnExportParams& params,
                         const std::vector<FlightPoint>& points);

}  // namespace px
