// SPDX-License-Identifier: MIT
#pragma once

#include <optional>
#include <string>
#include <vector>

namespace px {

// 性能来源（决策 6/21：引擎隐含模型的字段——kLnm/kCustom → 标准引擎，
// kOpenAp/kFcom → 实验性引擎）。
enum class PerfSource { kLnm, kOpenAp, kCustom, kFcom };

// 机型档案（决策 21：flightplan 域字段；燃油域字段由燃油引擎设计补充）。
struct Airframe {
  std::string type;     // ICAO 机型码
  std::string variant;  // 机架名（SimBrief 式）
  PerfSource perf_source = PerfSource::kLnm;
  double dow_kg = 0.0;              // 干使用重量
  double mzfw_kg = 0.0;             // 最大零油重量
  double mtow_kg = 0.0;             // 最大起飞重量
  double mlw_kg = 0.0;              // 最大着陆重量
  double service_ceiling_ft = 0.0;  // 升限（altitude 硬过滤）
  double unit_pax_kg = 0.0;         // 单位旅客重量（配载）
  double unit_bag_kg = 0.0;         // 单位行李重量（配载）
  // 巡航速度（决策 38：kt TAS 整数，Prefile FPL 编码用，与燃油引擎解耦；
  // 可选——未录则 Prefile 提示缺失）。
  std::optional<int> cruise_speed_kt;
};

// 校验问题（决策 28：物理不等式链 + 必填）。
struct AirframeIssue {
  std::string field;
  std::string message;
};

// upsert 校验链：DOW ≤ MZFW ≤ MTOW、MLW ≤ MTOW、单位重量 > 0、
// 升限 > 0、type/variant 非空；cruise_speed_kt 提供则 > 0。空 = 合法。
std::vector<AirframeIssue> ValidateAirframe(const Airframe& airframe);

}  // namespace px
