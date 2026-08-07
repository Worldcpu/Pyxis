// SPDX-License-Identifier: MIT
// profile JSON 转换与 profiles.json 文件存取（决策 55：Suggest Route 偏好
// 后端 profile——airframe 同款 Load/Store + 原子改名模式）。
#pragma once

#include <rapidjson/document.h>
#include <rapidjson/stringbuffer.h>
#include <rapidjson/writer.h>

#include <optional>
#include <string>
#include <vector>

#include "px/core/result.h"

namespace px {

// 偏好 profile（plan.routes 参数子集，决策 55）。
struct Profile {
  std::string name;
  std::optional<int> k;              // 1..15
  std::optional<std::string> level;  // "low" | "high"
  std::optional<int> min_fl;
  std::optional<int> max_fl;
  std::vector<std::string> avoid_waypoints;
};

// JSON 对象 → Profile（形状非法返回 false，不报字段——校验链在 handler）。
bool ParseProfile(const rapidjson::Value& value, Profile* out);

// Profile → JSON 对象（name/k/level/min_fl/max_fl/avoid_waypoints）。
void WriteProfileJson(rapidjson::Writer<rapidjson::StringBuffer>& writer,
                      const Profile& profile);

// 读 profiles.json（根数组）；文件缺失 → 空列表。
std::vector<Profile> LoadProfiles(const std::string& file);

// 写 profiles.json（原子：先写临时文件再改名，避免半写损坏）。
Result<void> StoreProfiles(const std::string& file,
                           const std::vector<Profile>& profiles);

}  // namespace px
