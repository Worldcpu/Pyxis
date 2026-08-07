// SPDX-License-Identifier: MIT
// profile JSON 转换与 profiles.json 文件存取（决策 55：偏好后端 profile，
// airframe 同款模式）。

#include "px/service/profile_json.h"

#include <rapidjson/document.h>

#include <filesystem>
#include <fstream>

namespace px {

bool ParseProfile(const rapidjson::Value& value, Profile* out) {
  if (!value.IsObject() || !value.HasMember("name") ||
      !value["name"].IsString()) {
    return false;
  }
  Profile profile;
  profile.name = value["name"].GetString();
  if (value.HasMember("k")) {
    if (!value["k"].IsInt()) return false;
    profile.k = value["k"].GetInt();
  }
  if (value.HasMember("level")) {
    if (!value["level"].IsString()) return false;
    profile.level = value["level"].GetString();
  }
  if (value.HasMember("min_fl")) {
    if (!value["min_fl"].IsInt()) return false;
    profile.min_fl = value["min_fl"].GetInt();
  }
  if (value.HasMember("max_fl")) {
    if (!value["max_fl"].IsInt()) return false;
    profile.max_fl = value["max_fl"].GetInt();
  }
  if (value.HasMember("avoid_waypoints")) {
    if (!value["avoid_waypoints"].IsArray()) return false;
    for (const auto& id : value["avoid_waypoints"].GetArray()) {
      if (!id.IsString()) return false;
      profile.avoid_waypoints.push_back(id.GetString());
    }
  }
  *out = std::move(profile);
  return true;
}

void WriteProfileJson(rapidjson::Writer<rapidjson::StringBuffer>& writer,
                      const Profile& profile) {
  writer.StartObject();
  writer.Key("name");
  writer.String(profile.name.c_str());
  if (profile.k.has_value()) {
    writer.Key("k");
    writer.Int(*profile.k);
  }
  if (profile.level.has_value()) {
    writer.Key("level");
    writer.String(profile.level->c_str());
  }
  if (profile.min_fl.has_value()) {
    writer.Key("min_fl");
    writer.Int(*profile.min_fl);
  }
  if (profile.max_fl.has_value()) {
    writer.Key("max_fl");
    writer.Int(*profile.max_fl);
  }
  if (!profile.avoid_waypoints.empty()) {
    writer.Key("avoid_waypoints");
    writer.StartArray();
    for (const auto& id : profile.avoid_waypoints) {
      writer.String(id.c_str());
    }
    writer.EndArray();
  }
  writer.EndObject();
}

std::vector<Profile> LoadProfiles(const std::string& file) {
  std::vector<Profile> out;
  std::ifstream in(file);
  if (!in) return out;  // 缺失 = 空档案
  const std::string content((std::istreambuf_iterator<char>(in)),
                            std::istreambuf_iterator<char>());
  rapidjson::Document doc;
  doc.Parse(content.c_str());
  if (doc.HasParseError() || !doc.IsArray()) {
    return out;  // 损坏文件 = 空档案（不崩溃；upsert 会重建）
  }
  out.reserve(doc.Size());
  for (const auto& value : doc.GetArray()) {
    Profile profile;
    if (ParseProfile(value, &profile)) {
      out.push_back(std::move(profile));
    }
  }
  return out;
}

Result<void> StoreProfiles(const std::string& file,
                           const std::vector<Profile>& profiles) {
  rapidjson::StringBuffer buffer;
  rapidjson::Writer<rapidjson::StringBuffer> writer(buffer);
  writer.StartArray();
  for (const auto& profile : profiles) {
    WriteProfileJson(writer, profile);
  }
  writer.EndArray();

  // 原子写：临时文件 + rename（POSIX 覆盖 / MSVC rename 前先删目标）。
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
