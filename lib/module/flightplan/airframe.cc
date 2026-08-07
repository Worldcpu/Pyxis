// SPDX-License-Identifier: MIT
// airframe 校验链（决策 28：物理不等式链 + 必填）。
#include "px/module/flightplan/airframe.h"

namespace px {

namespace {

void Check(bool condition, const char* field, const char* message,
           std::vector<AirframeIssue>* issues) {
  if (!condition) issues->push_back({field, message});
}

}  // namespace

std::vector<AirframeIssue> ValidateAirframe(const Airframe& airframe) {
  std::vector<AirframeIssue> issues;
  Check(!airframe.type.empty(), "type", "机型码必填", &issues);
  Check(!airframe.variant.empty(), "variant", "机架名必填", &issues);
  Check(airframe.dow_kg <= airframe.mzfw_kg, "dow_kg", "DOW 不得超过 MZFW",
        &issues);
  Check(airframe.mzfw_kg <= airframe.mtow_kg, "mzfw_kg", "MZFW 不得超过 MTOW",
        &issues);
  Check(airframe.mlw_kg <= airframe.mtow_kg, "mlw_kg", "MLW 不得超过 MTOW",
        &issues);
  Check(airframe.unit_pax_kg > 0.0, "unit_pax_kg", "单位旅客重量必须为正",
        &issues);
  Check(airframe.unit_bag_kg > 0.0, "unit_bag_kg", "单位行李重量必须为正",
        &issues);
  Check(airframe.service_ceiling_ft > 0.0, "service_ceiling_ft", "升限必须为正",
        &issues);
  // 决策 38：巡航速度可选；提供则必须为正（kt TAS）。
  if (airframe.cruise_speed_kt.has_value()) {
    Check(*airframe.cruise_speed_kt > 0, "cruise_speed_kt", "巡航速度必须为正",
          &issues);
  }
  return issues;
}

}  // namespace px
