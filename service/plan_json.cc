// SPDX-License-Identifier: MIT
// plan JSON 渲染（决策 7/9/14：JSON 唯一真源，RapidJSON Writer 流式输出）。
#include "px/service/plan_json.h"

#include <rapidjson/stringbuffer.h>
#include <rapidjson/writer.h>

#include <string>

namespace px {

namespace {

// SegmentKind → JSON 字符串（决策 2c 枚举语义）。switch 穷举无 default：
// 新增枚举成员由 -Wswitch 兜底告警，避免静默输出错误 kind。
const char* KindName(SegmentKind kind) {
  switch (kind) {
    case SegmentKind::kSid:
      return "sid";
    case SegmentKind::kEnroute:
      return "enroute";
    case SegmentKind::kStar:
      return "star";
    case SegmentKind::kApproach:
      return "approach";
    case SegmentKind::kAlternate:
      return "alternate";
  }
  // 尾 return 仅满足 -Werror 下的 -Wreturn-type（GCC 无法证明 switch 穷举）。
  return "unknown";
}

// AltitudeRule → JSON 字符串（决策 27 三态：kAuto 不塌缩）。
const char* RuleName(AltitudeRule rule) {
  switch (rule) {
    case AltitudeRule::kAuto:
      return "auto";
    case AltitudeRule::kIcao:
      return "icao";
    case AltitudeRule::kChina:
      return "china";
  }
  return "unknown";
}

// 航段 JSON 形状（generate 阶段；决策 2c 字段集）。
void WriteSegment(rapidjson::Writer<rapidjson::StringBuffer>& writer,
                  const FlightSegment& segment) {
  writer.StartObject();
  writer.Key("kind");
  writer.String(KindName(segment.kind));
  writer.Key("from");
  writer.String(segment.from_ident.c_str());
  writer.Key("to");
  writer.String(segment.to_ident.c_str());
  writer.Key("via");
  writer.String(segment.via.c_str());
  writer.Key("distance_nm");
  writer.Double(segment.distance_nm);
  writer.EndObject();
}

// 航路点 JSON 形状（含坐标与段归属）。
void WritePoint(rapidjson::Writer<rapidjson::StringBuffer>& writer,
                const FlightPoint& point) {
  writer.StartObject();
  writer.Key("ident");
  writer.String(point.ident.c_str());
  writer.Key("via");
  writer.String(point.via.c_str());
  writer.Key("lat");
  writer.Double(point.latitude);
  writer.Key("lon");
  writer.Double(point.longitude);
  writer.Key("segment_index");
  writer.Int(point.segment_index);
  writer.EndObject();
}

// 燃油字段位：未计算输出 null（缺 key 与 null 语义不同，前端分支依赖）。
void WriteOptionalKg(rapidjson::Writer<rapidjson::StringBuffer>& writer,
                     const char* key, const std::optional<double>& value) {
  writer.Key(key);
  if (value.has_value()) {
    writer.Double(*value);
  } else {
    writer.Null();
  }
}

}  // namespace

std::string RenderPlanCandidatesJson(const std::vector<FlightPlan>& candidates,
                                     uint32_t seed) {
  rapidjson::StringBuffer buffer;
  rapidjson::Writer<rapidjson::StringBuffer> writer(buffer);
  writer.StartArray();
  for (size_t i = 0; i < candidates.size(); ++i) {
    const auto& plan = candidates[i];
    writer.StartObject();
    writer.Key("index");
    writer.Uint64(static_cast<uint64_t>(i));
    writer.Key("route_string");
    writer.String(plan.route_string.c_str());

    // 距离：总分 + 分阶段（dep = SID 段和；enroute = 航路段和；
    // arr = STAR + 进近段和；备降段不计入候选）。
    double total = 0.0, dep = 0.0, enroute = 0.0, arr = 0.0;
    for (const auto& segment : plan.segments) {
      total += segment.distance_nm;
      switch (segment.kind) {
        case SegmentKind::kSid:
          dep += segment.distance_nm;
          break;
        case SegmentKind::kEnroute:
          enroute += segment.distance_nm;
          break;
        case SegmentKind::kStar:
        case SegmentKind::kApproach:
          arr += segment.distance_nm;
          break;
        case SegmentKind::kAlternate:
          break;
      }
    }
    writer.Key("total_distance_nm");
    writer.Double(total);
    writer.Key("distances");
    writer.StartObject();
    writer.Key("dep_nm");
    writer.Double(dep);
    writer.Key("enroute_nm");
    writer.Double(enroute);
    writer.Key("arr_nm");
    writer.Double(arr);
    writer.EndObject();

    // 程序/跑道/连接（决策 9；空串原样输出）。
    writer.Key("sid");
    writer.String(plan.sid.c_str());
    writer.Key("star");
    writer.String(plan.star.c_str());
    writer.Key("dep_runway");
    writer.String(plan.dep_runway.c_str());
    writer.Key("arr_runway");
    writer.String(plan.arr_runway.c_str());
    writer.Key("dep_connection");
    writer.String(plan.dep_connection.c_str());
    writer.Key("arr_connection");
    writer.String(plan.arr_connection.c_str());

    writer.Key("seed");
    writer.Uint(seed);

    // 决策 9：候选只带完整点序列，不带 segments。
    writer.Key("points");
    writer.StartArray();
    for (const auto& point : plan.points) {
      WritePoint(writer, point);
    }
    writer.EndArray();
    writer.EndObject();
  }
  writer.EndArray();
  return buffer.GetString();
}

std::string RenderPlanJson(const FlightPlan& plan) {
  rapidjson::StringBuffer buffer;
  rapidjson::Writer<rapidjson::StringBuffer> writer(buffer);
  writer.StartObject();

  // 决策 14：generate 响应含航路（route_string + segments + 点序列 +
  // 程序/跑道/连接——与候选契约一致，生成页显示 SID/STAR 无需另取）。
  writer.Key("route");
  writer.StartObject();
  writer.Key("route_string");
  writer.String(plan.route_string.c_str());
  writer.Key("sid");
  writer.String(plan.sid.c_str());
  writer.Key("star");
  writer.String(plan.star.c_str());
  writer.Key("dep_runway");
  writer.String(plan.dep_runway.c_str());
  writer.Key("arr_runway");
  writer.String(plan.arr_runway.c_str());
  writer.Key("dep_connection");
  writer.String(plan.dep_connection.c_str());
  writer.Key("arr_connection");
  writer.String(plan.arr_connection.c_str());
  writer.Key("segments");
  writer.StartArray();
  for (const auto& segment : plan.segments) {
    WriteSegment(writer, segment);
  }
  writer.EndArray();
  writer.Key("points");
  writer.StartArray();
  for (const auto& point : plan.points) {
    WritePoint(writer, point);
  }
  writer.EndArray();
  writer.EndObject();

  writer.Key("altitude");
  writer.StartObject();
  writer.Key("fl");
  writer.Int(plan.altitude.fl);
  writer.Key("meters");
  writer.Int(plan.altitude.meters);
  writer.Key("rule");
  writer.String(RuleName(plan.altitude.rule));
  writer.Key("rationale");
  writer.String(plan.altitude.rationale.c_str());
  writer.Key("manual");
  writer.Bool(plan.altitude.manual);
  writer.EndObject();

  writer.Key("fuel");
  writer.StartObject();
  WriteOptionalKg(writer, "taxi_kg", plan.fuel.taxi_kg);
  WriteOptionalKg(writer, "trip_kg", plan.fuel.trip_kg);
  WriteOptionalKg(writer, "contingency_kg", plan.fuel.contingency_kg);
  WriteOptionalKg(writer, "alternate_kg", plan.fuel.alternate_kg);
  WriteOptionalKg(writer, "final_reserve_kg", plan.fuel.final_reserve_kg);
  WriteOptionalKg(writer, "extra_kg", plan.fuel.extra_kg);
  WriteOptionalKg(writer, "block_kg", plan.fuel.block_kg);
  writer.EndObject();

  writer.Key("weights");
  writer.StartObject();
  writer.Key("dow_kg");
  writer.Double(plan.weights.dow_kg);
  writer.Key("zfw_kg");
  writer.Double(plan.weights.zfw_kg);
  writer.Key("tow_kg");
  writer.Double(plan.weights.tow_kg);
  writer.Key("lw_kg");
  writer.Double(plan.weights.lw_kg);
  writer.EndObject();

  writer.Key("checks");
  writer.StartObject();
  writer.Key("status");
  writer.Int(static_cast<int>(plan.checks.status));
  writer.Key("warnings");
  writer.StartArray();
  for (const auto& warning : plan.checks.warnings) {
    writer.String(warning.c_str());
  }
  writer.EndArray();
  writer.EndObject();

  writer.Key("mora_checked");
  writer.Bool(plan.mora_checked);
  writer.Key("experimental");
  writer.Bool(plan.experimental);

  writer.EndObject();
  return buffer.GetString();
}

}  // namespace px
