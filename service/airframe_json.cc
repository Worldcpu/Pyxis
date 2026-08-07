// SPDX-License-Identifier: MIT
// airframe JSON 转换与文件存取实现（决策 21/28/38）。
#include "px/service/airframe_json.h"

#include <filesystem>
#include <fstream>
#include <iterator>
#include <string_view>
#include <system_error>
#include <utility>

namespace px {

namespace {

const char* PerfSourceName(PerfSource source) {
  switch (source) {
    case PerfSource::kLnm:
      return "lnm";
    case PerfSource::kOpenAp:
      return "openap";
    case PerfSource::kCustom:
      return "custom";
    case PerfSource::kFcom:
      return "fcom";
  }
  return "lnm";
}

bool ParsePerfSource(const rapidjson::Value& value, PerfSource* out) {
  if (!value.IsString()) return false;
  const std::string_view s = value.GetString();
  if (s == "lnm") {
    *out = PerfSource::kLnm;
  } else if (s == "openap") {
    *out = PerfSource::kOpenAp;
  } else if (s == "custom") {
    *out = PerfSource::kCustom;
  } else if (s == "fcom") {
    *out = PerfSource::kFcom;
  } else {
    return false;
  }
  return true;
}

bool GetDouble(const rapidjson::Value& value, const char* key, double* out) {
  if (!value.HasMember(key) || !value[key].IsNumber()) return false;
  *out = value[key].GetDouble();
  return true;
}

}  // namespace

bool ParseAirframe(const rapidjson::Value& value, Airframe* out) {
  if (!value.IsObject() || !value.HasMember("type") ||
      !value["type"].IsString() || !value.HasMember("variant") ||
      !value["variant"].IsString() || !value.HasMember("perf_source")) {
    return false;
  }
  Airframe airframe;
  airframe.type = value["type"].GetString();
  airframe.variant = value["variant"].GetString();
  if (!ParsePerfSource(value["perf_source"], &airframe.perf_source))
    return false;
  if (!GetDouble(value, "dow_kg", &airframe.dow_kg) ||
      !GetDouble(value, "mzfw_kg", &airframe.mzfw_kg) ||
      !GetDouble(value, "mtow_kg", &airframe.mtow_kg) ||
      !GetDouble(value, "mlw_kg", &airframe.mlw_kg) ||
      !GetDouble(value, "service_ceiling_ft", &airframe.service_ceiling_ft) ||
      !GetDouble(value, "unit_pax_kg", &airframe.unit_pax_kg) ||
      !GetDouble(value, "unit_bag_kg", &airframe.unit_bag_kg)) {
    return false;
  }
  // 决策 38：巡航速度可选（kt TAS）。
  if (value.HasMember("cruise_speed_kt")) {
    if (!value["cruise_speed_kt"].IsInt()) return false;
    airframe.cruise_speed_kt = value["cruise_speed_kt"].GetInt();
  }
  *out = std::move(airframe);
  return true;
}

void WriteAirframeJson(rapidjson::Writer<rapidjson::StringBuffer>& writer,
                       const Airframe& airframe) {
  writer.StartObject();
  writer.Key("type");
  writer.String(airframe.type.c_str());
  writer.Key("variant");
  writer.String(airframe.variant.c_str());
  writer.Key("perf_source");
  writer.String(PerfSourceName(airframe.perf_source));
  writer.Key("dow_kg");
  writer.Double(airframe.dow_kg);
  writer.Key("mzfw_kg");
  writer.Double(airframe.mzfw_kg);
  writer.Key("mtow_kg");
  writer.Double(airframe.mtow_kg);
  writer.Key("mlw_kg");
  writer.Double(airframe.mlw_kg);
  writer.Key("service_ceiling_ft");
  writer.Double(airframe.service_ceiling_ft);
  writer.Key("unit_pax_kg");
  writer.Double(airframe.unit_pax_kg);
  writer.Key("unit_bag_kg");
  writer.Double(airframe.unit_bag_kg);
  if (airframe.cruise_speed_kt.has_value()) {
    writer.Key("cruise_speed_kt");
    writer.Int(*airframe.cruise_speed_kt);
  }
  writer.EndObject();
}

std::vector<Airframe> LoadAirframes(const std::string& file) {
  std::vector<Airframe> out;
  std::ifstream in(file);
  if (!in) return out;  // 缺失 = 空档案
  const std::string content((std::istreambuf_iterator<char>(in)),
                            std::istreambuf_iterator<char>());
  rapidjson::Document doc;
  doc.Parse(content.c_str());
  if (doc.HasParseError() || !doc.IsObject() || !doc.HasMember("airframes") ||
      !doc["airframes"].IsArray()) {
    return out;  // 损坏文件 = 空档案（不崩溃；upsert 会重建）
  }
  for (const auto& value : doc["airframes"].GetArray()) {
    Airframe airframe;
    if (ParseAirframe(value, &airframe)) out.push_back(std::move(airframe));
  }
  return out;
}

Result<void> StoreAirframes(const std::string& file,
                            const std::vector<Airframe>& airframes) {
  rapidjson::StringBuffer buffer;
  rapidjson::Writer<rapidjson::StringBuffer> writer(buffer);
  writer.StartObject();
  writer.Key("airframes");
  writer.StartArray();
  for (const auto& airframe : airframes) {
    WriteAirframeJson(writer, airframe);
  }
  writer.EndArray();
  writer.EndObject();
  // 原子写：临时文件 + rename（覆盖语义——MSVC rename 对已存在目标
  // 失败，用 filesystem::rename；审查修复），避免中断留下半写档案。
  const std::string tmp = file + ".tmp";
  {
    std::ofstream out(tmp, std::ios::binary);
    if (!out) {
      return Err(Error(ErrorCode::kDataMissing, "无法写入 " + tmp));
    }
    out << buffer.GetString();
    out.flush();
    if (!out) {  // 写盘失败（磁盘满等）不得覆盖旧档案
      return Err(Error(ErrorCode::kInternalError, "写入失败: " + tmp));
    }
  }
  std::error_code ec;
  std::filesystem::rename(tmp, file, ec);
  if (ec) {
    return Err(Error(ErrorCode::kInternalError,
                     "无法替换 " + file + ": " + ec.message()));
  }
  return Ok();
}

}  // namespace px
