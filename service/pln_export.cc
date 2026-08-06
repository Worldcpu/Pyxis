// SPDX-License-Identifier: MIT
// .PLN XML 生成（决策 17：MSFS/FSX/P3D 格式，后端生成）。
#include "px/service/pln_export.h"

#include <cmath>
#include <cstdio>
#include <string>

namespace px {

namespace {

// XML 转义（& < > " '——用户可控字段（title/ident 等）直拼会产出非法
// XML，MSFS 拒载 .PLN）。
std::string EscapeXml(const std::string& text) {
  std::string out;
  out.reserve(text.size());
  for (const char c : text) {
    switch (c) {
      case '&':
        out += "&amp;";
        break;
      case '<':
        out += "&lt;";
        break;
      case '>':
        out += "&gt;";
        break;
      case '"':
        out += "&quot;";
        break;
      case '\'':
        out += "&apos;";
        break;
      default:
        out += c;
    }
  }
  return out;
}

// DMS 组件格式化：度分秒，秒保留两位。秒四舍五入后进位（浮点误差
// 59.9964" 会舍入为 60.00"——必须进位到分，分钟 60 进位到度）。
void AppendDms(std::string* out, double value) {
  const double abs_value = std::abs(value);
  int degrees = static_cast<int>(abs_value);
  const double minutes_raw = (abs_value - degrees) * 60.0;
  int minutes = static_cast<int>(minutes_raw);
  double seconds = std::round((minutes_raw - minutes) * 60.0 * 100.0) / 100.0;
  if (seconds >= 60.0) {
    seconds = 0.0;
    ++minutes;
  }
  if (minutes >= 60) {
    minutes = 0;
    ++degrees;
  }
  char buffer[32];
  std::snprintf(buffer, sizeof(buffer), "%d°%d'%.2f\"", degrees, minutes,
                seconds);
  out->append(buffer);
}

}  // namespace

std::string FormatDms(double decimal_degrees, bool is_latitude) {
  std::string result;
  if (is_latitude) {
    result += decimal_degrees < 0.0 ? 'S' : 'N';
  } else {
    result += decimal_degrees < 0.0 ? 'W' : 'E';
  }
  AppendDms(&result, decimal_degrees);
  return result;
}

std::string RenderPlnXml(const PlnExportParams& params,
                         const std::vector<FlightPoint>& points) {
  std::string xml;
  xml += "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n";
  xml += "<SimBase.Document Type=\"AcePlans\" version=\"1,0\">\n";
  xml += "  <FlightPlan.FlightPlan>\n";
  xml += "    <Title>" + EscapeXml(params.title) + "</Title>\n";
  xml += "    <FPType>" + EscapeXml(params.fp_type) + "</FPType>\n";
  xml += "    <CruisingAltitude>" +
         std::to_string(static_cast<long long>(params.cruising_altitude_ft)) +
         "</CruisingAltitude>\n";
  xml +=
      "    <DepartureID>" + EscapeXml(params.departure_id) + "</DepartureID>\n";
  xml += "    <DestinationID>" + EscapeXml(params.destination_id) +
         "</DestinationID>\n";
  xml += "    <DeparturePosition>" + FormatDms(params.dep_lat, true) + "," +
         FormatDms(params.dep_lon, false) + ",+000000.00</DeparturePosition>\n";
  xml += "    <DestinationPosition>" + FormatDms(params.dest_lat, true) + "," +
         FormatDms(params.dest_lon, false) +
         ",+000000.00</DestinationPosition>\n";
  for (const auto& point : points) {
    // segment_index 哨兵（segments 空）点无段归属——跳过导出。
    if (point.segment_index < 0) continue;
    xml += "    <ATCWaypoint id=\"" + EscapeXml(point.ident) + "\">\n";
    xml += "      <ATCWaypointType>Intersection</ATCWaypointType>\n";
    xml += "      <WorldPosition>" + FormatDms(point.latitude, true) + "," +
           FormatDms(point.longitude, false) + ",+000000.00</WorldPosition>\n";
    xml += "      <ICAO><ICAOIdent>" + EscapeXml(point.ident) +
           "</ICAOIdent></ICAO>\n";
    xml += "    </ATCWaypoint>\n";
  }
  xml += "  </FlightPlan.FlightPlan>\n";
  xml += "</SimBase.Document>\n";
  return xml;
}

}  // namespace px
