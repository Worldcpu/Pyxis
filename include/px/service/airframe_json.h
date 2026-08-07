// SPDX-License-Identifier: MIT
// airframe JSON 转换与 airframes.json 文件存取（决策 21/28：档案持久层
// 在 service 层——rapidjson 翻译；T6 校验链在域层）。
#pragma once

#include <rapidjson/document.h>
#include <rapidjson/stringbuffer.h>
#include <rapidjson/writer.h>

#include <string>
#include <vector>

#include "px/core/result.h"
#include "px/module/flightplan/airframe.h"

namespace px {

// JSON 对象 → Airframe（形状非法返回 false，不报字段——校验链在
// ValidateAirframe，字段错误消费端展示）。
bool ParseAirframe(const rapidjson::Value& value, Airframe* out);

// Airframe → JSON 对象（决策 21 字段集 + 决策 38 cruise_speed_kt）。
void WriteAirframeJson(rapidjson::Writer<rapidjson::StringBuffer>& writer,
                       const Airframe& airframe);

// 读 airframes.json（{airframes: [...]}）；文件缺失 → 空列表。
std::vector<Airframe> LoadAirframes(const std::string& file);

// 写 airframes.json（原子：先写临时文件再改名，避免半写损坏）。
Result<void> StoreAirframes(const std::string& file,
                            const std::vector<Airframe>& airframes);

}  // namespace px
